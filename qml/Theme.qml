pragma Singleton

import QtQuick

/// Central design tokens. The visual parameters are MrMusic's own choices
/// (TARGET.md §6 leaves colours, spacing, radii and typography to us).
QtObject {
    id: theme

    property bool dark: false

    // surfaces
    readonly property color background: dark ? "#111317" : "#F4F5F7"
    readonly property color surface: dark ? "#191C21" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#22262C" : "#EDEFF3"
    readonly property color hover: dark ? "#2A2F36" : "#E7EAF0"
    readonly property color selected: dark ? "#2C3444" : "#E4ECFF"
    readonly property color divider: dark ? "#2A2E35" : "#E4E7EC"

    // text
    readonly property color text: dark ? "#E9EBEE" : "#1A1D21"
    readonly property color textMuted: dark ? "#98A0A8" : "#6C7480"
    readonly property color textFaint: dark ? "#6E767F" : "#9AA1AB"
    /// Navigation icons: white on dark backgrounds, black on light ones (TARGET.md §9.11).
    readonly property color iconStrong: dark ? "#FFFFFF" : "#000000"

    // accents
    readonly property color accent: dark ? "#6FA8FF" : "#2F6BFF"
    readonly property color accentSoft: dark ? "#1E2C42" : "#E6EEFF"
    readonly property color danger: "#E5484D"
    readonly property color netease: "#E8453C"
    readonly property color kugou: "#2F7BFF"

    // metrics
    readonly property int radius: 12
    readonly property int radiusSmall: 8
    readonly property int pagePadding: 22
    readonly property int rowHeight: 68
    readonly property int iconSize: 20
    readonly property int navWidth: 104

    // typography
    readonly property int fontTitle: 15
    readonly property int fontBody: 13
    readonly property int fontCaption: 11
    readonly property string fontFamily: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : ""

    function sourceColor(name) {
        if (name === "网易云" || name === "netease")
            return theme.netease
        if (name === "酷狗" || name === "kugou")
            return theme.kugou
        return theme.textMuted
    }
}
