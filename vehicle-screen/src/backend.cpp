#include "backend.h"
#include <iostream>

Backend::Backend(QObject *parent)
    : QObject(parent), m_counter(0) {}

void Backend::setCounter(int value) {
    if (m_counter != value) {
        m_counter = value;
        emit counterChanged();
    }
}

void Backend::increment() {
    m_counter++;
    emit counterChanged();
    std::cout << "Counter incremented to: " << m_counter << std::endl;
}

void Backend::reset() {
    m_counter = 0;
    emit counterChanged();
    std::cout << "Counter reset" << std::endl;
}

QString Backend::getMessage() {
    return QString("Текущее значение: %1").arg(m_counter);
}
