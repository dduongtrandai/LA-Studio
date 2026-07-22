import QtQuick
import QtQuick.Controls
import LAStudio

SpinBox {
    id: root

    implicitWidth: 124
    implicitHeight: 40
    editable: true
    clip: true

    contentItem: TextInput {
        z: 2
        text: root.displayText
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        selectionColor: Theme.accent
        selectedTextColor: Theme.textPrimary
        font.pixelSize: Theme.fontSmall
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        leftPadding: root.down.indicator.width + Theme.paddingSmall
        rightPadding: root.up.indicator.width + Theme.paddingSmall
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: root.width - width
        implicitWidth: 38
        implicitHeight: root.height
        height: root.height
        color: !root.enabled ? "transparent"
                             : (root.up.pressed
                                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                                : (root.up.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent"))

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - Theme.paddingMedium
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        LineIcon {
            anchors.centerIn: parent
            width: 14
            height: 14
            name: "plus"
            color: root.enabled
                   ? (root.up.hovered ? Theme.accentLight : Theme.textPrimary)
                   : Theme.textSecondary
        }

        HoverHandler { cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }

    down.indicator: Rectangle {
        x: 0
        implicitWidth: 38
        implicitHeight: root.height
        height: root.height
        color: !root.enabled ? "transparent"
                             : (root.down.pressed
                                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                                : (root.down.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent"))

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height - Theme.paddingMedium
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        LineIcon {
            anchors.centerIn: parent
            width: 14
            height: 14
            name: "minus"
            color: root.enabled
                   ? (root.down.hovered ? Theme.accentLight : Theme.textPrimary)
                   : Theme.textSecondary
        }

        HoverHandler { cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.enabled ? Theme.surfaceAlt : Qt.rgba(1, 1, 1, 0.025)
        border.color: root.activeFocus
                      ? Theme.accent
                      : (root.hovered ? Qt.rgba(1, 1, 1, 0.14)
                                      : Qt.rgba(1, 1, 1, 0.08))
        border.width: 1
    }
}
