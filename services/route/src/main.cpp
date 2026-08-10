#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "route.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

namespace {

constexpr double kEarthRadiusM = 6371000.0;

std::string envOr(const char *key, const std::string &fallback) {
  const char *value = std::getenv(key);
  return value ? std::string(value) : fallback;
}

double deg2rad(double d) { return d * M_PI / 180.0; }

// great-circle distance in meters (haversine)
double haversine(double lat1, double lon1, double lat2, double lon2) {
  const double dlat = deg2rad(lat2 - lat1);
  const double dlon = deg2rad(lon2 - lon1);
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(deg2rad(lat1)) * std::cos(deg2rad(lat2)) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  return 2 * kEarthRadiusM * std::asin(std::min(1.0, std::sqrt(a)));
}

struct Stop {
  std::string name;
  double lat;
  double lon;
};

struct Route {
  std::string name;
  std::vector<Stop> stops;
};

// read-heavy cache of route descriptions under a shared_mutex
class RouteCache {
public:
  RouteCache() { seed(); }

  std::optional<Route> get(const std::string &route_id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = routes_.find(route_id);
    if (it == routes_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

private:
  // seeded from config; could later be loaded from a DB under the write lock
  void seed() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    routes_["42"] = Route{
        "Маршрут 42",
        {
            {"ул. Ленина", 55.7520, 37.6175},
            {"Парк Победы", 55.7558, 37.6220},
            {"Центральный рынок", 55.7600, 37.6270},
            {"Северный вокзал", 55.7650, 37.6320},
        }};
    routes_["7"] = Route{
        "Маршрут 7",
        {
            {"Южные ворота", 55.7000, 37.6000},
            {"Университет", 55.7050, 37.6080},
            {"Стадион", 55.7110, 37.6150},
        }};
  }

  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, Route> routes_;
};

// planar offset (meters) of (lat,lon) from origin — equirectangular, fine at
// city scale; used only to project the vehicle onto a route segment
void toXY(double lat, double lon, double lat0, double lon0, double &x,
         double &y) {
  x = deg2rad(lon - lon0) * std::cos(deg2rad(lat0)) * kEarthRadiusM;
  y = deg2rad(lat - lat0) * kEarthRadiusM;
}

class RouteServiceImpl final : public transport::route::RouteService::Service {
public:
  RouteServiceImpl(RouteCache *cache, double speed_ms)
      : cache_(cache), speed_ms_(speed_ms) {}

  ::grpc::Status
  GetArrivals(::grpc::ServerContext *,
              const ::transport::route::RouteRequest *request,
              ::transport::route::RouteProgress *response) override {
    const auto &device = request->device_info();
    const std::optional<Route> route = cache_->get(device.route_id());
    if (!route) {
      return {::grpc::StatusCode::NOT_FOUND, "unknown route"};
    }

    const auto &stops = route->stops;
    if (stops.size() < 2) {
      return {::grpc::StatusCode::FAILED_PRECONDITION, "route too short"};
    }

    // cumulative distance along the route to each stop
    std::vector<double> cum(stops.size(), 0.0);
    for (std::size_t i = 1; i < stops.size(); ++i) {
      cum[i] = cum[i - 1] + haversine(stops[i - 1].lat, stops[i - 1].lon,
                                      stops[i].lat, stops[i].lon);
    }

    const double lat = device.current_position().latitude();
    const double lon = device.current_position().longitude();
    const double progress = progressAlong(stops, cum, lat, lon);

    response->set_route_name(route->name);
    response->set_final_stop(stops.back().name);
    response->mutable_calculated_at()->set_unix_seconds(
        static_cast<int64_t>(std::time(nullptr)));

    bool next_marked = false;
    for (std::size_t i = 0; i < stops.size(); ++i) {
      const double remaining = cum[i] - progress;
      if (remaining <= 0.0) {
        continue; // already passed
      }
      auto *arrival = response->add_upcoming_stops();
      arrival->set_stop_name(stops[i].name);
      arrival->set_eta_seconds(static_cast<int32_t>(remaining / speed_ms_));
      arrival->set_is_next(!next_marked);
      next_marked = true;
    }
    return ::grpc::Status::OK;
  }

private:
  // distance travelled along the route, by projecting onto the nearest segment
  double progressAlong(const std::vector<Stop> &stops,
                       const std::vector<double> &cum, double lat,
                       double lon) const {
    const double lat0 = stops.front().lat;
    const double lon0 = stops.front().lon;
    double px, py;
    toXY(lat, lon, lat0, lon0, px, py);

    double best_perp = std::numeric_limits<double>::max();
    double progress = 0.0;
    for (std::size_t i = 0; i + 1 < stops.size(); ++i) {
      double ax, ay, bx, by;
      toXY(stops[i].lat, stops[i].lon, lat0, lon0, ax, ay);
      toXY(stops[i + 1].lat, stops[i + 1].lon, lat0, lon0, bx, by);

      const double abx = bx - ax;
      const double aby = by - ay;
      const double seg_sq = abx * abx + aby * aby;
      double t = 0.0;
      if (seg_sq > 0.0) {
        t = ((px - ax) * abx + (py - ay) * aby) / seg_sq;
        t = std::clamp(t, 0.0, 1.0);
      }
      const double projx = ax + t * abx;
      const double projy = ay + t * aby;
      const double perp = std::hypot(px - projx, py - projy);
      if (perp < best_perp) {
        best_perp = perp;
        progress = cum[i] + t * (cum[i + 1] - cum[i]);
      }
    }
    return progress;
  }

  RouteCache *cache_;
  double speed_ms_;
};

} // namespace

int main() {
  const std::string address = envOr("ROUTE_ADDR", "0.0.0.0:50054");
  const double speed_ms = std::strtod(envOr("ROUTE_SPEED_MS", "8.33").c_str(),
                                      nullptr);

  RouteCache cache;
  RouteServiceImpl service(&cache, speed_ms);

  ::grpc::EnableDefaultHealthCheckService(true);
  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  std::cerr << "Route service listening on " << address
            << " (speed=" << speed_ms << " m/s)" << std::endl;
  server->Wait();
}
