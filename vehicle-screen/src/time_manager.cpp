#include "time_manager.h"
#include <QTime>

TimeManager::TimeManager(QObject *parent)
    : QObject(parent), timer_(new QTimer(this)), currentTime_("") {
    connect(timer_, &QTimer::timeout, this, &TimeManager::updateTime);
    timer_->start(1000);
    updateTime();
}

TimeManager::~TimeManager() {
    stopTimer();
    delete timer_;
}

void TimeManager::startTimer() {
    updateTime();
    if (!timer_->isActive()) {
        timer_->start(1000);
    }
}

void TimeManager::stopTimer() {
    if (timer_->isActive()) {
        timer_->stop();
    }
}

QString TimeManager::getTime() const {
    return currentTime_;
}

void TimeManager::updateTime() {
    currentTime_ = QTime::currentTime().toString("hh:mm:ss");
    emit timeChanged();
}

