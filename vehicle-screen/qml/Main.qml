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

    header: ToolBar {
        height: 36
        background: Rectangle { color: "#333" }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 15
            text: "Шлюз: " + backend.board.status
            color: "#fff"
            font.pointSize: 11
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 10
        columns: 2
        rows: 2
        columnSpacing: 10
        rowSpacing: 10

        Rectangle {
            id: routeWidget
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
                    text: "МАРШРУТ"
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

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        Text {
                            text: backend.board.routeValid
                                  ? backend.board.routeName + " → " + backend.board.routeFinalStop
                                  : "Ожидание данных…"
                            font.pointSize: 13
                            font.bold: true
                            color: "#222"
                        }

                        Repeater {
                            model: backend.board.routeStops
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: modelData.isNext ? "#2e7d32" : "#bbb"
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                Text {
                                    text: modelData.stopName
                                    font.pointSize: 12
                                    font.bold: modelData.isNext
                                    color: "#333"
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.eta
                                    font.pointSize: 12
                                    color: modelData.isNext ? "#2e7d32" : "#666"
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
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
                    text: "ИНФОРМАЦИОННЫЙ ПОТОК"
                    font.pointSize: 14
                    font.bold: true
                    color: "#333"
                }

                Rectangle {
                    id: streamCard
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#f9f9f9"
                    border.color: "#eee"
                    border.width: 1
                    radius: 4

                    // the current batch, and which item is on screen
                    property var items: backend.board.contentItems
                    property int idx: 0
                    property var cur: (items && items.length > 0)
                                      ? items[idx % items.length] : null

                    // rotate through the batch, one item at a time
                    Timer {
                        id: rotator
                        running: streamCard.items && streamCard.items.length > 1
                        repeat: true
                        interval: streamCard.cur
                                  ? Math.max(4, streamCard.cur.displaySeconds) * 1000
                                  : 8000
                        onTriggered: streamCard.idx =
                            (streamCard.idx + 1) % Math.max(1, streamCard.items.length)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        visible: streamCard.cur !== null

                        Text {
                            text: streamCard.cur ? streamCard.cur.type.toUpperCase() : ""
                            font.pointSize: 10
                            font.bold: true
                            color: "#2e7d32"
                        }
                        Text {
                            text: streamCard.cur ? streamCard.cur.title : ""
                            font.pointSize: 16
                            font.bold: true
                            color: "#1a1a1a"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: width
                            contentHeight: bodyText.height
                            boundsBehavior: Flickable.StopAtBounds

                            Text {
                                id: bodyText
                                width: parent.width
                                text: streamCard.cur ? streamCard.cur.body : ""
                                font.pointSize: 12
                                lineHeight: 1.2
                                color: "#444"
                                wrapMode: Text.WordWrap
                            }
                        }

                        Text {
                            text: streamCard.items
                                  ? (streamCard.idx % streamCard.items.length + 1)
                                    + " / " + streamCard.items.length
                                  : ""
                            font.pointSize: 10
                            color: "#999"
                            Layout.alignment: Qt.AlignRight
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: streamCard.cur === null
                        text: "Ожидание новостей…"
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
                        font.pointSize: 42
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

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        visible: backend.board.weatherValid

                        Text {
                            text: backend.board.weatherTemperature
                            font.pointSize: 36
                            font.bold: true
                            color: "#333"
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: backend.board.weatherCondition
                            font.pointSize: 14
                            color: "#555"
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "Ветер: " + backend.board.weatherWind
                            font.pointSize: 12
                            color: "#777"
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: "данные устарели"
                            visible: backend.board.weatherStale
                            font.pointSize: 10
                            color: "#c62828"
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !backend.board.weatherValid
                        text: "Загрузка данных…"
                        color: "#999"
                    }
                }
            }
        }
    }
}
