import QtQuick
import QtQuick.Controls
import LAStudio
import "../base"

Button {
    id: root
    property string iconName: "play"
    property string toolTip: ""
    implicitWidth: 34
    implicitHeight: 34
    padding: 0
    contentItem: Item {
        LineIcon {
            anchors.centerIn: parent
            name: root.iconName
            color: root.enabled ? (root.highlighted ? Theme.textPrimary : Theme.textSecondary) : Qt.rgba(0.56, 0.56, 0.69, 0.38)
            width: 15
            height: 15
        }
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.highlighted ? Theme.accent : (root.hovered ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.12))
        border.color: root.highlighted ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }
    ToolTip.visible: hovered && toolTip !== ""
    ToolTip.delay: 350
    ToolTip.text: toolTip
}
