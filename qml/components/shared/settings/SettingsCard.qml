import QtQuick
import QtQuick.Layouts
import LAStudio
import "../../base"

Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string iconName: ""
    property bool hoverEnabled: false
    
    default property alias content: contentLayout.data

    Layout.fillWidth: true
    implicitHeight: mainLayout.implicitHeight
    radius: Theme.radiusMedium
    color: hoverHandler.hovered ? Qt.rgba(255, 255, 255, 0.015) : Qt.rgba(255, 255, 255, 0.01)
    border.color: hoverHandler.hovered ? Qt.rgba(124, 77, 255, 0.2) : Qt.rgba(255, 255, 255, 0.04)
    border.width: 1

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.paddingXL
        spacing: Theme.paddingLarge

        // Header Row (Optional, only visible if title is set)
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingMedium
            visible: root.title !== ""

            // Small glowing icon container
            Rectangle {
                width: 32
                height: 32
                radius: Theme.radiusSmall
                color: Qt.rgba(124, 77, 255, 0.1)
                border.color: Qt.rgba(124, 77, 255, 0.25)
                border.width: 1
                visible: root.iconName !== ""

                LineIcon {
                    anchors.centerIn: parent
                    name: root.iconName
                    color: Theme.accentLight
                    width: 16
                    height: 16
                    strokeWidth: 1.8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                
                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                
                Text {
                    text: root.description
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    visible: root.description !== ""
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
            opacity: 0.3
            visible: root.title !== "" && contentLayout.children.length > 0
        }

        // Content Layout
        ColumnLayout {
            id: contentLayout
            Layout.fillWidth: true
            spacing: Theme.paddingMedium
        }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.hoverEnabled
    }
}
