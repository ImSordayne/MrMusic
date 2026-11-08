import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// Small round/square icon button used all over the UI.
Item {
    id: root

    property string icon: ""
    property int size: 34
    property int iconSize: 18
    property bool flat: true
    property bool checked: false
    property string tooltip: ""
    property color iconColor: checked ? Theme.accent : Theme.textMuted
    property color background: checked ? Theme.accentSoft : (mouse.containsMouse && flat ? Theme.hover : "transparent")
    property bool round: true

    signal clicked()

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: root.round ? root.height / 2 : Theme.radiusSmall
        color: root.background
        Behavior on color { ColorAnimation { duration: 120 } }

        TintedIcon {
            anchors.centerIn: parent
            source: root.icon ? "qrc:/res/" + root.icon + ".svg" : ""
            width: root.iconSize
            height: root.iconSize
            color: root.iconColor
            visible: root.icon !== ""
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: tooltip !== "" && mouse.containsMouse
    ToolTip.text: tooltip
    ToolTip.delay: 600
}
