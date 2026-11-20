import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MrMusic

/// Circular account avatar: red for NetEase, blue for KuGou.
///
/// Placeholder rules (TARGET.md §9.13): when the platform is not logged in, or
/// the avatar image cannot be loaded, only the platform character is shown -
/// never a character on top of an icon.
Item {
    id: bubble

    property string platform: "netease"
    property bool loggedIn: false
    property string image: ""

    readonly property string placeholderText: platform === "netease" ? "网" : "酷"
    readonly property color platformColor: platform === "netease" ? Theme.netease : Theme.kugou
    readonly property bool showsImage: loggedIn && image.length > 0 && avatarImage.status === Image.Ready

    signal clicked()

    implicitWidth: 42
    implicitHeight: 42

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: bubble.platformColor
        opacity: bubble.loggedIn ? 0.95 : 0.35
    }

    // A plain QML Image is rectangular; PreserveAspectCrop alone only crops the
    // source, it does NOT round the corners, so a square avatar (e.g. KuGou's
    // white-background icons) used to stick out of the circular bubble.
    // Mask it with a circle (TARGET.md §10.31). This is a geometric mask, not
    // the shader colour tint that §9.12 forbids.
    Image {
        id: avatarImage
        anchors.fill: parent
        anchors.margins: 3
        source: bubble.loggedIn ? bubble.image : ""
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        visible: bubble.showsImage
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: circleMask
            maskSpreadAtMin: 1.0
            maskThresholdMin: 0.5
        }
    }

    // Invisible circular mask source. It must be rendered to a layer but not
    // shown in the visual tree; MultiEffect samples it as a texture.
    Item {
        id: circleMask
        width: avatarImage.width
        height: avatarImage.height
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "white"
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 3
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: Theme.surface
        visible: bubble.showsImage
    }

    Label {
        anchors.centerIn: parent
        text: bubble.placeholderText
        color: "white"
        font.pixelSize: Math.max(10, Math.round(bubble.width * 0.3))
        font.bold: true
        font.family: Theme.fontFamily
        visible: !bubble.showsImage
    }

    MouseArea {
        id: bubbleMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: bubble.clicked()
    }

    ToolTip.visible: bubbleMouse.containsMouse
    ToolTip.text: bubble.platform === "netease"
                  ? (bubble.loggedIn ? qsTr("网易云：%1").arg(accounts.neteaseNickname) : qsTr("登录网易云"))
                  : (bubble.loggedIn ? qsTr("酷狗：%1").arg(accounts.kugouNickname) : qsTr("登录酷狗"))
    ToolTip.delay: 500
}
