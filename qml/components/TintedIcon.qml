import QtQuick

/// SVG icon painted in an arbitrary colour.
///
/// Rendering goes through the C++ `image://mricon` provider, which substitutes
/// the `currentColor` of the SVG *before* rasterising it. This deliberately does
/// not use `MultiEffect` / shader based tinting, which silently produced black
/// icons in some configurations (TARGET.md §9.12).
Item {
    id: root

    property string source: ""
    property color color: "#000000"
    property int iconSize: 20

    readonly property string iconName: {
        if (!source)
            return ""
        var name = source
        var slash = name.lastIndexOf("/")
        if (slash >= 0)
            name = name.substring(slash + 1)
        if (name.endsWith(".svg"))
            name = name.substring(0, name.length - 4)
        return name
    }

    readonly property string tintedSource: iconName.length === 0
        ? ""
        : "image://mricon/" + iconName
          + "?c=" + encodeURIComponent(root.color.toString())
          + "&s=" + Math.round(Math.max(16, root.width * 2))

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        anchors.fill: parent
        source: root.tintedSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: true
        asynchronous: false
    }
}
