import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtQuick.Layouts
import Qt.labs.platform as Platform
import MrMusic

ApplicationWindow {
    id: window

    width: 1180
    height: 760
    minimumWidth: 380
    minimumHeight: 500
    visible: true
    title: qsTr("MrMusic")
    color: Theme.background

    property bool compact: width < 760
    property int currentPage: 0        // 0 home, 1 history, 2 favorites, 3 settings
    property bool searchMode: false
    property bool showWarning: accounts.storageWarning.length > 0
    property bool quitting: false

    function navigate(page) {
        searchMode = false
        currentPage = page
    }

    onSearchModeChanged: console.log("[mrmusic] searchMode -> " + searchMode)

    /// Leaves the search page and returns to the page the user came from.
    function leaveSearch() {
        searchMode = false
    }

    /// Automation helper: `--open-first album|artist` opens the first catalogue
    /// result once the search finishes (used for screenshots / smoke tests).
    property string autoOpen: ""
    property bool autoOpenDone: false

    function maybeAutoOpen() {
        if (autoOpenDone || autoOpen.length === 0 || app.searching)
            return

        if (autoOpen === "album" && app.albumResults.count > 0) {
            autoOpenDone = true
            browse.openAlbum(app.albumResults.get(0).source, app.albumResults.get(0).id)
        } else if (autoOpen === "mv" && app.mvResults.count > 0) {
            autoOpenDone = true
            player.playMv(app.mvResults.get(0))
        } else if (autoOpen === "artist" && app.searchResults.count > 0
                   && app.searchResults.at(0).artistId.length > 0) {
            autoOpenDone = true
            browse.openArtist(app.searchResults.at(0).source, app.searchResults.at(0).artistId)
        }
    }

    Connections {
        target: app
        function onSearchingChanged() { window.maybeAutoOpen() }
    }

    // automation helper: show the MV window as soon as a video frame is ready
    Connections {
        target: player
        function onVideoAvailableChanged() {
            if (player.videoAvailable && window.autoOpen === "mv")
                videoPopup.open()
        }
    }

    Timer {
        id: autoOpenTimer
        interval: 1500
        repeat: true
        running: window.autoOpen.length > 0 && !window.autoOpenDone
        onTriggered: window.maybeAutoOpen()
    }

    function doSearch(text) {
        if (text.trim().length === 0) {
            searchMode = false
            return
        }
        searchMode = true
        app.search(text)
    }

    /// Brings the window back from the tray / minimised state. Also called by
    /// the C++ side when a second instance is started (TARGET.md §9.8).
    function restoreWindow() {
        window.show()
        window.raise()
        window.requestActivate()
    }

    /// Really terminates the application (tray menu / Quit action).
    function quitApp() {
        quitting = true
        trayIcon.visible = false
        app.quitApplication()
    }

    onClosing: function (close) {
        if (window.quitting) {
            close.accepted = true
            return
        }
        if (Qt.platform.os !== "android" && settings.closeAction === 1) {
            close.accepted = false
            window.hide()
            if (trayIcon.supported)
                trayIcon.showMessage(qsTr("MrMusic 仍在后台运行"), qsTr("点击托盘图标可重新打开窗口"))
        }
    }

    function showQueue() { queuePanel.open() }
    function showDownloads() { downloadsPanel.open() }
    function openLogin(platform) {
        loginDialog.platform = platform
        loginDialog.open()
    }

    function applyTheme() {
        var mode = settings.themeMode
        var dark = false
        if (mode === 2) {
            dark = true
        } else if (mode === 1) {
            dark = false
        } else {
            dark = (typeof Application !== "undefined" && Application.styleHints
                    && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)
        }
        Theme.dark = dark
    }

    Component.onCompleted: {
        applyTheme()
        player.attachWindow(window)
        app.refreshDaily()

        // developer/CI helper: start on a specific page or force the phone layout
        var args = Qt.application.arguments
        var pageIndex = args.indexOf("--page")
        if (pageIndex >= 0)
            window.currentPage = parseInt(args[pageIndex + 1])
        if (args.indexOf("--compact") >= 0)
            window.width = 430
        var openIndex = args.indexOf("--open-first")
        if (openIndex >= 0 && openIndex + 1 < args.length)
            window.autoOpen = args[openIndex + 1]

        // the search may already be finished when the UI is created
        autoOpenTimer.start()
        window.maybeAutoOpen()
        console.log("[mrmusic] args=" + args.join(" ") + " page=" + window.currentPage + " searchMode=" + window.searchMode)
    }

    Connections {
        target: settings
        function onChanged() { window.applyTheme() }
    }

    Connections {
        target: player
        function onTrackStarted(track) { library.addHistory(track) }
    }

    Connections {
        target: app
        function onSearchingChanged() {
            if (app.searching)
                window.searchMode = true
        }
    }

    Connections {
        target: library
        function onFavoritesChanged() { window.syncFavorites() }
    }

    function syncFavorites() {
        var keys = library.favoriteKeyList()
        app.setFavoriteKeys(keys)
        player.setFavoriteKeys(keys)
    }

    // ---------------------------------------------------------------- layout
    Item {
        id: root
        anchors.fill: parent

        NavRail {
            id: navRail
            visible: !window.compact
            width: Theme.navWidth
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            currentPage: window.currentPage
            onPageSelected: (page) => window.navigate(page)
            onAvatarClicked: window.openLogin(platform)
        }

        Rectangle {
            visible: !window.compact
            anchors.left: navRail.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Theme.divider
        }

        BottomNav {
            id: bottomNav
            visible: window.compact
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 58
            currentPage: window.currentPage
            onPageSelected: (page) => window.navigate(page)
        }

        TopBar {
            id: topBar
            compact: window.compact
            anchors.top: parent.top
            anchors.left: window.compact ? parent.left : navRail.right
            anchors.right: parent.right
            height: 62
            onSearchRequested: (text) => window.doSearch(text)
            onDownloadClicked: window.showDownloads()
            onQueueClicked: window.showQueue()
            onAvatarClicked: (platform) => window.openLogin(platform)
        }

        // storage warning banner
        Rectangle {
            id: warningBanner
            visible: window.showWarning
            anchors.top: topBar.bottom
            anchors.left: window.compact ? parent.left : navRail.right
            anchors.right: parent.right
            height: visible ? 40 : 0
            color: "#FFF4E5"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: accounts.storageWarning
                    color: "#8A5300"
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    elide: Text.ElideRight
                }

                Button {
                    text: qsTr("重新登录")
                    flat: true
                    font.pixelSize: Theme.fontCaption
                    font.family: Theme.fontFamily
                    onClicked: {
                        accounts.dismissStorageWarning()
                        window.openLogin("netease")
                    }
                }
            }
        }

        Item {
            id: contentArea
            anchors.top: warningBanner.visible ? warningBanner.bottom : topBar.bottom
            anchors.bottom: playerBar.top
            anchors.left: window.compact ? parent.left : navRail.right
            anchors.right: parent.right
            clip: true

            StackLayout {
                anchors.fill: parent
                currentIndex: window.searchMode ? 4 : window.currentPage

                HomePage { compact: window.compact }
                HistoryPage { compact: window.compact }
                FavoritesPage { compact: window.compact }
                SettingsPage { compact: window.compact }
                SearchPage { compact: window.compact }
            }
        }

        // --- artist / album detail pages (TARGET.md §10.20) -------------
        Item {
            id: detailArea
            anchors.top: contentArea.top
            anchors.bottom: contentArea.bottom
            anchors.left: contentArea.left
            anchors.right: contentArea.right
            visible: browse.view.length > 0
            clip: true

            Rectangle {
                anchors.fill: parent
                color: Theme.background
            }

            StackLayout {
                anchors.fill: parent
                currentIndex: browse.view === "album" ? 0 : 1

                AlbumPage { compact: window.compact }
                ArtistPage { compact: window.compact }
            }
        }

        PlayerBar {
            id: playerBar
            compact: window.compact
            onVideoRequested: videoPopup.open()
            anchors.left: window.compact ? parent.left : navRail.right
            anchors.right: parent.right
            anchors.bottom: window.compact ? bottomNav.top : parent.bottom
            height: window.compact ? 72 : 86
        }
    }

    // -------------------------------------------------------------- MV window
    Popup {
        id: videoPopup
        width: Math.min(window.width * 0.78, 1024)
        height: Math.round(width * 9 / 16) + 2
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        background: Rectangle { color: "#000000"; radius: Theme.radiusSmall }

        onOpened: player.attachVideoOutput(videoOutput)
        onClosed: player.attachVideoOutput(null)

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
        }
    }

    // ------------------------------------------------------------------ tray
    Platform.SystemTrayIcon {
        id: trayIcon
        visible: Qt.platform.os !== "android"
        icon.source: "qrc:/res/app.png"
        tooltip: qsTr("MrMusic")

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("显示主窗口")
                onTriggered: window.restoreWindow()
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                text: qsTr("退出 MrMusic")
                onTriggered: {
                    window.quitting = true
                    trayIcon.visible = false
                    app.quitApplication()
                }
            }
        }

        onActivated: function (reason) {
            if (reason === Platform.SystemTrayIcon.Trigger || reason === Platform.SystemTrayIcon.DoubleClick)
                window.restoreWindow()
        }
    }

    // ----------------------------------------------------------------- panels
    QueuePanel { id: queuePanel }
    DownloadsPanel { id: downloadsPanel }
    LoginDialog { id: loginDialog }

    Shortcut {
        sequence: "Space"
        enabled: !topBar.searchText.length
        onActivated: player.togglePlay()
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: topBar.focusSearch()
    }
}
