pragma Singleton
import QtQuick

QtObject {
    signal requestShowDownloads()

    readonly property color background:  "#1e1e2e"
    readonly property color surface:     "#2a2a3e"
    readonly property color surfaceAlt:  "#35354a"
    readonly property color accent:      "#7c4dff"
    readonly property color accentLight: "#a27eff"
    readonly property color textPrimary: "#e0e0f0"
    readonly property color textSecondary: "#9090b0"
    readonly property color danger:      "#ef5350"
    readonly property color success:     "#66bb6a"
    readonly property color warning:     "#ffa726"

    readonly property int radiusSmall:  8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge:  16

    readonly property int paddingSmall:  8
    readonly property int paddingMedium: 12
    readonly property int paddingLarge:  16
    readonly property int paddingXL:     24

    readonly property int fontSmall:  12
    readonly property int fontMedium: 14
    readonly property int fontLarge:  18
    readonly property int fontTitle:  24

    readonly property int sidebarWidth: 220
    readonly property int iconSize:    20
}
