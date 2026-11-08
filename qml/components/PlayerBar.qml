import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MrMusic

/// Playback bar: progress, transport controls, volume and quality selection.
Rectangle {
    id: bar

    property bool compact: false

    signal videoRequested()

    color: Theme.surface
    implicitHeight: compact ? 72 : 86

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.divider
    }

    // thin progress line on mobile
    Rectangle {
        visible: bar.compact
        anchors.top: parent.top
        width: bar.width * Math.max(0, Math.min(1, player.duration > 0 ? player.position / player.duration : 0))
        height: 2
        color: Theme.accent
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: bar.compact ? 12 : 20
        anchors.rightMargin: bar.compact ? 12 : 20
        spacing: bar.compact ? 10 : 18

        // ---- now playing -------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: bar.compact ? 9999 : 260
            spacing: 10

            Rectangle {
                Layout.preferredWidth: bar.compact ? 40 : 52
                Layout.preferredHeight: bar.compact ? 40 : 52
                radius: Theme.radiusSmall
                color: Theme.surfaceAlt
                clip: true

                Image {
                    id: nowCover
                    anchors.fill: parent
                    source: player.coverUrl
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                TintedIcon {
                    anchors.centerIn: parent
                    width: parent.width * 0.4
                    height: width
                    source: "qrc:/res/note.svg"
                    color: Theme.textFaint
                    visible: nowCover.status !== Image.Ready
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.showQueue()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    text: player.hasTrack ? player.title : qsTr("未在播放")
                    color: Theme.text
                    font.pixelSize: bar.compact ? 13 : 14
                    font.bold: true
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: player.hasTrack
                          ? (player.statusMessage.length > 0 ? player.statusMessage
                                                             : player.artists + (player.sourceName.length > 0 ? " · " + player.sourceName : ""))
                          : qsTr("从搜索或每日推荐开始")
                    color: player.statusMessage.length > 0 ? Theme.danger : Theme.textMuted
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    visible: !bar.compact && player.loading
                    text: qsTr("正在解析播放地址…")
                    color: Theme.accent
                    font.pixelSize: Theme.fontCaption
                }
            }
        }

        // ---- transport ---------------------------------------------------
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: bar.compact ? 4 : 8

            IconButton {
                icon: player.playMode === 0 ? "repeat" : (player.playMode === 1 ? "repeat"
                                                          : (player.playMode === 2 ? "repeat-one" : "shuffle"))
                size: 30
                iconSize: 16
                visible: !bar.compact
                tooltip: qsTr("播放模式：") + (player.playMode === 0 ? qsTr("顺序播放")
                                              : player.playMode === 1 ? qsTr("列表循环")
                                              : player.playMode === 2 ? qsTr("单曲循环") : qsTr("随机播放"))
                onClicked: player.cyclePlayMode()
            }

            IconButton {
                icon: "prev"
                size: 34
                iconSize: 18
                onClicked: player.previous()
            }

            Rectangle {
                width: bar.compact ? 38 : 44
                height: width
                radius: width / 2
                color: Theme.accent

                TintedIcon {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    source: player.playing ? "qrc:/res/pause.svg" : "qrc:/res/play.svg"
                    color: "white"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: player.togglePlay()
                }
            }

            IconButton {
                icon: "next"
                size: 34
                iconSize: 18
                onClicked: player.next()
            }
        }

        // ---- progress ----------------------------------------------------
        ColumnLayout {
            visible: !bar.compact
            Layout.fillWidth: true
            Layout.minimumWidth: 180
            spacing: 0

            Slider {
                id: progress
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, player.duration)
                value: player.position
                enabled: player.duration > 0
                onMoved: player.seek(Math.round(value))

                background: Rectangle {
                    x: progress.leftPadding
                    y: progress.topPadding + progress.availableHeight / 2 - height / 2
                    width: progress.availableWidth
                    height: 4
                    radius: 2
                    color: Theme.surfaceAlt

                    Rectangle {
                        width: progress.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                    }
                }

                handle: Rectangle {
                    x: progress.leftPadding + progress.visualPosition * (progress.availableWidth - width)
                    y: progress.topPadding + progress.availableHeight / 2 - height / 2
                    width: 12
                    height: 12
                    radius: 6
                    color: Theme.accent
                    visible: progress.pressed || progress.hovered
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: player.positionText
                    color: Theme.textMuted
                    font.pixelSize: 10
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: player.durationText
                    color: Theme.textMuted
                    font.pixelSize: 10
                }
            }
        }

        // ---- right side ---------------------------------------------------
        RowLayout {
            visible: !bar.compact
            spacing: 6

            IconButton {
                icon: player.volume > 0.001 ? "volume" : "volume-mute"
                size: 30
                iconSize: 16
                onClicked: player.volume = player.volume > 0.001 ? 0 : 0.8
            }

            Slider {
                Layout.preferredWidth: 90
                from: 0
                to: 1
                value: player.volume
                onMoved: player.volume = value

                background: Rectangle {
                    x: parent.leftPadding
                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                    width: parent.availableWidth
                    height: 4
                    radius: 2
                    color: Theme.surfaceAlt

                    Rectangle {
                        width: parent.parent.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                    }
                }

                handle: Rectangle {
                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                    width: 10
                    height: 10
                    radius: 5
                    color: Theme.accent
                }
            }

            Button {
                id: videoButton
                visible: player.videoMode
                text: "MV"
                flat: true
                font.pixelSize: 11
                font.family: Theme.fontFamily
                padding: 6

                contentItem: Label {
                    text: videoButton.text
                    color: Theme.accent
                    font: videoButton.font
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: videoButton.hovered ? Theme.hover : "transparent"
                }

                onClicked: playerBar.videoRequested()
            }

            Button {
                id: qualityButton
                text: ["标准", "高品", "无损", "Hi-Res"][settings.playbackQuality]
                flat: true
                font.pixelSize: 11
                font.family: Theme.fontFamily
                padding: 6

                contentItem: Label {
                    text: qualityButton.text
                    color: Theme.textMuted
                    font: qualityButton.font
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: qualityButton.hovered ? Theme.hover : "transparent"
                }

                onClicked: qualityMenu.open()
                Menu {
                    id: qualityMenu
                    y: -height
                    MenuItem {
                        text: qsTr("标准 (128K)")
                        checkable: true
                        checked: settings.playbackQuality === 0
                        onTriggered: settings.playbackQuality = 0
                    }
                    MenuItem {
                        text: qsTr("高品 (320K)")
                        checkable: true
                        checked: settings.playbackQuality === 1
                        onTriggered: settings.playbackQuality = 1
                    }
                    MenuItem {
                        text: qsTr("无损")
                        checkable: true
                        checked: settings.playbackQuality === 2
                        onTriggered: settings.playbackQuality = 2
                    }
                    MenuItem {
                        text: qsTr("Hi-Res")
                        checkable: true
                        checked: settings.playbackQuality === 3
                        onTriggered: settings.playbackQuality = 3
                    }
                }
            }

            IconButton {
                icon: "download"
                size: 34
                tooltip: qsTr("下载当前歌曲")
                enabled: player.hasTrack
                onClicked: downloads.enqueue(player.hasTrack ? currentTrackMap() : ({}))
            }
        }

        IconButton {
            visible: bar.compact
            icon: "queue"
            size: 32
            iconSize: 16
            onClicked: window.showQueue()
        }
    }
}
