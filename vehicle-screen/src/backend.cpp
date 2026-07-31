#include "backend.h"
#include "time_manager.h"
#include <iostream>

Backend::Backend(QObject *parent)
    : QObject(parent) {
        timeManager_ = new TimeManager(this);
        timeManager_->startTimer();
        tripInfoManager_ = new TripInfoManager(this);
    }


