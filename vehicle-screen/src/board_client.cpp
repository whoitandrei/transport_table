#include "board_client.h"

#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <QtGrpc/QGrpcHttp2Channel>
#include <QtGrpc/QGrpcServerStream>
#include <QtGrpc/QGrpcStatus>

namespace {

constexpr auto kDefaultGatewayUri = "http://localhost:50051";
constexpr int kReconnectMs = 2000;

QString formatEta(int seconds) {
    if (seconds <= 0) {
        return QStringLiteral("прибывает");
    }
    const int minutes = seconds / 60;
    const int rest = seconds % 60;
    if (minutes == 0) {
        return QStringLiteral("%1 сек").arg(rest);
    }
    return QStringLiteral("%1 мин %2 сек").arg(minutes).arg(rest);
}

QString contentTypeLabel(transport::content::ContentTypeGadget::ContentType type) {
    using CT = transport::content::ContentTypeGadget::ContentType;
    switch (type) {
    case CT::CONTENT_TYPE_AD:
        return QStringLiteral("Реклама");
    case CT::CONTENT_TYPE_NEWS:
        return QStringLiteral("Новость");
    case CT::CONTENT_TYPE_FACT:
        return QStringLiteral("Факт");
    default:
        return QStringLiteral("Инфо");
    }
}

} // namespace

BoardClient::BoardClient(QObject *parent) : QObject(parent) {
    const QString uri = qEnvironmentVariable("GATEWAY_ADDR", kDefaultGatewayUri);

    channel_ = std::make_shared<QGrpcHttp2Channel>(QUrl(uri));
    client_ = std::make_unique<transport::board::BoardService::Client>();
    client_->attachChannel(channel_);

    setStatus(QStringLiteral("Соединение…"));
}

BoardClient::~BoardClient() = default;

transport::board::BoardRequest BoardClient::buildRequest() const {
    // start position; the gateway advances it along the route during the stream
    transport::common::GeoPoint position;
    position.setLatitude(55.7520);
    position.setLongitude(37.6175);

    transport::common::DeviceInfo device;
    device.setDeviceId(QStringLiteral("vehicle-001"));
    device.setDeviceType(
        transport::common::DeviceTypeGadget::DeviceType::DEVICE_TYPE_VEHICLE);
    device.setRouteId(QStringLiteral("42"));
    device.setCurrentPosition(position);

    transport::board::BoardRequest request;
    request.setDevice(device);
    return request;
}

void BoardClient::start() {
    stream_ = client_->StreamBoard(buildRequest());

    connect(stream_.get(), &QGrpcServerStream::messageReceived, this, [this] {
        if (auto board = stream_->read<transport::board::Board>()) {
            applyBoard(*board);
            setStatus(QStringLiteral("Подключено (стрим)"));
        }
    });

    connect(stream_.get(), &QGrpcServerStream::finished, this,
            [this](const QGrpcStatus &status) {
                setStatus(status.isOk()
                              ? QStringLiteral("Стрим завершён, переподключение…")
                              : QStringLiteral("Обрыв стрима: %1, переподключение…")
                                    .arg(status.message()));
                // resubscribe after a short delay
                QTimer::singleShot(kReconnectMs, this, [this] { start(); });
            });
}

void BoardClient::applyBoard(const transport::board::Board &board) {
    bool weatherFound = false;
    bool routeFound = false;
    QVariantList stops;
    QVariantList content;

    for (const transport::board::BoardBlock &block : board.blocks()) {
        if (block.hasWeather()) {
            const transport::weather::WeatherData &w = block.weather();
            weatherTemperature_ =
                QStringLiteral("%1 °C").arg(w.temperatureCelsius(), 0, 'f', 1);
            weatherCondition_ = w.condition();
            weatherWind_ =
                QStringLiteral("%1 м/с").arg(w.windSpeedMs(), 0, 'f', 1);
            weatherStale_ = w.stale();
            weatherFound = true;
        } else if (block.hasRoute()) {
            const transport::route::RouteProgress &r = block.route();
            routeName_ = r.routeName();
            routeFinalStop_ = r.finalStop();
            for (const transport::route::StopArrival &s : r.upcomingStops()) {
                QVariantMap stop;
                stop[QStringLiteral("stopName")] = s.stopName();
                stop[QStringLiteral("eta")] = formatEta(s.etaSeconds());
                stop[QStringLiteral("isNext")] = s.isNext();
                stops.append(stop);
            }
            routeFound = true;
        } else if (block.hasContent()) {
            const transport::content::ContentItem &c = block.content();
            QVariantMap item;
            item[QStringLiteral("title")] = c.title();
            item[QStringLiteral("body")] = c.body();
            item[QStringLiteral("type")] = contentTypeLabel(c.type());
            item[QStringLiteral("displaySeconds")] = static_cast<int>(c.displaySeconds());
            content.append(item);
        }
    }

    weatherValid_ = weatherFound;
    routeValid_ = routeFound;
    routeStops_ = stops;
    contentItems_ = content;

    emit boardChanged();
}

void BoardClient::setStatus(const QString &status) {
    if (status_ != status) {
        status_ = status;
        emit statusChanged();
    }
}
