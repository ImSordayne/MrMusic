import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Right hand drawer listing download jobs and their progress.
Drawer {
    id: drawer

    edge: Qt.RightEdge
    width: Math.min(460, window.width * 0.92)
    height: window.height
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
                text: qsTr("下载管理")
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }

            IconButton {
                icon: "refresh"
                size: 28
                iconSize: 15
                tooltip: qsTr("打开下载目录")
                onClicked: downloads.openFolder()
            }

            IconButton {
                icon: "trash"
                size: 28
                iconSize: 15
                tooltip: qsTr("清除已完成")
                onClicked: downloads.clearFinished()
            }

            IconButton {
                icon: "close"
                size: 28
                iconSize: 14
                onClicked: drawer.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        Label {
            Layout.fillWidth: true
            Layout.margins: 14
            text: qsTr("保存目录：%1").arg(downloads.targetDir)
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            font.family: Theme.fontFamily
            elide: Text.ElideMiddle
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: downloads.items

            delegate: Item {
                required property var modelData
                required property int index

                width: list.width
                height: 74

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: modelData.title
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.family: Theme.fontFamily
                            elide: Text.ElideRight
                        }

                        Label {
                            text: {
                                switch (modelData.state) {
                                case "queued": return qsTr("排队中")
                                case "resolving": return qsTr("解析中")
                                case "downloading": return qsTr("下载中")
                                case "tagging": return qsTr("写入标签")
                                case "done": return qsTr("完成")
                                case "error": return qsTr("失败")
                                default: return ""
                                }
                            }
                            color: modelData.state === "error" ? Theme.danger
                                   : (modelData.state === "done" ? Theme.accent : Theme.textMuted)
                            font.pixelSize: Theme.fontCaption
                            font.family: Theme.fontFamily
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: modelData.artists + " · " + modelData.quality
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: modelData.progress
                        visible: modelData.state !== "error"
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: modelData.state === "error" && modelData.message.length > 0
                        text: modelData.message
                        color: Theme.danger
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 18
            visible: downloads.items.length === 0
            text: qsTr("暂无下载任务")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
