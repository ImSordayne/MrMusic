import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Mobile bottom navigation bar (TARGET.md §6).
Rectangle {
    id: nav

    property int currentPage: 0
    signal pageSelected(int page)

    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 0

        Repeater {
            model: [
                { icon: "house", fill: "house-fill", label: qsTr("首页") },
                { icon: "clock", fill: "clock-fill", label: qsTr("最近") },
                { icon: "heart", fill: "heart-fill", label: qsTr("收藏") },
                { icon: "gear", fill: "gear-fill", label: qsTr("设置") }
            ]

            delegate: Item {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                Layout.fillHeight: true

                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    TintedIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 22
                        height: 22
                        source: "qrc:/res/" + (nav.currentPage === index ? modelData.fill : modelData.icon) + ".svg"
                        color: Theme.iconStrong
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: Theme.iconStrong
                        font.pixelSize: 10
                        font.family: Theme.fontFamily
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: nav.pageSelected(index)
                }
            }
        }
    }
}
