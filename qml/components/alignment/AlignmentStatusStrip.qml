import QtQuick
import QtQuick.Layouts
import LAStudio
import "../base"

Rectangle {
    id: root
    property string label: ""
    property string detail: ""
    property string iconName: "activity"
    property color accent: Theme.textSecondary

    implicitHeight: row.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusSmall
    color: Qt.rgba(accent.r, accent.g, accent.b, 0.07)
    border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.24)
    border.width: 1

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium
        LineIcon {
            name: root.iconName
            color: root.accent
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
        }
        Text {
            text: root.label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 14
            color: Qt.rgba(1, 1, 1, 0.10)
        }
        Text {
            Layout.fillWidth: true
            text: root.detail
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
        }
    }
}
