import QtQuick
import QtQuick.Layouts
import "../base"
import LAStudio

Item {
    id: root

    required property string stepId
    required property string title
    property string iconName: "check"
    property bool complete: false
    property bool active: false
    signal selected(string stepId)

    implicitWidth: stepRow.implicitWidth
    implicitHeight: 32

    RowLayout {
        id: stepRow
        anchors.fill: parent
        spacing: 6
        Rectangle {
            Layout.preferredWidth: 25
            Layout.preferredHeight: 25
            radius: 13
            color: root.active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20) : "transparent"
            border.color: root.complete || root.active ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.16)
            border.width: 1
            LineIcon { anchors.centerIn: parent; width: 13; height: 13; name: root.complete ? "check" : root.iconName; color: root.complete || root.active ? Theme.accentLight : Theme.textSecondary }
        }
        Text { text: root.title; color: root.active ? Theme.textPrimary : Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: root.active || root.complete }
    }

    TapHandler { onTapped: root.selected(root.stepId) }
    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
