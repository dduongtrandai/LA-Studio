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
    property color borderColor: Qt.rgba(1, 1, 1, 0.08)
    property bool quiet: false

    contentItem: RowLayout {
        spacing: Theme.paddingSmall
        opacity: root.loading ? 0 : 1

        Item { Layout.fillWidth: true }

        LineIcon {
            visible: root.iconName !== ""
            name: root.iconName
            color: root.enabled ? root.textColor : Theme.textSecondary
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
        }

        Text {
            text: root.text
            color: root.enabled ? root.textColor : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Item { Layout.fillWidth: true }
    }

    background: Rectangle {
        implicitWidth: 120
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

