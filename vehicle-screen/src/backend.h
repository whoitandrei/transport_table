#pragma once

#include <QObject>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(int counter READ getCounter WRITE setCounter NOTIFY counterChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    int getCounter() const { return m_counter; }
    void setCounter(int value);

    Q_INVOKABLE void increment();
    Q_INVOKABLE void reset();
    Q_INVOKABLE QString getMessage();

signals:
    void counterChanged();

private:
    int m_counter = 0;
};
