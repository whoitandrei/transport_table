import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: page
    width: 400
    height: 600
    color: "#f0f0f0"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        Text {
            text: "Счётчик"
            font.pointSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            height: 100
            color: "white"
            border.color: "#ccc"
            border.width: 1
            radius: 8

            Text {
                id: counterText
                text: backend.counter
                font.pointSize: 48
                font.bold: true
                anchors.centerIn: parent
            }
        }

        Text {
            id: messageText
            text: backend.getMessage()
            font.pointSize: 14
            Layout.alignment: Qt.AlignHCenter
            color: "#333"
        }

        Button {
            text: "Увеличить"
            Layout.fillWidth: true
            height: 50
            font.pointSize: 14

            onClicked: {
                backend.increment()
                messageText.text = backend.getMessage()
            }
        }

        Button {
            text: "Сбросить"
            Layout.fillWidth: true
            height: 50
            font.pointSize: 14

            onClicked: {
                backend.reset()
                messageText.text = backend.getMessage()
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Connections {
        target: backend
        function onCounterChanged() {
            console.log("Counter changed to:", backend.counter)
        }
    }
}