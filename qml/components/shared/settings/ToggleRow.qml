import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

CheckBox {
    id: toggle

    Layout.fillWidth: true
    implicitHeight: 34
    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 20
        x: toggle.width - width
        y: parent.height / 2 - height / 2
        radius: 10
        color: toggle.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.22) : Qt.rgba(1, 1, 1, 0.04)
        border.color: toggle.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.85) : Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Rectangle {
            width: 14
            height: 14
            radius: 7
            x: toggle.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: toggle.checked ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.42)

            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }

    contentItem: Text {
        text: toggle.text
        color: toggle.enabled ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        verticalAlignment: Text.AlignVCenter
        rightPadding: toggle.indicator.width + Theme.paddingMedium
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
