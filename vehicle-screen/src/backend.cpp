#include "backend.h"

Backend::Backend(QObject *parent)
    : QObject(parent) {
        timeManager_ = new TimeManager(this);
        timeManager_->startTimer();
        boardClient_ = new BoardClient(this);
        boardClient_->start();
    }
