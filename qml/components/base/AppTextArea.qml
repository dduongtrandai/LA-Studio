import QtQuick
import QtQuick.Controls
import LAStudio

TextArea {
    id: root

    color: Theme.textPrimary
    wrapMode: Text.Wrap
    placeholderTextColor: Theme.textSecondary
    padding: Theme.paddingMedium
    font.pixelSize: Theme.fontMedium

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surface
        border.color: Theme.surfaceAlt
        border.width: 1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: Theme.accent
            border.width: root.activeFocus ? 1 : 0
        }
    }

    palette {
        highlight: Theme.accent
        highlightedText: "#ffffff"
    }
}
