import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// A single song row, styled after UiDesign/ListItem.png:
///   title / "作者 - 专辑 - 来源" plus "+" and heart actions.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string coverUrl: ""
    property string sourceName: ""
    property string durationText: ""
    property string artists: ""
    property string albumName: ""
    property string source: ""
    property string artistId: ""
    property string albumId: ""
    property bool favorite: false
    property bool playing: false
    property bool vip: false
    property bool compact: false

    signal activated()
    signal artistRequested()
    signal albumRequested()
    signal addRequested()
    signal favoriteRequested()
    signal downloadRequested()

    implicitHeight: compact ? 58 : Theme.rowHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: root.playing ? Theme.selected : (rowMouse.containsMouse ? Theme.hover : "transparent")
        Behavior on color { ColorAnimation { duration: 110 } }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: 12

        Rectangle {
            Layout.preferredWidth: root.compact ? 38 : 46
            Layout.preferredHeight: root.compact ? 38 : 46
            radius: Theme.radiusSmall
            color: Theme.surfaceAlt
            clip: true

            Image {
                id: cover
                anchors.fill: parent
                source: root.coverUrl
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
                onStatusChanged: {
                    if (status === Image.Error)
                        console.log("[cover] error: " + root.coverUrl)
                }
            }

            TintedIcon {
                anchors.centerIn: parent
                width: parent.width * 0.45
                height: width
                source: "qrc:/res/note.svg"
                color: Theme.textFaint
                visible: cover.status !== Image.Ready
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                spacing: 6
                Label {
                    text: root.title
                    color: root.playing ? Theme.accent : Theme.text
                    font.pixelSize: Theme.fontTitle
                    font.bold: root.playing
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Rectangle {
                    visible: root.vip
                    radius: 4
                    color: "#F6D06B"
                    implicitWidth: vipLabel.implicitWidth + 8
                    implicitHeight: vipLabel.implicitHeight + 2
                    Label {
                        id: vipLabel
                        anchors.centerIn: parent
                        text: "VIP"
                        color: "#6B4B00"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    text: root.artists.length > 0 ? root.artists : root.subtitle
                    color: root.artistId.length > 0 ? Theme.accent : Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width * 0.42

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: root.artistId.length > 0
                        onClicked: root.artistRequested()
                    }
                }

                Label {
                    visible: root.albumName.length > 0
                    text: " - " + root.albumName
                    color: root.albumId.length > 0 ? Theme.accent : Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                    Layout.maximumWidth: root.width * 0.36

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: root.albumId.length > 0
                        onClicked: root.albumRequested()
                    }
                }

                Label {
                    visible: root.sourceName.length > 0
                    text: " - " + root.sourceName
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }
            }
        }

        Label {
            text: root.durationText
            color: Theme.textFaint
            font.pixelSize: Theme.fontCaption
            font.family: Theme.fontFamily
            visible: root.durationText !== "" && !root.compact
        }

        IconButton {
            icon: "plus"
            size: 30
            iconSize: 16
            tooltip: qsTr("下一首播放")
            onClicked: root.addRequested()
        }

        IconButton {
            icon: root.favorite ? "heart-fill" : "heart"
            size: 30
            iconSize: 16
            iconColor: root.favorite ? Theme.danger : Theme.textMuted
            tooltip: root.favorite ? qsTr("取消收藏") : qsTr("收藏")
            onClicked: root.favoriteRequested()
        }

        IconButton {
            icon: "download"
            size: 30
            iconSize: 16
            tooltip: qsTr("下载")
            visible: !root.compact
            onClicked: root.downloadRequested()
        }
    }
}
