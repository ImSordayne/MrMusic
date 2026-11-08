import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Track list with an optional header and an empty state.
Item {
    id: view

    property var trackModel: null
    property string title: ""
    property bool showHeader: true
    property bool showPlayAll: true
    property bool compact: false
    property string emptyIcon: "note"
    property string emptyTitle: qsTr("这里还没有内容")
    property string emptyDescription: ""
    property bool busy: false

    signal playAllRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            visible: view.showHeader
            spacing: 8

            Label {
                text: view.title
                color: Theme.text
                font.pixelSize: Theme.fontTitle + 2
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            FlatButton {
                visible: view.showPlayAll && view.trackModel && view.trackModel.count > 0
                text: qsTr("播放全部")
                onClicked: view.playAllRequested()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: view.trackModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: rowItem

                required property string title
                required property string subtitle
                required property string coverUrl
                required property string sourceName
                required property string durationText
                required property bool favorite
                required property bool playing
                required property bool vip
                required property var track
                required property int index
                required property string artists
                required property string artistId
                required property string albumId
                required property string source
                required property string album

                width: list.width
                height: view.compact ? 58 : Theme.rowHeight

                TrackRow {
                    anchors.fill: parent
                    compact: view.compact
                    title: rowItem.title
                    subtitle: rowItem.subtitle
                    artists: rowItem.artists
                    albumName: rowItem.album
                    source: rowItem.source
                    artistId: rowItem.artistId
                    albumId: rowItem.albumId
                    onArtistRequested: browse.openArtist(rowItem.source, rowItem.artistId)
                    onAlbumRequested: browse.openAlbum(rowItem.source, rowItem.albumId)
                    coverUrl: rowItem.coverUrl
                    sourceName: rowItem.sourceName
                    durationText: rowItem.durationText
                    favorite: rowItem.favorite
                    playing: rowItem.playing
                    vip: rowItem.vip
                    onActivated: player.playTracks(view.trackModel.toVariantList(), rowItem.index)
                    onAddRequested: player.enqueueNext(rowItem.track)
                    onFavoriteRequested: library.toggleFavorite(rowItem.track)
                    onDownloadRequested: downloads.enqueue(rowItem.track)
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !view.trackModel || view.trackModel.count === 0
            icon: view.emptyIcon
            title: view.busy ? qsTr("正在加载…") : view.emptyTitle
            description: view.emptyDescription
            busy: view.busy
        }
    }
}
