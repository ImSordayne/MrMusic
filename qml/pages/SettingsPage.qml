import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import MrMusic

/// Settings: quality, download folder, storage mode, proxy and theme.
Item {
    id: page

    property bool compact: false

    Platform.FolderDialog {
        id: folderDialog
        title: qsTr("选择下载目录")
        folder: "file:///" + settings.downloadDir.replace(/\\/g, "/")
        onAccepted: settings.downloadDir = String(folder).replace("file:///", "").replace(/\//g, "\\")
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: page.width - Theme.pagePadding * 2
            spacing: 14

            Label {
                text: qsTr("设置")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
                font.family: Theme.fontFamily
            }

            // ---- playback ------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: playbackColumn.implicitHeight + 28
                radius: Theme.radius
                color: Theme.surface

                ColumnLayout {
                    id: playbackColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Label {
                        text: qsTr("播放")
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                        font.family: Theme.fontFamily
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label {
                            text: qsTr("音质")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            Layout.preferredWidth: 180
                            model: [qsTr("标准 (128K)"), qsTr("高品 (320K)"), qsTr("无损"), qsTr("Hi-Res")]
                            currentIndex: settings.playbackQuality
                            font.family: Theme.fontFamily
                            onActivated: settings.playbackQuality = index
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("无版权或 VIP 受限的歌曲会明确提示，不会自动降级音质。")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ---- download -------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: downloadColumn.implicitHeight + 28
                radius: Theme.radius
                color: Theme.surface

                ColumnLayout {
                    id: downloadColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Label {
                        text: qsTr("下载")
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                        font.family: Theme.fontFamily
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: qsTr("下载目录")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }

                        Label {
                            Layout.fillWidth: true
                            text: settings.downloadDir
                            color: Theme.text
                            font.pixelSize: Theme.fontCaption
                            font.family: Theme.fontFamily
                            elide: Text.ElideMiddle
                        }

                        FlatButton {
                            text: qsTr("选择…")
                            onClicked: folderDialog.open()
                        }

                        FlatButton {
                            text: qsTr("打开")
                            onClicked: downloads.openFolder()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("文件命名：作者-专辑-音质-平台.扩展名；每个平台最多同时下载 5 首。")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ---- storage --------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: storageColumn.implicitHeight + 28
                radius: Theme.radius
                color: Theme.surface

                ColumnLayout {
                    id: storageColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Label {
                        text: qsTr("账号与数据")
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                        font.family: Theme.fontFamily
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("明文存储登录信息")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                        Switch {
                            checked: settings.plaintextStorage
                            onToggled: {
                                settings.plaintextStorage = checked
                                accounts.applyPlaintextStorage(checked)
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("默认使用系统指纹派生的密钥加密保存 token / cookie；开启此项后将改为明文 JSON。")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("本机设备标识")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                        Label {
                            text: accounts.deviceFingerprint
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontCaption
                            font.family: Theme.fontFamily
                            elide: Text.ElideMiddle
                            Layout.maximumWidth: 220
                        }
                    }
                }
            }

            // ---- network & look ------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: networkColumn.implicitHeight + 28
                radius: Theme.radius
                color: Theme.surface

                ColumnLayout {
                    id: networkColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Label {
                        text: qsTr("网络与外观")
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                        font.family: Theme.fontFamily
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("使用代理")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                        Switch {
                            checked: settings.proxyEnabled
                            onToggled: settings.proxyEnabled = checked
                        }
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: settings.proxyUrl
                        enabled: settings.proxyEnabled
                        placeholderText: qsTr("http://127.0.0.1:7890")
                        font.family: Theme.fontFamily
                        selectByMouse: true
                        onEditingFinished: settings.proxyUrl = text
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("关闭窗口时")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                        ComboBox {
                            Layout.preferredWidth: 180
                            model: [qsTr("直接关闭"), qsTr("在托盘运行")]
                            currentIndex: settings.closeAction
                            font.family: Theme.fontFamily
                            onActivated: settings.closeAction = index
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("选择「在托盘运行」后，关闭窗口会隐藏到托盘（Linux 无托盘时在后台继续运行），音乐不会中断。")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontCaption
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("主题")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                        ComboBox {
                            Layout.preferredWidth: 180
                            model: [qsTr("跟随系统"), qsTr("浅色"), qsTr("深色")]
                            currentIndex: settings.themeMode
                            font.family: Theme.fontFamily
                            onActivated: settings.themeMode = index
                        }
                    }
                }
            }

            // ---- about ----------------------------------------------------
            Label {
                Layout.fillWidth: true
                text: qsTr("MrMusic %1 · Qt %2 · 数据目录：%3").arg(app.version).arg(Qt.version).arg(app.dataDir)
                color: Theme.textFaint
                font.pixelSize: Theme.fontCaption
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
        }
    }
}
