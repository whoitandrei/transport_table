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
                    
                    Text { text: "Маршрут: " + backend.trip.route }
                    Text { text: "Номер: " + backend.trip.vehicleNumber }
                    Text { text: "Статус: " + backend.trip.status }

                    Component.onCompleted: {
                        backend.trip.fetchTripData("trip-123")
                    }
                }
            }
        }

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
                        text: backend.time.currentTime
                        font.pointSize: 18
                        font.bold: true
                        color: "#333"
                    }
                }
            }
        }

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
