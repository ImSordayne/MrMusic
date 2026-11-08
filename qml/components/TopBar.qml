import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Search bar + window actions.
Rectangle {
    id: bar

    property bool compact: false
    property alias searchText: searchField.text
    property int downloadCount: downloads.activeCount

    signal searchRequested(string keyword)
    signal downloadClicked()
    signal queueClicked()
    signal avatarClicked(string platform)

    color: Theme.background
    implicitHeight: 62

    function focusSearch() {
        searchField.forceActiveFocus()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: compact ? 12 : 22
        anchors.rightMargin: compact ? 12 : 22
        spacing: 10

        Row {
            visible: bar.compact
            spacing: 6
            AvatarBubble {
                platform: "netease"
                loggedIn: accounts.neteaseLoggedIn
                image: accounts.neteaseAvatar
                width: 34
                height: 34
                onClicked: bar.avatarClicked("netease")
            }
            AvatarBubble {
                platform: "kugou"
                loggedIn: accounts.kugouLoggedIn
                image: accounts.kugouAvatar
                width: 34
                height: 34
                onClicked: bar.avatarClicked("kugou")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 520
            Layout.preferredHeight: 38
            radius: 19
            color: Theme.surfaceAlt

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                TintedIcon {
                    source: "qrc:/res/search.svg"
                    width: 16
                    height: 16
                    color: Theme.textMuted
                }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("搜索歌曲（网易云 + 酷狗）")
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.fontFamily
                    color: Theme.text
                    background: null
                    selectByMouse: true
                    onAccepted: bar.searchRequested(text)

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            text = ""
                            bar.searchRequested("")
                            event.accepted = true
                        }
                    }
                }

                IconButton {
                    icon: "search-clear"
                    size: 24
                    iconSize: 14
                    visible: searchField.text.length > 0
                    onClicked: {
                        searchField.text = ""
                        bar.searchRequested("")
                    }
                }
            }
        }

        Item { Layout.fillWidth: !bar.compact }

        IconButton {
            icon: "download"
            size: 34
            tooltip: qsTr("下载管理")
            onClicked: bar.downloadClicked()
        }

        IconButton {
            icon: "queue"
            size: 34
            tooltip: qsTr("播放队列")
            onClicked: bar.queueClicked()
        }
    }
}
