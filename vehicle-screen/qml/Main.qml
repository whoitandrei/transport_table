import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Транспортная таблица"
    width: 1200
    height: 800
    visible: true
    color: "#f5f5f5"

    GridLayout {
        anchors.fill: parent
        anchors.margins: 10
        columns: 2
        rows: 2
        columnSpacing: 10
        rowSpacing: 10

        // Верхний левый: ИНФОРМАЦИЯ О ПОЕЗДКЕ
        Rectangle {
            id: tripInfoWidget
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#ddd"
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                Text {
                    text: "ИНФОРМАЦИЯ О ПОЕЗДКЕ"
                    font.pointSize: 14
                    font.bold: true
                    color: "#333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#f9f9f9"
                    border.color: "#eee"
                    border.width: 1
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "Маршрут, номер транспорта, статус..."
                        color: "#999"
                    }
                }
            }
        }

        // Верхний правый: ИНФОРМАЦИОННЫЙ ПОТОК (БД)
        Rectangle {
            id: infoStreamWidget
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#ddd"
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                Text {
                    text: "ИНФОРМАЦИОННЫЙ ПОТОК (БД)"
                    font.pointSize: 14
                    font.bold: true
                    color: "#333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#f9f9f9"
                    border.color: "#eee"
                    border.width: 1
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "События и уведомления..."
                        color: "#999"
                    }
                }
            }
        }

        // Нижний левый: ВРЕМЯ
        Rectangle {
            id: timeWidget
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#ddd"
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                Text {
                    text: "ВРЕМЯ"
                    font.pointSize: 14
                    font.bold: true
                    color: "#333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#f9f9f9"
                    border.color: "#eee"
                    border.width: 1
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "00:00:00"
                        font.pointSize: 18
                        font.bold: true
                        color: "#333"
                    }
                }
            }
        }

        // Нижний правый: ПОГОДА
        Rectangle {
            id: weatherWidget
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#ddd"
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                Text {
                    text: "ПОГОДА"
                    font.pointSize: 14
                    font.bold: true
                    color: "#333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#f9f9f9"
                    border.color: "#eee"
                    border.width: 1
                    radius: 4

                    Text {
                        anchors.centerIn: parent
                        text: "Загрузка данных..."
                        color: "#999"
                    }
                }
            }
        }
    }
}
