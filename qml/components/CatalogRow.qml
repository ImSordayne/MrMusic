import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// One album or music video row (TARGET.md §10.19).
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string coverUrl: ""
    property string sourceName: ""
    property string badge: ""          // e.g. the MV duration
    property bool compact: false
    property bool roundCover: false
    property string actionIcon: ""         // e.g. "heart" -> shows a trailing button
    property string actionTooltip: ""

    signal activated()
    signal actionTriggered()

    implicitHeight: compact ? 60 : 72

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: rowMouse.containsMouse ? Theme.hover : "transparent"
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
        anchors.rightMargin: 12
        spacing: 12

        Rectangle {
            Layout.preferredWidth: root.compact ? 44 : 54
            Layout.preferredHeight: root.compact ? 44 : 54
            radius: root.roundCover ? width / 2 : Theme.radiusSmall
            color: Theme.surfaceAlt
            clip: true

            Image {
                id: cover
                anchors.fill: parent
                source: root.coverUrl
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
            }

            TintedIcon {
                anchors.centerIn: parent
                width: parent.width * 0.42
                height: width
                source: "qrc:/res/note.svg"
                color: Theme.textFaint
                visible: cover.status !== Image.Ready
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.family: Theme.fontFamily
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: root.subtitle
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
                elide: Text.ElideRight
            }
        }

        Rectangle {
            visible: root.badge.length > 0
            radius: 4
            color: Theme.surfaceAlt
            implicitWidth: badgeLabel.implicitWidth + 10
            implicitHeight: badgeLabel.implicitHeight + 4
            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.badge
                color: Theme.textMuted
                font.pixelSize: 11
                font.family: Theme.fontFamily
            }
        }

        IconButton {
            visible: root.actionIcon.length > 0
            icon: root.actionIcon
            size: 30
            iconSize: 16
            tooltip: root.actionTooltip
            onClicked: root.actionTriggered()
        }

        TintedIcon {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: "qrc:/res/next.svg"
            color: Theme.textFaint
            visible: root.actionIcon.length === 0
        }
    }
}
