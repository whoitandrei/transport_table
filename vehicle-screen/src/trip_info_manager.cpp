#include "trip_info_manager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

TripInfoManager::TripInfoManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &TripInfoManager::onReplyFinished);
}

TripInfoManager::~TripInfoManager() {}

void TripInfoManager::fetchTripData(const QString &tripId)
{
    QString url = QString("http://gateway:8080/api/trip/%1").arg(tripId);
    QNetworkRequest request{QUrl(url)};
    m_networkManager->get(request);
}

void TripInfoManager::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        m_errorMessage = reply->errorString();
        emit tripDataError(m_errorMessage);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    parseJson(data);
    reply->deleteLater();
}

void TripInfoManager::parseJson(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString newRoute = obj["route"].toString();
    QString newVehicleNumber = obj["vehicleNumber"].toString();
    QString newStatus = obj["status"].toString();

    if (newRoute != m_route || newVehicleNumber != m_vehicleNumber ||
        newStatus != m_status) {
        m_route = newRoute;
        m_vehicleNumber = newVehicleNumber;
        m_status = newStatus;
        emit tripDataChanged();
    }
}