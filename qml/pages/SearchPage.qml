import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Search results across both platforms: songs, albums and MVs
/// (TARGET.md §10.18 / §10.19).
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

            IconButton {
                icon: "arrow-left"
                size: 32
                iconSize: 18
                tooltip: qsTr("返回")
                onClicked: window.leaveSearch()
            }

            Label {
                text: app.keyword.length > 0 ? qsTr("搜索：%1").arg(app.keyword) : qsTr("搜索")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                font.family: Theme.fontFamily
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Repeater {
                model: [
                    { value: 0, label: qsTr("单曲") },
                    { value: 1, label: qsTr("专辑") },
                    { value: 2, label: qsTr("MV") }
                ]
                delegate: FilterChip {
                    required property var modelData
                    text: modelData.label
                    checked: app.searchKind === modelData.value
                    onClicked: {
                        if (app.searchKind === modelData.value)
                            return
                        app.searchKind = modelData.value
                        if (app.keyword.length > 0)
                            app.search(app.keyword)
                    }
                }
            }

            Repeater {
                model: [
                    { value: 0, label: qsTr("全部") },
                    { value: 1, label: qsTr("网易云") },
                    { value: 2, label: qsTr("酷狗") }
                ]
                delegate: FilterChip {
                    required property var modelData
                    visible: app.searchKind === 0
                    text: modelData.label
                    checked: app.sourceFilter === modelData.value
                    onClicked: app.sourceFilter = modelData.value
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: app.searchMessage.length > 0
            text: app.searchMessage
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            font.family: Theme.fontFamily
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: app.searchKind === 0
            showHeader: false
            compact: page.compact
            trackModel: app.searchResults
            busy: app.searching
            emptyIcon: "search"
            emptyTitle: app.searching ? qsTr("正在搜索…") : qsTr("输入关键字开始搜索")
            emptyDescription: qsTr("同时搜索网易云音乐与酷狗音乐，结果会合并显示")
            onPlayAllRequested: player.playTracks(app.searchResults.toVariantList(), 0)
        }

        ListView {
            id: albumList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: app.searchKind === 1
            clip: true
            spacing: 2
            model: app.albumResults
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: albumList.width

                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                compact: page.compact
                onActivated: browse.openAlbum(model.source, model.id)
            }

            EmptyState {
                anchors.fill: parent
                visible: app.albumResults.count === 0
                icon: "search"
                title: app.searching ? qsTr("正在搜索专辑…") : qsTr("没有搜索到专辑")
                description: qsTr("专辑结果来自网易云与酷狗的上游搜索接口")
                busy: app.searching
            }
        }

        ListView {
            id: mvList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: app.searchKind === 2
            clip: true
            spacing: 2
            model: app.mvResults
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: CatalogRow {
                width: mvList.width

                title: model.name
                subtitle: model.subtitle
                coverUrl: model.coverUrl
                badge: model.durationText
                onActivated: player.playMv(model)
                compact: page.compact
            }

            EmptyState {
                anchors.fill: parent
                visible: app.mvResults.count === 0
                icon: "search"
                title: app.searching ? qsTr("正在搜索 MV…") : qsTr("没有搜索到 MV")
                description: qsTr("MV 结果来自网易云与酷狗的上游搜索接口")
                busy: app.searching
            }
        }

        FlatButton {
            Layout.alignment: Qt.AlignHCenter
            visible: (app.searchKind === 0 ? app.searchResults.count : (app.searchKind === 1 ? app.albumResults.count : app.mvResults.count)) > 0
                     && app.keyword.length > 0
            text: qsTr("加载更多")
            enabled: !app.searching
            onClicked: app.searchMore()
        }
    }
}
