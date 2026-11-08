import QtQuick
import QtQuick.Controls

/// Borderless text button with an explicit theme colour (the Basic style's
/// default text colour is too low contrast in one of the two themes).
Button {
    id: control

    property color textColor: Theme.textMuted

    flat: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontCaption

    contentItem: Label {
        text: control.text
        color: control.enabled ? control.textColor : Theme.textFaint
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.down || control.hovered ? Theme.hover : "transparent"
    }
}
