import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Recently played tracks.
Item {
    id: page

    property bool compact: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("最近播放")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }

            FlatButton {
                text: qsTr("清空")
                enabled: library.history.count > 0
                onClicked: library.clearHistory()
            }
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            showHeader: false
            compact: page.compact
            trackModel: library.history
            emptyIcon: "clock"
            emptyTitle: qsTr("还没有播放记录")
            emptyDescription: qsTr("播放过的歌曲会自动记录在这里")
            onPlayAllRequested: player.playTracks(library.history.toVariantList(), 0)
        }
    }
}
