#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "weather.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

namespace {

std::string envOr(const char *key, const std::string &fallback) {
  const char *value = std::getenv(key);
  return value ? std::string(value) : fallback;
}

double envDouble(const char *key, double fallback) {
  const char *value = std::getenv(key);
  return value ? std::strtod(value, nullptr) : fallback;
}

// WMO weather codes -> human text
std::string weatherCodeText(int code) {
  switch (code) {
  case 0:
    return "Ясно";
  case 1:
  case 2:
    return "Переменная облачность";
  case 3:
    return "Пасмурно";
  case 45:
  case 48:
    return "Туман";
  case 51:
  case 53:
  case 55:
    return "Морось";
  case 61:
  case 63:
  case 65:
    return "Дождь";
  case 71:
  case 73:
  case 75:
    return "Снег";
  case 80:
  case 81:
  case 82:
    return "Ливень";
  case 95:
  case 96:
  case 99:
    return "Гроза";
  default:
    return "—";
  }
}

std::optional<transport::weather::WeatherData> fetchWeather(double lat,
                                                            double lon) {
  const cpr::Response r = cpr::Get(
      cpr::Url{"https://api.open-meteo.com/v1/forecast"},
      cpr::Parameters{
          {"latitude", std::to_string(lat)},
          {"longitude", std::to_string(lon)},
          {"current", "temperature_2m,weather_code,wind_speed_10m"},
      },
      cpr::Timeout{5000});

  if (r.status_code != 200) {
    std::cerr << "open-meteo http " << r.status_code << ": " << r.error.message
              << std::endl;
    return std::nullopt;
  }

  try {
    const auto json = nlohmann::json::parse(r.text);
    const auto &cur = json.at("current");

    transport::weather::WeatherData data;
    data.set_temperature_celsius(cur.at("temperature_2m").get<float>());
    data.set_wind_speed_ms(cur.at("wind_speed_10m").get<float>());
    data.set_condition(weatherCodeText(cur.at("weather_code").get<int>()));
    data.mutable_measured_at()->set_unix_seconds(
        static_cast<int64_t>(std::time(nullptr)));
    data.set_stale(false);
    return data;
  } catch (const std::exception &e) {
    std::cerr << "open-meteo parse error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

// reader-writer cache: gRPC handlers read, the background thread writes
class WeatherCache {
public:
  std::optional<transport::weather::WeatherData> get() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!has_data_) {
      return std::nullopt;
    }
    return data_;
  }

  bool hasData() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return has_data_;
  }

  void update(transport::weather::WeatherData data) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_ = std::move(data);
    has_data_ = true;
  }

  // API down -> keep the last value but flag it as stale
  void markStale() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (has_data_) {
      data_.set_stale(true);
    }
  }

private:
  mutable std::shared_mutex mutex_;
  transport::weather::WeatherData data_;
  bool has_data_ = false;
};

// background updater: jthread + stop_token for graceful shutdown
class WeatherUpdater {
public:
  WeatherUpdater(WeatherCache &cache, double lat, double lon,
                 std::chrono::seconds interval)
      : cache_(cache), lat_(lat), lon_(lon), interval_(interval) {
    refreshOnce(); // warm start before serving
    thread_ = std::jthread([this](std::stop_token st) { run(std::move(st)); });
  }

private:
  void refreshOnce() {
    if (auto data = fetchWeather(lat_, lon_)) {
      cache_.update(std::move(*data));
    } else {
      cache_.markStale();
    }
  }

  void run(std::stop_token st) {
    std::stop_callback wake(st, [this] { cv_.notify_all(); });
    while (!st.stop_requested()) {
      const auto wait = cache_.hasData() ? interval_ : kRetry;
      std::unique_lock<std::mutex> lock(sleep_mutex_);
      cv_.wait_for(lock, wait, [&] { return st.stop_requested(); });
      lock.unlock();
      if (st.stop_requested()) {
        break;
      }
      refreshOnce();
    }
  }

  static constexpr std::chrono::seconds kRetry{15};

  WeatherCache &cache_;
  double lat_;
  double lon_;
  std::chrono::seconds interval_;
  std::mutex sleep_mutex_;
  std::condition_variable cv_;
  std::jthread thread_;
};

class WeatherServiceImpl final
    : public transport::weather::WeatherService::Service {
public:
  explicit WeatherServiceImpl(WeatherCache *cache) : cache_(cache) {}

  ::grpc::Status GetWeather(::grpc::ServerContext *,
                            const ::transport::weather::WeatherRequest *,
                            ::transport::weather::WeatherData *response) override {
    auto data = cache_->get();
    if (!data) {
      return {::grpc::StatusCode::UNAVAILABLE, "weather not ready"};
    }
    *response = std::move(*data);
    return ::grpc::Status::OK;
  }

private:
  WeatherCache *cache_;
};

} // namespace

int main() {
  const std::string address = envOr("WEATHER_ADDR", "0.0.0.0:50053");
  const double lat = envDouble("WEATHER_LAT", 55.751244);
  const double lon = envDouble("WEATHER_LON", 37.618423);
  const auto interval =
      std::chrono::seconds(std::strtol(envOr("WEATHER_INTERVAL", "600").c_str(),
                                       nullptr, 10));

  WeatherCache cache;
  WeatherUpdater updater(cache, lat, lon, interval);
  WeatherServiceImpl service(&cache);

  ::grpc::EnableDefaultHealthCheckService(true);
  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  std::cerr << "Weather service listening on " << address << " (lat=" << lat
            << " lon=" << lon << ")" << std::endl;
  server->Wait();
}
