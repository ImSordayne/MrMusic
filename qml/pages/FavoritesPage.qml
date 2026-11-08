import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Favourites page: songs (local + account), account albums and account MVs
/// (TARGET.md §10.21).
Item {
    id: page

    property bool compact: false
    property int tab: 0   // 0 = 歌曲, 1 = 专辑, 2 = MV

    Component.onCompleted: library.refreshRemote()

    Connections {
        target: accounts
        function onStateChanged() { library.refreshRemote() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("我的收藏")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("%1 首").arg(library.favoriteCount)
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
            }

            IconButton {
                icon: "refresh"
                size: 30
                iconSize: 16
                tooltip: qsTr("刷新账号收藏")
                onClicked: library.refreshRemote()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { value: 0, label: qsTr("歌曲") },
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

            Label {
                visible: library.remoteMessage.length > 0
                text: library.remoteMessage
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                Layout.maximumWidth: page.width * 0.55
            }
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 0
            showHeader: false
            compact: page.compact
            busy: library.remoteLoading
            trackModel: library.favorites
            emptyIcon: "heart"
            emptyTitle: qsTr("还没有收藏的歌曲")
            emptyDescription: qsTr("点击歌曲右侧的心形图标即可收藏，登录后会同步到账号")
            onPlayAllRequested: player.playTracks(library.favorites.toVariantList(), 0)
        }

        ListView {
            id: albumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 1
            clip: true
            spacing: 2
            model: library.favoriteAlbums
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: albumList.width
                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                actionIcon: "heart-fill"
                actionTooltip: qsTr("取消收藏")
                compact: page.compact
                onActivated: browse.openAlbum(model.source, model.id)
                onActionTriggered: library.removeFavoriteAlbum(model.id)
            }

            EmptyState {
                anchors.fill: parent
                visible: library.favoriteAlbums.count === 0
                icon: "heart"
                title: library.remoteLoading ? qsTr("正在加载…") : qsTr("还没有收藏的专辑")
                description: qsTr("登录网易云后，账号收藏的专辑会显示在这里")
                busy: library.remoteLoading
            }
        }

        ListView {
            id: mvList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.tab === 2
            clip: true
            spacing: 2
            model: library.favoriteMvs
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: mvList.width
                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                badge: model.durationText
                onActivated: player.playMv(model)
                actionIcon: "heart-fill"
                actionTooltip: qsTr("取消收藏")
                compact: page.compact
                onActionTriggered: library.removeFavoriteMv(model.id)
            }

            EmptyState {
                anchors.fill: parent
                visible: library.favoriteMvs.count === 0
                icon: "heart"
                title: library.remoteLoading ? qsTr("正在加载…") : qsTr("还没有收藏的 MV")
                description: qsTr("登录网易云后，账号收藏的 MV 会显示在这里")
                busy: library.remoteLoading
            }
        }
    }
}
