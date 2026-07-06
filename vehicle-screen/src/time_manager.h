#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

class TimeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentTime READ getTime NOTIFY timeChanged)

public:
    explicit TimeManager(QObject *parent = nullptr);
    ~TimeManager();

    QString getTime() const;
    void startTimer();
    void stopTimer();

signals:
    void timeChanged();

private slots:
    void updateTime();

private:
    QString currentTime_;
    QTimer *timer_;

};