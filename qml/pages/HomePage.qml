import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Home page: the two daily recommendation entries (one per platform).
Item {
    id: page

    property bool compact: false
    property int activeTab: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("每日推荐")
                color: Theme.text
                font.pixelSize: 22
                font.bold: true
                font.family: Theme.fontFamily
                Layout.fillWidth: true
            }

            IconButton {
                icon: "refresh"
                size: 32
                iconSize: 16
                tooltip: qsTr("刷新")
                onClicked: app.refreshDaily()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: app.dailyMessage.length > 0
            text: app.dailyMessage
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        TabBar {
            Layout.fillWidth: true
            visible: page.compact
            currentIndex: page.activeTab
            onCurrentIndexChanged: page.activeTab = currentIndex

            TabButton { text: qsTr("网易云 - 每日推荐"); font.family: Theme.fontFamily }
            TabButton { text: qsTr("酷狗 - 每日推荐"); font.family: Theme.fontFamily }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 22
            visible: !page.compact

            TrackListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("网易云 - 每日推荐")
                trackModel: app.neteaseDaily
                busy: app.loadingDaily
                emptyTitle: qsTr("网易云每日推荐需要登录")
                emptyDescription: qsTr("点击左侧头像登录后即可看到每日推荐")
                emptyIcon: "clock"
                onPlayAllRequested: player.playTracks(app.neteaseDaily.toVariantList(), 0)
            }

            Rectangle { width: 1; Layout.fillHeight: true; color: Theme.divider }

            TrackListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("酷狗 - 每日推荐")
                trackModel: app.kugouDaily
                busy: app.loadingDaily
                emptyTitle: qsTr("暂无酷狗推荐")
                emptyDescription: qsTr("稍后重试或检查网络")
                emptyIcon: "clock"
                onPlayAllRequested: player.playTracks(app.kugouDaily.toVariantList(), 0)
            }
        }

        TrackListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.compact
            showHeader: false
            compact: true
            trackModel: page.activeTab === 0 ? app.neteaseDaily : app.kugouDaily
            busy: app.loadingDaily
            emptyTitle: page.activeTab === 0 ? qsTr("网易云每日推荐需要登录") : qsTr("暂无酷狗推荐")
            emptyIcon: "clock"
            onPlayAllRequested: player.playTracks(page.activeTab === 0 ? app.neteaseDaily.toVariantList()
                                                                       : app.kugouDaily.toVariantList(), 0)
        }
    }
}
