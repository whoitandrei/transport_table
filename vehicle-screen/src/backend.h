#pragma once

#include <QObject>

#include "time_manager.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(TimeManager* time READ getTime CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);

    TimeManager* getTime() const {
        return timeManager_;
    }

private: 
    TimeManager* timeManager_;

};
