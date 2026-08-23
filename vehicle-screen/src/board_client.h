#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>

#include "board.qpb.h"
#include "board_client.grpc.qpb.h"

class QGrpcHttp2Channel;
class QGrpcServerStream;

class BoardClient : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

    Q_PROPERTY(bool weatherValid READ weatherValid NOTIFY boardChanged)
    Q_PROPERTY(QString weatherTemperature READ weatherTemperature NOTIFY boardChanged)
    Q_PROPERTY(QString weatherCondition READ weatherCondition NOTIFY boardChanged)
    Q_PROPERTY(QString weatherWind READ weatherWind NOTIFY boardChanged)
    Q_PROPERTY(bool weatherStale READ weatherStale NOTIFY boardChanged)

    Q_PROPERTY(bool routeValid READ routeValid NOTIFY boardChanged)
    Q_PROPERTY(QString routeName READ routeName NOTIFY boardChanged)
    Q_PROPERTY(QString routeFinalStop READ routeFinalStop NOTIFY boardChanged)
    Q_PROPERTY(QVariantList routeStops READ routeStops NOTIFY boardChanged)

    Q_PROPERTY(QVariantList contentItems READ contentItems NOTIFY boardChanged)

public:
    explicit BoardClient(QObject *parent = nullptr);
    ~BoardClient() override;

    QString status() const { return status_; }

    bool weatherValid() const { return weatherValid_; }
    QString weatherTemperature() const { return weatherTemperature_; }
    QString weatherCondition() const { return weatherCondition_; }
    QString weatherWind() const { return weatherWind_; }
    bool weatherStale() const { return weatherStale_; }

    bool routeValid() const { return routeValid_; }
    QString routeName() const { return routeName_; }
    QString routeFinalStop() const { return routeFinalStop_; }
    QVariantList routeStops() const { return routeStops_; }

    QVariantList contentItems() const { return contentItems_; }

    // subscribes to the gateway's server stream (polling removed)
    Q_INVOKABLE void start();

signals:
    void statusChanged();
    void boardChanged();

private:
    void applyBoard(const transport::board::Board &board);
    void setStatus(const QString &status);
    transport::board::BoardRequest buildRequest() const;

    std::shared_ptr<QGrpcHttp2Channel> channel_;
    std::unique_ptr<transport::board::BoardService::Client> client_;
    std::unique_ptr<QGrpcServerStream> stream_;

    QString status_;

    bool weatherValid_ = false;
    QString weatherTemperature_;
    QString weatherCondition_;
    QString weatherWind_;
    bool weatherStale_ = false;

    bool routeValid_ = false;
    QString routeName_;
    QString routeFinalStop_;
    QVariantList routeStops_;

    QVariantList contentItems_;
};
