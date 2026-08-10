#pragma once

#include <QObject>

#include "board_client.h"
#include "time_manager.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(TimeManager* time READ getTime CONSTANT)
    Q_PROPERTY(BoardClient* board READ getBoard CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);

    TimeManager* getTime() const {
        return timeManager_;
    }

    BoardClient* getBoard() const {
        return boardClient_;
    }

private:
    TimeManager* timeManager_;
    BoardClient* boardClient_;
};
