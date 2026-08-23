#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <cpr/cpr.h>
#include <pqxx/pqxx>
#include <pugixml.hpp>

#include "content.grpc.pb.h"
#include <grpcpp/ext/health_check_service_server_builder_option.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

namespace {

std::string envOr(const char *key, const std::string &fallback) {
  const char *value = std::getenv(key);
  return value ? std::string(value) : fallback;
}

// thread-safe pool: libpqxx connections are not shareable across threads
class ConnectionPool {
public:
  ConnectionPool(const std::string &dsn, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
      conns_.push_back(std::make_unique<pqxx::connection>(dsn));
    }
  }

  class Handle {
  public:
    Handle(ConnectionPool *pool, std::unique_ptr<pqxx::connection> conn)
        : pool_(pool), conn_(std::move(conn)) {}
    Handle(Handle &&) = default;
    Handle &operator=(Handle &&) = default;
    ~Handle() {
      if (conn_) {
        pool_->release(std::move(conn_));
      }
    }
    pqxx::connection &operator*() { return *conn_; }
    pqxx::connection *operator->() { return conn_.get(); }

  private:
    ConnectionPool *pool_;
    std::unique_ptr<pqxx::connection> conn_;
  };

  Handle acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !conns_.empty(); });
    auto conn = std::move(conns_.back());
    conns_.pop_back();
    return Handle(this, std::move(conn));
  }

private:
  friend class Handle;
  void release(std::unique_ptr<pqxx::connection> conn) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      conns_.push_back(std::move(conn));
    }
    cv_.notify_one();
  }

  std::vector<std::unique_ptr<pqxx::connection>> conns_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

class ContentStore {
public:
  ContentStore(const std::string &dsn, std::size_t poolSize)
      : pool_(dsn, poolSize) {
    initSchema();
    seedIfEmpty();
  }

  std::vector<transport::content::ContentItem>
  fetch(const std::string &route_id, int max_items) {
    if (max_items <= 0) {
      max_items = 10;
    }
    const int64_t now = static_cast<int64_t>(std::time(nullptr));

    auto conn = pool_.acquire();
    pqxx::work txn(*conn);
    const pqxx::result rows = txn.exec(
        "SELECT id, type, title, body, display_seconds "
        "FROM content_items "
        "WHERE active = TRUE "
        "  AND (starts_at IS NULL OR starts_at <= $1) "
        "  AND (ends_at IS NULL OR ends_at >= $1) "
        "  AND (target = '' OR target = $2) "
        "ORDER BY priority DESC, id ASC "
        "LIMIT $3",
        pqxx::params{now, route_id, max_items});
    txn.commit();

    std::vector<transport::content::ContentItem> items;
    items.reserve(rows.size());
    for (const auto &row : rows) {
      transport::content::ContentItem item;
      item.set_id(row["id"].as<std::string>());
      item.set_type(static_cast<transport::content::ContentType>(
          row["type"].as<int>()));
      item.set_title(row["title"].as<std::string>());
      item.set_body(row["body"].as<std::string>());
      item.set_display_seconds(row["display_seconds"].as<int>());
      items.push_back(std::move(item));
    }
    return items;
  }

  // insert a news item; dedup by id (the article link) at the DB level
  void upsertNews(const std::string &id, const std::string &title,
                  const std::string &body) {
    auto conn = pool_.acquire();
    pqxx::work txn(*conn);
    txn.exec("INSERT INTO content_items (id, type, title, body, priority, display_seconds) "
             "VALUES ($1, 2, $2, $3, 25, 12) ON CONFLICT (id) DO NOTHING",
             pqxx::params{id, title, body});
    txn.commit();
  }

private:
  void initSchema() {
    auto conn = pool_.acquire();
    pqxx::work txn(*conn);
    txn.exec(
        "CREATE TABLE IF NOT EXISTS content_items ("
        "  id              TEXT PRIMARY KEY,"
        "  type            INTEGER NOT NULL,"
        "  title           TEXT NOT NULL,"
        "  body            TEXT NOT NULL,"
        "  image_url       TEXT NOT NULL DEFAULT '',"
        "  priority        INTEGER NOT NULL DEFAULT 0,"
        "  starts_at       BIGINT,"
        "  ends_at         BIGINT,"
        "  active          BOOLEAN NOT NULL DEFAULT TRUE,"
        "  target          TEXT NOT NULL DEFAULT '',"
        "  display_seconds INTEGER NOT NULL DEFAULT 10"
        ")");
    txn.commit();
  }

  void seedIfEmpty() {
    auto conn = pool_.acquire();
    pqxx::work txn(*conn);
    const int count = txn.query_value<int>("SELECT COUNT(*) FROM content_items");
    if (count > 0) {
      return;
    }
    txn.exec(
        "INSERT INTO content_items (id, type, title, body, priority, display_seconds) VALUES "
        "('news-1', 2, 'Городские новости', "
        "  'Открыта новая трамвайная ветка в северном районе города.', 30, 12),"
        "('fact-1', 3, 'А вы знали?', "
        "  'Первый электрический трамвай в России пустили в 1892 году.', 20, 10),"
        "('fact-2', 3, 'Факт дня', "
        "  'Самая длинная трамвайная линия в мире — Бельгийская береговая, 68 км.', 10, 10),"
        "('ad-1', 1, 'Реклама', "
        "  'Оплачивайте проезд картой — быстрее и удобнее.', 5, 8)");
    txn.commit();
    std::cerr << "content_items seeded" << std::endl;
  }

  ConnectionPool pool_;
};

struct NewsItem {
  std::string id; // article link, used for dedup
  std::string title;
  std::string summary; // RSS summary, fallback if the article can't be fetched
};

// bounded blocking queue with backpressure (condition_variable)
template <class T> class BoundedQueue {
public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

  bool push(T value, std::stop_token st) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [&] {
      return queue_.size() < capacity_ || closed_ || st.stop_requested();
    });
    if (closed_ || st.stop_requested()) {
      return false;
    }
    queue_.push(std::move(value));
    not_empty_.notify_one();
    return true;
  }

  std::optional<T> pop(std::stop_token st) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [&] {
      return !queue_.empty() || closed_ || st.stop_requested();
    });
    if (queue_.empty()) {
      return std::nullopt;
    }
    T value = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    return value;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    not_full_.notify_all();
    not_empty_.notify_all();
  }

private:
  std::size_t capacity_;
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
  bool closed_ = false;
};

std::vector<std::string> splitCsv(const std::string &s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      out.push_back(item);
    }
  }
  return out;
}

// truncate on a UTF-8 char boundary (Postgres rejects split byte sequences)
std::string truncateUtf8(std::string s, std::size_t max_bytes) {
  if (s.size() <= max_bytes) {
    return s;
  }
  s.resize(max_bytes);
  while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) {
    s.pop_back(); // drop trailing continuation bytes
  }
  if (!s.empty() && static_cast<unsigned char>(s.back()) >= 0xC0) {
    s.pop_back(); // drop a now-incomplete lead byte
  }
  s += "…";
  return s;
}

// strip HTML tags, decode a few common entities, collapse whitespace
std::string stripHtml(const std::string &in) {
  std::string text;
  text.reserve(in.size());
  bool in_tag = false;
  for (char c : in) {
    if (c == '<') {
      in_tag = true;
    } else if (c == '>') {
      in_tag = false;
    } else if (!in_tag) {
      text += c;
    }
  }

  const std::pair<std::string, std::string> entities[] = {
      {"&nbsp;", " "},  {"&mdash;", "—"}, {"&ndash;", "–"}, {"&laquo;", "«"},
      {"&raquo;", "»"}, {"&hellip;", "…"}, {"&quot;", "\""}, {"&#39;", "'"},
      {"&apos;", "'"},  {"&amp;", "&"},
  };
  for (const auto &[from, to] : entities) {
    for (std::size_t p = text.find(from); p != std::string::npos;
         p = text.find(from, p + to.size())) {
      text.replace(p, from.size(), to);
    }
  }

  // collapse runs of whitespace, trim
  std::string out;
  out.reserve(text.size());
  bool space = false;
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      space = true;
    } else {
      if (space && !out.empty()) {
        out += ' ';
      }
      space = false;
      out += c;
    }
  }
  return out;
}

// extract readable article text: keep paragraphs that look like prose, drop
// nav/boilerplate (very low space ratio)
std::string extractArticle(const std::string &html) {
  std::string out;
  std::size_t pos = 0;
  while ((pos = html.find("<p", pos)) != std::string::npos && out.size() < 3000) {
    const std::size_t open_end = html.find('>', pos);
    if (open_end == std::string::npos) {
      break;
    }
    const std::size_t close = html.find("</p", open_end);
    if (close == std::string::npos) {
      break;
    }
    const std::string para =
        stripHtml(html.substr(open_end + 1, close - open_end - 1));
    pos = close + 3;

    if (para.size() < 60) {
      continue;
    }
    const std::size_t spaces = std::count(para.begin(), para.end(), ' ');
    if (spaces * 20 < para.size()) {
      continue; // menus/boilerplate have almost no spaces
    }
    if (!out.empty()) {
      out += "\n\n";
    }
    out += para;
  }
  return out;
}

// fetch the article page and pull out its body text
std::string fetchArticle(const std::string &url) {
  const cpr::Response r =
      cpr::Get(cpr::Url{url}, cpr::Timeout{6000},
               cpr::Header{{"User-Agent", "transport-board/1.0"}});
  if (r.status_code != 200) {
    return "";
  }
  return truncateUtf8(extractArticle(r.text), 2500);
}

std::vector<NewsItem> fetchFeed(const std::string &url) {
  std::vector<NewsItem> items;
  const cpr::Response r =
      cpr::Get(cpr::Url{url}, cpr::Timeout{5000},
               cpr::Header{{"User-Agent", "transport-board/1.0"}});
  if (r.status_code != 200) {
    std::cerr << "rss http " << r.status_code << " for " << url << std::endl;
    return items;
  }

  pugi::xml_document doc;
  const pugi::xml_parse_result parsed = doc.load_string(r.text.c_str());
  if (!parsed) {
    std::cerr << "rss parse error: " << parsed.description() << std::endl;
    return items;
  }

  const pugi::xml_node channel = doc.child("rss").child("channel");
  for (pugi::xml_node item = channel.child("item"); item;
       item = item.next_sibling("item")) {
    NewsItem ni;
    ni.title = item.child_value("title");
    ni.id = item.child_value("link");
    ni.summary = truncateUtf8(stripHtml(item.child_value("description")), 500);
    if (ni.id.empty()) {
      ni.id = ni.title;
    }
    if (!ni.title.empty()) {
      items.push_back(std::move(ni));
    }
  }
  return items;
}

void interruptibleSleep(std::stop_token st, std::chrono::seconds duration) {
  std::mutex m;
  std::condition_variable cv;
  std::stop_callback wake(st, [&] {
    std::lock_guard<std::mutex> lock(m);
    cv.notify_all();
  });
  std::unique_lock<std::mutex> lock(m);
  cv.wait_for(lock, duration, [&] { return st.stop_requested(); });
}

// producers pull RSS feeds into a bounded queue; one consumer dedups by link
// and writes news into content_items
class NewsIngestor {
public:
  NewsIngestor(ContentStore &store, std::vector<std::string> feeds,
               std::chrono::seconds interval)
      : store_(store), feeds_(std::move(feeds)), interval_(interval),
        queue_(64) {
    if (feeds_.empty()) {
      return;
    }
    consumer_ = std::jthread([this](std::stop_token st) { consume(std::move(st)); });
    for (const auto &url : feeds_) {
      producers_.emplace_back(
          [this, url](std::stop_token st) { produce(std::move(st), url); });
    }
    std::cerr << "news ingestor started for " << feeds_.size() << " feed(s)"
              << std::endl;
  }

  ~NewsIngestor() {
    for (auto &p : producers_) {
      p.request_stop();
    }
    consumer_.request_stop();
    queue_.close(); // unblock threads so their jthread dtors can join
  }

private:
  void produce(std::stop_token st, std::string url) {
    while (!st.stop_requested()) {
      for (auto &item : fetchFeed(url)) {
        if (!queue_.push(std::move(item), st)) {
          return;
        }
      }
      interruptibleSleep(st, interval_);
    }
  }

  void consume(std::stop_token st) {
    while (auto item = queue_.pop(st)) {
      if (!seen_.insert(item->id).second) {
        continue; // already ingested this link
      }
      // enrich with the full article body; fall back to the RSS summary
      std::string body = fetchArticle(item->id);
      if (body.size() < 120) {
        body = item->summary;
      }
      try {
        store_.upsertNews(item->id, item->title, body);
      } catch (const std::exception &e) {
        std::cerr << "news write failed: " << e.what() << std::endl;
      }
    }
  }

  ContentStore &store_;
  std::vector<std::string> feeds_;
  std::chrono::seconds interval_;
  std::unordered_set<std::string> seen_; // consumer-only, no lock needed
  BoundedQueue<NewsItem> queue_;
  std::vector<std::jthread> producers_;
  std::jthread consumer_;
};

class ContentServiceImpl final
    : public transport::content::ContentService::Service {
public:
  explicit ContentServiceImpl(ContentStore *store) : store_(store) {}

  ::grpc::Status
  GetContent(::grpc::ServerContext *,
             const ::transport::content::ContentRequest *request,
             ::transport::content::ContentResponse *response) override {
    try {
      auto items = store_->fetch(request->route_id(), request->max_items());
      for (auto &item : items) {
        *response->add_items() = std::move(item);
      }
      response->set_ttl_seconds(30);
      return ::grpc::Status::OK;
    } catch (const std::exception &e) {
      std::cerr << "GetContent failed: " << e.what() << std::endl;
      return {::grpc::StatusCode::INTERNAL, e.what()};
    }
  }

private:
  ContentStore *store_;
};

} // namespace

int main() {
  const std::string dsn = envOr("CONTENT_DB_DSN", "dbname=transport");
  const std::string address = envOr("CONTENT_ADDR", "0.0.0.0:50052");

  std::unique_ptr<ContentStore> store;
  try {
    store = std::make_unique<ContentStore>(dsn, /*poolSize=*/4);
  } catch (const std::exception &e) {
    std::cerr << "cannot open database (" << dsn << "): " << e.what()
              << std::endl;
    return 1;
  }

  const auto news_interval = std::chrono::seconds(
      std::strtol(envOr("NEWS_INTERVAL", "300").c_str(), nullptr, 10));
  NewsIngestor ingestor(*store, splitCsv(envOr("NEWS_FEEDS", "https://lenta.ru/rss/news")),
                        news_interval);

  ContentServiceImpl service(store.get());

  ::grpc::EnableDefaultHealthCheckService(true);
  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  std::cerr << "Content service listening on " << address << std::endl;
  server->Wait();
}
