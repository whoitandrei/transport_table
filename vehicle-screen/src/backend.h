#pragma once

#include <QObject>

#include "time_manager.h"
#include "trip_info_manager.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(TimeManager* time READ getTime CONSTANT)
    Q_PROPERTY(TripInfoManager* trip READ getTrip CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);

    TimeManager* getTime() const {
        return timeManager_;
    }

    TripInfoManager* getTrip() const {
        return tripInfoManager_;
    }

private:
    TimeManager* timeManager_;
    TripInfoManager* tripInfoManager_;

};
