import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Album detail page: header (cover, meta, subscribe button) + track list.
Item {
    id: page

    property bool compact: false

    readonly property var album: browse.album

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: 12

        // --- back + header ------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            IconButton {
                icon: "arrow-left"
                size: 32
                iconSize: 18
                tooltip: qsTr("返回")
                onClicked: browse.close()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("专辑")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                font.family: Theme.fontFamily
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Rectangle {
                Layout.preferredWidth: compact ? 96 : 132
                Layout.preferredHeight: compact ? 96 : 132
                radius: Theme.radiusSmall
                color: Theme.surfaceAlt
                clip: true

                Image {
                    id: cover
                    anchors.fill: parent
                    source: page.album.coverUrl !== undefined ? page.album.coverUrl : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                TintedIcon {
                    anchors.centerIn: parent
                    width: parent.width * 0.35
                    height: width
                    source: "qrc:/res/note.svg"
                    color: Theme.textFaint
                    visible: cover.status !== Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: page.album.name !== undefined ? page.album.name : qsTr("加载中…")
                    color: Theme.text
                    font.pixelSize: 22
                    font.bold: true
                    font.family: Theme.fontFamily
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 6
                    Label {
                        text: qsTr("歌手：")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                    }
                    Label {
                        text: page.album.artist !== undefined ? page.album.artist : ""
                        color: Theme.accent
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: page.album.artistId !== undefined && page.album.artistId.length > 0
                            onClicked: browse.openArtist(page.album.source, page.album.artistId)
                        }
                    }
                    Label {
                        text: page.album.sourceName !== undefined ? "· " + page.album.sourceName : ""
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: page.album.subtitle !== undefined ? page.album.subtitle : ""
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 8

                    Button {
                        text: page.album.subscribed === true ? qsTr("已收藏") : qsTr("收藏专辑")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                        enabled: !browse.loading
                        onClicked: browse.toggleSubscribeAlbum()
                    }

                    FlatButton {
                        text: qsTr("播放全部")
                        enabled: browse.albumTracks.count > 0
                        onClicked: player.playTracks(browse.albumTracks.toVariantList(), 0)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: browse.message.length > 0
            text: browse.message
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            font.family: Theme.fontFamily
            wrapMode: Text.Wrap
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            showHeader: false
            compact: page.compact
            trackModel: browse.albumTracks
            busy: browse.loading
            emptyIcon: "note"
            emptyTitle: browse.loading ? qsTr("正在加载专辑…") : qsTr("这张专辑还没有可播放的歌曲")
            emptyDescription: qsTr("数据来自上游平台的作品列表")
            onPlayAllRequested: player.playTracks(browse.albumTracks.toVariantList(), 0)
        }
    }
}
