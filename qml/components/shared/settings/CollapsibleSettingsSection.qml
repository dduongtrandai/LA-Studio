import QtQuick
import QtQuick.Layouts
import LAStudio
import "../../base"

ColumnLayout {
    id: root

    property string title: ""
    property string iconName: "sliders"
    property bool expanded: false
    default property alias content: contentColumn.data

    signal toggled()

    Layout.fillWidth: true
    spacing: Theme.paddingSmall

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: headerRow.implicitHeight + Theme.paddingSmall
        radius: Theme.radiusSmall
        color: Qt.rgba(1, 1, 1, 0.025)
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1

        RowLayout {
            id: headerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.paddingSmall
            anchors.rightMargin: Theme.paddingSmall
            spacing: Theme.paddingSmall

            LineIcon {
                name: root.iconName
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }

            Text {
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
            }

            LineIcon {
                name: root.expanded ? "arrow-down" : "chevron-right"
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.toggled()
        }
    }

    ColumnLayout {
        id: contentColumn
        Layout.fillWidth: true
        spacing: Theme.paddingMedium
        visible: root.expanded
    }
}
