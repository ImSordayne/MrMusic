import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Artist page: header (avatar, stats, follow button) and three tabs for
/// songs / albums / MVs (TARGET.md §10.20).
Item {
    id: page

    property bool compact: false
    property int tab: 0   // 0 = 音乐, 1 = 专辑, 2 = MV

    readonly property var artist: browse.artist

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: 12

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
                text: qsTr("歌手")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                font.family: Theme.fontFamily
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.preferredWidth: compact ? 76 : 104
                Layout.preferredHeight: compact ? 76 : 104
                radius: width / 2
                color: Theme.surfaceAlt
                clip: true

                Image {
                    id: avatar
                    anchors.fill: parent
                    source: page.artist.avatarUrl !== undefined ? page.artist.avatarUrl : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                TintedIcon {
                    anchors.centerIn: parent
                    width: parent.width * 0.4
                    height: width
                    source: "qrc:/res/user.svg"
                    color: Theme.iconStrong
                    visible: avatar.status !== Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: page.artist.name !== undefined ? page.artist.name : qsTr("加载中…")
                    color: Theme.text
                    font.pixelSize: 22
                    font.bold: true
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        var parts = []
                        if (page.artist.sourceName !== undefined)
                            parts.push(page.artist.sourceName)
                        if (page.artist.songCount > 0)
                            parts.push(qsTr("%1 首歌曲").arg(page.artist.songCount))
                        if (page.artist.albumCount > 0)
                            parts.push(qsTr("%1 张专辑").arg(page.artist.albumCount))
                        if (page.artist.mvCount > 0)
                            parts.push(qsTr("%1 个 MV").arg(page.artist.mvCount))
                        return parts.join(" · ")
                    }
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: 8

                    Button {
                        text: page.artist.followed === true ? qsTr("已关注") : qsTr("关注歌手")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                        enabled: !browse.loading
                        onClicked: browse.toggleFollowArtist()
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { value: 0, label: qsTr("音乐") },
                    { value: 1, label: qsTr("专辑") },
                    { value: 2, label: qsTr("MV") }
                ]
                delegate: FilterChip {
                    required property var modelData
                    text: modelData.label
                    checked: page.tab === modelData.value
                    onClicked: page.tab = modelData.value
                }
            }

            Item { Layout.fillWidth: true }

            FlatButton {
                visible: page.tab === 0 && browse.artistTracks.count > 0
                text: qsTr("播放全部")
                onClicked: player.playTracks(browse.artistTracks.toVariantList(), 0)
            }
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 0
            showHeader: false
            compact: page.compact
            trackModel: browse.artistTracks
            busy: browse.loading
            emptyIcon: "note"
            emptyTitle: browse.loading ? qsTr("正在加载歌手作品…") : qsTr("暂时没有可播放的歌曲")
            emptyDescription: qsTr("数据来自上游平台的歌手作品列表")
            onPlayAllRequested: player.playTracks(browse.artistTracks.toVariantList(), 0)
        }

        ListView {
            id: listAlbums
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 1
            clip: true
            spacing: 2
            model: browse.artistAlbums
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: listAlbums.width

                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                compact: page.compact
                onActivated: browse.openAlbum(model.source, model.id)
            }

            EmptyState {
                anchors.fill: parent
                visible: browse.artistAlbums.count === 0
                icon: "note"
                title: browse.loading ? qsTr("正在加载…") : qsTr("这位歌手还没有专辑")
                busy: browse.loading
            }
        }

        ListView {
            id: listMvs
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 2
            clip: true
            spacing: 2
            model: browse.artistMvs
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: listMvs.width

                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                badge: model.durationText
                onActivated: player.playMv(model)
                compact: page.compact
            }

            EmptyState {
                anchors.fill: parent
                visible: browse.artistMvs.count === 0
                icon: "note"
                title: browse.loading ? qsTr("正在加载…") : qsTr("这位歌手还没有 MV")
                busy: browse.loading
            }
        }
    }
}
