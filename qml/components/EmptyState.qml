import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Placeholder shown for empty / error / loading states.
ColumnLayout {
    id: state

    property string icon: "note"
    property string title: ""
    property string description: ""
    property bool busy: false

    spacing: 10
    Layout.alignment: Qt.AlignHCenter

    BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: state.busy
        visible: state.busy
    }

    TintedIcon {
        Layout.alignment: Qt.AlignHCenter
        visible: !state.busy
        source: "qrc:/res/" + state.icon + ".svg"
        width: 44
        height: 44
        color: Theme.textFaint
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: state.title
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.family: Theme.fontFamily
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: state.description
        color: Theme.textMuted
        font.pixelSize: Theme.fontCaption
        font.family: Theme.fontFamily
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        Layout.maximumWidth: 320
    }
}
