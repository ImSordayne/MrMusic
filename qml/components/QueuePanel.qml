import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Right hand drawer listing the play queue.
Drawer {
    id: drawer

    edge: Qt.RightEdge
    width: Math.min(420, window.width * 0.9)
    height: window.height
    interactive: true
    padding: 0

    background: Rectangle { color: Theme.surface }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8

            Label {
                text: qsTr("播放队列 (%1)").arg(player.queue.count)
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }

            IconButton {
                icon: "trash"
                size: 28
                iconSize: 15
                tooltip: qsTr("清空队列")
                onClicked: player.clearQueue()
            }

            IconButton {
                icon: "close"
                size: 28
                iconSize: 14
                onClicked: drawer.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        ListView {
            id: queueList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: player.queue

            delegate: Item {
                required property int index
                required property string title
                required property string artists
                required property bool playing
                required property string durationText

                width: queueList.width
                height: 52

                Rectangle {
                    anchors.fill: parent
                    color: playing ? Theme.selected : (queueMouse.containsMouse ? Theme.hover : "transparent")
                }

                MouseArea {
                    id: queueMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: player.playAt(index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 8
                    spacing: 10

                    Label {
                        text: index + 1
                        color: playing ? Theme.accent : Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        width: 22
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            text: title
                            color: playing ? Theme.accent : Theme.text
                            font.pixelSize: Theme.fontBody
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: artists
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontCaption
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Label {
                        text: durationText
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                    }

                    IconButton {
                        icon: "close"
                        size: 26
                        iconSize: 12
                        onClicked: player.removeFromQueue(index)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 18
            visible: player.queue.count === 0
            text: qsTr("队列为空")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
