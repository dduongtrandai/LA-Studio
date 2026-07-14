import QtQuick
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property color accent: Theme.accentLight

    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + Theme.paddingMedium
    radius: Theme.radiusSmall
    color: Qt.rgba(1, 1, 1, 0.035)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.paddingSmall
        spacing: Theme.paddingSmall
        Rectangle { Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 4; color: root.accent }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text { Layout.fillWidth: true; text: root.label; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
            Text { Layout.fillWidth: true; text: root.value; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true; elide: Text.ElideRight }
        }
    }
}
