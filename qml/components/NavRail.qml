import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Desktop side bar (TARGET.md §6): logo, page icons (fill variants when
/// selected) and the two account avatars at the bottom left.
Rectangle {
    id: rail

    property int currentPage: 0
    signal pageSelected(int page)
    signal avatarClicked(string platform)

    color: Theme.surface
    border.color: Theme.divider
    border.width: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 16
        spacing: 6

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 10
            spacing: 6

            TintedIcon {
                source: "qrc:/res/logo-mark.svg"
                color: Theme.iconStrong
                iconSize: 24
            }

            Label {
                text: qsTr("MrMusic")
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
                font.family: Theme.fontFamily
            }
        }

        Repeater {
            model: [
                { icon: "house", fill: "house-fill", label: qsTr("首页") },
                { icon: "clock", fill: "clock-fill", label: qsTr("最近播放") },
                { icon: "heart", fill: "heart-fill", label: qsTr("收藏") },
                { icon: "gear", fill: "gear-fill", label: qsTr("设置") }
            ]

            delegate: Item {
                required property var modelData
                required property int index

                Layout.alignment: Qt.AlignHCenter
                width: 44
                height: 44

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSmall
                    color: rail.currentPage === index ? Theme.accentSoft : (navMouse.containsMouse ? Theme.hover : "transparent")
                }

                TintedIcon {
                    anchors.centerIn: parent
                    width: Theme.iconSize
                    height: Theme.iconSize
                    source: "qrc:/res/" + (rail.currentPage === index ? modelData.fill : modelData.icon) + ".svg"
                    color: Theme.iconStrong
                }

                MouseArea {
                    id: navMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: rail.pageSelected(index)
                }

                ToolTip.visible: navMouse.containsMouse
                ToolTip.text: modelData.label
                ToolTip.delay: 500
            }
        }

        Item { Layout.fillHeight: true }

        // two circular avatars: left = NetEase, right = KuGou (stacked on desktop)
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            AvatarBubble {
                platform: "netease"
                loggedIn: accounts.neteaseLoggedIn
                image: accounts.neteaseAvatar
                onClicked: rail.avatarClicked("netease")
            }
            AvatarBubble {
                platform: "kugou"
                loggedIn: accounts.kugouLoggedIn
                image: accounts.kugouAvatar
                onClicked: rail.avatarClicked("kugou")
            }
        }
    }
}
