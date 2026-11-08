import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Login dialog: QR code scan or account/password, per platform.
Dialog {
    id: dialog

    property string platform: "netease"

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 400
    padding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.divider
    }

    onOpened: startLogin()

    function startLogin() {
        passwordField.text = ""
        accounts.startQrLogin(platform)
    }

    contentItem: ColumnLayout {
        spacing: 0

        // tabs
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8

            Repeater {
                model: [
                    { key: "netease", label: qsTr("网易云音乐") },
                    { key: "kugou", label: qsTr("酷狗音乐") }
                ]
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    flat: !(dialog.platform === modelData.key)
                    highlighted: dialog.platform === modelData.key
                    font.family: Theme.fontFamily
                    onClicked: {
                        dialog.platform = modelData.key
                        dialog.startLogin()
                    }
                }
            }

            IconButton {
                icon: "close"
                size: 28
                iconSize: 14
                onClicked: dialog.close()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 14

            Label {
                text: dialog.platform === "netease"
                      ? (accounts.neteaseLoggedIn ? qsTr("已登录：%1").arg(accounts.neteaseNickname)
                                                  : qsTr("使用网易云音乐 App 扫码登录"))
                      : (accounts.kugouLoggedIn ? qsTr("已登录：%1").arg(accounts.kugouNickname)
                                                : qsTr("使用酷狗音乐 App 扫码登录"))
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.family: Theme.fontFamily
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 200
                height: 200
                radius: Theme.radiusSmall
                color: "white"
                border.color: Theme.divider

                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    source: accounts.qrImage
                    fillMode: Image.PreserveAspectFit
                    visible: accounts.qrImage.length > 0
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: accounts.qrImage.length === 0 && accounts.busy
                    visible: running
                }

                Label {
                    anchors.centerIn: parent
                    visible: !accounts.busy && accounts.qrImage.length === 0
                    text: accounts.loginMessage
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    width: parent.width - 20
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }

            Label {
                Layout.fillWidth: true
                text: accounts.loginMessage
                color: accounts.loginStatus === "error" ? Theme.danger : Theme.textMuted
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FlatButton {
                    text: qsTr("刷新二维码")
                    onClicked: dialog.startLogin()
                }

                Item { Layout.fillWidth: true }

                FlatButton {
                    visible: dialog.platform === "netease" ? accounts.neteaseLoggedIn : accounts.kugouLoggedIn
                    text: qsTr("退出登录")
                    onClicked: {
                        accounts.logout(dialog.platform)
                        dialog.close()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.divider
            }

            Label {
                text: qsTr("或使用账号密码登录")
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
            }

            TextField {
                id: accountField
                Layout.fillWidth: true
                placeholderText: dialog.platform === "netease" ? qsTr("手机号") : qsTr("酷狗账号")
                font.family: Theme.fontFamily
                selectByMouse: true
            }

            TextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: qsTr("密码")
                echoMode: TextInput.Password
                font.family: Theme.fontFamily
                selectByMouse: true
                onAccepted: loginButton.clicked()
            }

            Button {
                id: loginButton
                Layout.fillWidth: true
                text: qsTr("登录")
                enabled: accountField.text.length > 0 && passwordField.text.length > 0
                font.family: Theme.fontFamily
                onClicked: accounts.loginWithPassword(dialog.platform, accountField.text, passwordField.text)
            }
        }
    }

    Connections {
        target: accounts
        function onStateChanged() {
            if ((dialog.platform === "netease" && accounts.neteaseLoggedIn)
                    || (dialog.platform === "kugou" && accounts.kugouLoggedIn)) {
                closeTimer.start()
            }
        }
    }

    Timer {
        id: closeTimer
        interval: 900
        onTriggered: dialog.close()
    }
}
