import QtQuick
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property string text: "Tab"
    property string icon: ""
    property string iconName: ""
    property bool selected: false
    property bool enabled: true

    signal clicked()

    implicitWidth: tabLayout.implicitWidth + Theme.paddingMedium * 2
    implicitHeight: 34
    radius: 7
    color: {
        if (selected) return Qt.rgba(0.49, 0.30, 1.0, 0.18)
        if (hoverHandler.hovered && root.enabled) return Qt.rgba(1, 1, 1, 0.055)
        return "transparent"
    }
    border.color: selected ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.07)
    border.width: 1
    opacity: enabled ? 1.0 : 0.6

    RowLayout {
        id: tabLayout
        anchors.centerIn: parent
        spacing: 6

        LineIcon {
            name: root.iconName
            color: root.selected ? Theme.accentLight : Theme.textSecondary
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            visible: root.iconName !== ""
        }

        Text {
            text: root.icon
            font.pixelSize: Theme.fontMedium
            visible: root.icon !== "" && root.iconName === ""
        }

        Text {
            text: root.text
            color: selected ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: selected
        }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: {
            if (root.enabled) {
                root.clicked()
            }
        }
    }

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }
}
