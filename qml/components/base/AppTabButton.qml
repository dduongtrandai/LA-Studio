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

    implicitWidth: tabText.implicitWidth + Theme.paddingMedium * 2 + (tabIcon.visible || tabEmoji.visible ? 20 : 0)
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

    Item {
        id: tabLayout
        anchors.fill: parent

        LineIcon {
            id: tabIcon
            name: root.iconName
            color: root.selected ? Theme.accentLight : Theme.textSecondary
            width: 14
            height: 14
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: tabText.left
            anchors.rightMargin: 6
            visible: root.iconName !== ""
        }

        Text {
            id: tabEmoji
            text: root.icon
            font.pixelSize: Theme.fontMedium
            visible: root.icon !== "" && root.iconName === ""
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: tabText.left
            anchors.rightMargin: 6
        }

        Text {
            id: tabText
            anchors.centerIn: parent
            width: Math.max(0, Math.min(implicitWidth, parent.width - (tabIcon.visible || tabEmoji.visible ? 20 : 0)))
            text: root.text
            color: selected ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: selected
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
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
