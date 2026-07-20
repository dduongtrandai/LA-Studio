import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Button {
    id: root

    property color buttonColor: Theme.accent
    property color textColor: "#ffffff"
    property bool loading: false
    property string iconName: ""
    property bool iconOnly: false
    property string toolTip: ""
    property color borderColor: Qt.rgba(1, 1, 1, 0.08)
    property bool quiet: false
    readonly property real requiredContentWidth:
        root.iconOnly ? 38
                      : buttonLabel.implicitWidth
                        + Theme.paddingSmall * 2
                        + (buttonIcon.visible
                           ? (buttonIcon.width + Theme.paddingSmall) * 2
                           : 0)

    implicitWidth: root.iconOnly ? 38 : Math.max(120, root.requiredContentWidth)
    implicitHeight: 38
    Layout.minimumWidth: root.requiredContentWidth

    AppToolTip {
        text: root.toolTip
        visible: root.hovered && root.toolTip !== ""
    }

    contentItem: Item {
        opacity: root.loading ? 0 : 1

        LineIcon {
            id: buttonIcon
            visible: root.iconName !== ""
            name: root.iconName
            color: root.enabled ? root.textColor : Theme.textSecondary
            width: 16
            height: 16
            anchors.verticalCenter: root.iconOnly ? undefined : parent.verticalCenter
            anchors.centerIn: root.iconOnly ? parent : undefined
            anchors.right: buttonLabel.left
            anchors.rightMargin: root.iconOnly ? 0 : Theme.paddingSmall
        }

        Text {
            id: buttonLabel
            visible: !root.iconOnly
            text: root.text
            color: root.enabled ? root.textColor : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: true
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Item {
            visible: root.iconOnly
            anchors.fill: parent
        }
    }

    background: Rectangle {
        implicitWidth: root.iconOnly ? 38 : 120
        implicitHeight: 38
        radius: 7
        color: {
            if (!root.enabled) return Theme.surfaceAlt
            if (root.quiet) {
                if (root.pressed) return Qt.rgba(1, 1, 1, 0.10)
                if (root.hovered) return Qt.rgba(1, 1, 1, 0.07)
                return Theme.surface
            }
            if (root.pressed) return Qt.darker(root.buttonColor, 1.12)
            if (root.hovered) return Qt.lighter(root.buttonColor, 1.08)
            return root.buttonColor
        }
        border.color: root.enabled ? (root.quiet ? root.borderColor : Qt.rgba(1, 1, 1, 0.10)) : "transparent"
        border.width: 1

        // Loading spinner
        BusyIndicator {
            anchors.centerIn: parent
            running: root.loading
            visible: root.loading
            width: 24
            height: 24
            palette.dark: root.textColor
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }
}

