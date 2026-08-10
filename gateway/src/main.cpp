#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "board.grpc.pb.h"
#include "content.grpc.pb.h"
#include "route.grpc.pb.h"
#include "weather.grpc.pb.h"
#include <grpcpp/grpcpp.h>

namespace {

constexpr int kDeadlineMs = 500;
constexpr int kTickSeconds = 2;
constexpr int kContentTtlSeconds = 30;
constexpr double kSimSpeedMs = 12.0;

std::string envOr(const char *key, const std::string &fallback) {
  const char *value = std::getenv(key);
  return value ? std::string(value) : fallback;
}

std::chrono::system_clock::time_point deadline() {
  return std::chrono::system_clock::now() + std::chrono::milliseconds(kDeadlineMs);
}

// --- server-side GPS simulator (dev) ---------------------------------------
// StreamBoard is server-streaming, so the client's position is fixed at
// subscribe time. To keep ETAs live the gateway walks the vehicle along the
// route itself and pushes fresh boards.
struct SimPoint {
  double lat;
  double lon;
};

const std::unordered_map<std::string, std::vector<SimPoint>> kSimRoutes = {
    {"42",
     {{55.7520, 37.6175}, {55.7558, 37.6220}, {55.7600, 37.6270}, {55.7650, 37.6320}}},
    {"7", {{55.7000, 37.6000}, {55.7050, 37.6080}, {55.7110, 37.6150}}},
};

double haversine(SimPoint a, SimPoint b) {
  constexpr double R = 6371000.0;
  const double dlat = (b.lat - a.lat) * M_PI / 180.0;
  const double dlon = (b.lon - a.lon) * M_PI / 180.0;
  const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(a.lat * M_PI / 180.0) * std::cos(b.lat * M_PI / 180.0) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * R * std::asin(std::min(1.0, std::sqrt(h)));
}

SimPoint pointAlong(const std::vector<SimPoint> &poly, double dist) {
  double total = 0.0;
  for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
    total += haversine(poly[i], poly[i + 1]);
  }
  if (total <= 0.0) {
    return poly.front();
  }
  dist = std::fmod(dist, total);
  for (std::size_t i = 0; i + 1 < poly.size(); ++i) {
    const double seg = haversine(poly[i], poly[i + 1]);
    if (dist <= seg) {
      const double t = seg > 0.0 ? dist / seg : 0.0;
      return {poly[i].lat + t * (poly[i + 1].lat - poly[i].lat),
              poly[i].lon + t * (poly[i + 1].lon - poly[i].lon)};
    }
    dist -= seg;
  }
  return poly.back();
}

void advancePosition(transport::board::BoardRequest *request, int tick) {
  auto it = kSimRoutes.find(request->device().route_id());
  if (it == kSimRoutes.end()) {
    return;
  }
  const SimPoint p = pointAlong(it->second, kSimSpeedMs * tick * kTickSeconds);
  auto *pos = request->mutable_device()->mutable_current_position();
  pos->set_latitude(p.lat);
  pos->set_longitude(p.lon);
}

} // namespace

class BoardServiceImpl final : public transport::board::BoardService::Service {
public:
  BoardServiceImpl(std::shared_ptr<grpc::Channel> content_channel,
                   std::shared_ptr<grpc::Channel> weather_channel,
                   std::shared_ptr<grpc::Channel> route_channel)
      : content_(transport::content::ContentService::NewStub(content_channel)),
        weather_(transport::weather::WeatherService::NewStub(weather_channel)),
        route_(transport::route::RouteService::NewStub(route_channel)) {}

private:
  ::grpc::Status GetBoard(::grpc::ServerContext *,
                          const ::transport::board::BoardRequest *request,
                          ::transport::board::Board *response) override {
    std::cerr << "GetBoard from device=" << request->device().device_id()
              << " route=" << request->device().route_id() << std::endl;
    buildBoard(*request, response);
    return ::grpc::Status::OK;
  }

  ::grpc::Status
  StreamBoard(::grpc::ServerContext *context,
              const ::transport::board::BoardRequest *request,
              ::grpc::ServerWriter<::transport::board::Board> *writer) override {
    std::cerr << "StreamBoard from device=" << request->device().device_id()
              << std::endl;

    transport::board::BoardRequest state = *request;

    // content is fetched as a batch and reused for a while; weather/route are
    // refreshed every tick (route so the ETA stays live)
    std::vector<transport::content::ContentItem> content;
    auto content_deadline = std::chrono::steady_clock::time_point::min();

    for (int tick = 0; !context->IsCancelled(); ++tick) {
      advancePosition(&state, tick);

      auto weather = std::async(std::launch::async,
                                [&] { return fetchWeather(state); });
      auto route = std::async(std::launch::async,
                              [&] { return fetchRoute(state); });

      if (std::chrono::steady_clock::now() >= content_deadline) {
        content = fetchContent(state);
        content_deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(kContentTtlSeconds);
      }

      transport::board::Board board;
      if (auto w = weather.get()) {
        *board.add_blocks()->mutable_weather() = std::move(*w);
      }
      if (auto r = route.get()) {
        *board.add_blocks()->mutable_route() = std::move(*r);
      }
      for (const auto &item : content) {
        *board.add_blocks()->mutable_content() = item;
      }
      board.mutable_server_time()->set_unix_seconds(
          static_cast<int64_t>(time(nullptr)));

      if (!writer->Write(board)) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::seconds(kTickSeconds));
    }
    return ::grpc::Status::OK;
  }

  // parallel fan-out: the three services are queried concurrently; each RPC
  // carries its own deadline, so a slow/dead service degrades that section
  // only (missing block) without stalling the others
  void buildBoard(const transport::board::BoardRequest &request,
                  transport::board::Board *board) {
    auto weather = std::async(std::launch::async,
                              [&] { return fetchWeather(request); });
    auto route = std::async(std::launch::async,
                            [&] { return fetchRoute(request); });
    auto content = std::async(std::launch::async,
                              [&] { return fetchContent(request); });

    if (auto w = weather.get()) {
      *board->add_blocks()->mutable_weather() = std::move(*w);
    }
    if (auto r = route.get()) {
      *board->add_blocks()->mutable_route() = std::move(*r);
    }
    for (auto &item : content.get()) {
      *board->add_blocks()->mutable_content() = std::move(item);
    }
    board->mutable_server_time()->set_unix_seconds(
        static_cast<int64_t>(time(nullptr)));
  }

  std::optional<transport::weather::WeatherData>
  fetchWeather(const transport::board::BoardRequest &request) {
    transport::weather::WeatherRequest req;
    *req.mutable_location() = request.device().current_position();

    ::grpc::ClientContext ctx;
    ctx.set_deadline(deadline());
    transport::weather::WeatherData resp;
    const ::grpc::Status status = weather_->GetWeather(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "weather unavailable: " << status.error_message() << std::endl;
      return std::nullopt;
    }
    return resp;
  }

  std::optional<transport::route::RouteProgress>
  fetchRoute(const transport::board::BoardRequest &request) {
    transport::route::RouteRequest req;
    *req.mutable_device_info() = request.device();

    ::grpc::ClientContext ctx;
    ctx.set_deadline(deadline());
    transport::route::RouteProgress resp;
    const ::grpc::Status status = route_->GetArrivals(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "route unavailable: " << status.error_message() << std::endl;
      return std::nullopt;
    }
    return resp;
  }

  std::vector<transport::content::ContentItem>
  fetchContent(const transport::board::BoardRequest &request) {
    transport::content::ContentRequest req;
    req.set_device_id(request.device().device_id());
    req.set_route_id(request.device().route_id());
    req.set_max_items(10);

    ::grpc::ClientContext ctx;
    ctx.set_deadline(deadline());
    transport::content::ContentResponse resp;
    const ::grpc::Status status = content_->GetContent(&ctx, req, &resp);
    if (!status.ok()) {
      std::cerr << "content unavailable: " << status.error_message() << std::endl;
      return {};
    }
    return {resp.items().begin(), resp.items().end()};
  }

  std::unique_ptr<transport::content::ContentService::Stub> content_;
  std::unique_ptr<transport::weather::WeatherService::Stub> weather_;
  std::unique_ptr<transport::route::RouteService::Stub> route_;
};

int main() {
  const std::string address = envOr("GATEWAY_LISTEN", "0.0.0.0:50051");
  const std::string content_addr = envOr("CONTENT_ADDR", "localhost:50052");
  const std::string weather_addr = envOr("WEATHER_ADDR", "localhost:50053");
  const std::string route_addr = envOr("ROUTE_ADDR", "localhost:50054");

  auto content_channel =
      grpc::CreateChannel(content_addr, grpc::InsecureChannelCredentials());
  auto weather_channel =
      grpc::CreateChannel(weather_addr, grpc::InsecureChannelCredentials());
  auto route_channel =
      grpc::CreateChannel(route_addr, grpc::InsecureChannelCredentials());
  BoardServiceImpl service(content_channel, weather_channel, route_channel);

  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  std::cerr << "Gateway listening on " << address << " (content=" << content_addr
            << " weather=" << weather_addr << " route=" << route_addr << ")"
            << std::endl;
  server->Wait();
}
