import QtQuick
import QtQuick.Controls

/// Small pill shaped toggle used for filters (source selection, tabs, ...).
Item {
    id: chip

    property string text: ""
    property bool checked: false

    signal clicked()

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 28

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: chip.checked ? Theme.accent : (mouse.containsMouse ? Theme.hover : Theme.surfaceAlt)
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Label {
        id: label
        anchors.centerIn: parent
        text: chip.text
        color: chip.checked ? "white" : Theme.textMuted
        font.pixelSize: Theme.fontCaption
        font.family: Theme.fontFamily
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chip.clicked()
    }
}
