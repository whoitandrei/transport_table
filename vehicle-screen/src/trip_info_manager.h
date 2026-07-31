#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class TripInfoManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ getRoute NOTIFY tripDataChanged)
    Q_PROPERTY(QString vehicleNumber READ getVehicleNumber NOTIFY tripDataChanged)
    Q_PROPERTY(QString status READ getStatus NOTIFY tripDataChanged)
    Q_PROPERTY(QString errorMessage READ getErrorMessage NOTIFY tripDataError)

public:
    explicit TripInfoManager(QObject *parent = nullptr);
    ~TripInfoManager();

    QString getRoute() const { return m_route; }
    QString getVehicleNumber() const { return m_vehicleNumber; }
    QString getStatus() const { return m_status; }
    QString getErrorMessage() const { return m_errorMessage; }

    Q_INVOKABLE void fetchTripData(const QString &tripId);

signals:
    void tripDataChanged();
    void tripDataError(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    void parseJson(const QByteArray &data);

    QNetworkAccessManager *m_networkManager;
    QString m_route;
    QString m_vehicleNumber;
    QString m_status;
    QString m_errorMessage;
};