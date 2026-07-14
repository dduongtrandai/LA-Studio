import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../components/base"
import "./settings"

Rectangle {
    id: root

    color: Theme.background
    readonly property int translationRevision: AppController.localization.revision

    function getPageTitle(index) {
        root.translationRevision
        switch(index) {
            case 0: return qsTr("General Settings")
            case 1: return qsTr("Hardware")
            default: return qsTr("Settings")
        }
    }

    function getPageDescription(index) {
        root.translationRevision
        switch(index) {
            case 0: return qsTr("Configure general options, download path, and view application metadata.")
            case 1: return qsTr("CPU, memory, GPU, and model loading limits.")
            default: return ""
        }
    }

    function settingsRoutes() {
        root.translationRevision
        return [
            { label: qsTr("General"), iconName: "settings", index: 0 }
        ]
    }

    function systemRoutes() {
        root.translationRevision
        return [
            { label: qsTr("Hardware"), iconName: "cpu", index: 1 }
        ]
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Theme.sidebarWidth
            color: Qt.darker(Theme.background, 1.01)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: 4

                Text {
                    text: qsTr("SETTINGS")
                    color: Theme.accentLight
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.0
                    Layout.leftMargin: Theme.paddingSmall
                    Layout.topMargin: Theme.paddingMedium
                    Layout.bottomMargin: Theme.paddingSmall
                }

                Repeater {
                    model: root.settingsRoutes()
                    delegate: sidebarItemDelegate
                }

                Item { Layout.preferredHeight: Theme.paddingSmall }

                Text {
                    text: qsTr("SYSTEM")
                    color: Theme.accentLight
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.0
                    Layout.leftMargin: Theme.paddingSmall
                    Layout.topMargin: Theme.paddingSmall
                    Layout.bottomMargin: Theme.paddingSmall
                }

                Repeater {
                    model: root.systemRoutes()
                    delegate: sidebarItemDelegate
                }

                Item { Layout.fillHeight: true }
            }

            Rectangle {
                anchors.right: parent.right
                height: parent.height
                width: 1
                color: Theme.surfaceAlt
                opacity: 0.5
            }
        }

        Item {
            id: contentAreaContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingXL
                spacing: Theme.paddingLarge

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: root.getPageTitle(settingsStack.currentIndex)
                        color: Theme.textPrimary
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Text {
                        text: root.getPageDescription(settingsStack.currentIndex)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.surfaceAlt
                    opacity: 0.3
                    Layout.bottomMargin: Theme.paddingSmall
                }

                StackLayout {
                    id: settingsStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: 0

                    GeneralSettingsTab {}
                    HardwareSettingsTab {}
                }
            }
        }
    }

    Component {
        id: sidebarItemDelegate

        Item {
            id: delegateItem

            required property var modelData

            Layout.fillWidth: true
            Layout.preferredHeight: 38

            property bool isSelected: settingsStack.currentIndex === modelData.index

            Rectangle {
                anchors.centerIn: parent
                width: parent.width - 4
                height: parent.height
                radius: 7
                color: {
                    if (delegateItem.isSelected) {
                        return Qt.rgba(0.49, 0.30, 1.0, 0.14)
                    }
                    if (mouseArea.containsMouse) {
                        return Qt.rgba(255, 255, 255, 0.04)
                    }
                    return "transparent"
                }
                border.color: delegateItem.isSelected ? Qt.rgba(0.49, 0.30, 1.0, 0.35) : "transparent"
                border.width: 1

                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: delegateItem.isSelected ? 18 : 0
                radius: 1.5
                color: Theme.accentLight

                Behavior on height {
                    NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium + 4
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium

                LineIcon {
                    name: delegateItem.modelData.iconName
                    color: delegateItem.isSelected ? Theme.accentLight : (mouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    strokeWidth: 1.8

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Text {
                    text: delegateItem.modelData.label
                    color: delegateItem.isSelected ? Theme.textPrimary : (mouseArea.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                    font.pixelSize: Theme.fontMedium
                    font.bold: delegateItem.isSelected
                    Layout.fillWidth: true

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: settingsStack.currentIndex = delegateItem.modelData.index
            }
        }
    }
}
