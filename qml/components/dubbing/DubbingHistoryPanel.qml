pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Rectangle {
    id: root

    required property var dubbing
    property bool expanded: true

    signal clearRequested()
    signal deleteRequested(string historyId)
    signal projectOpened()

    Layout.preferredWidth: 300
    Layout.fillHeight: true
    visible: root.expanded
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    clip: true

    Item {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.paddingSmall

            RowLayout {
                Layout.fillWidth: true
                LineIcon { name: "history"; color: Theme.accent; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
                Text {
                    text: qsTr("Dubbing History")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                PrimaryButton {
                    visible: root.dubbing.history.length > 0
                    text: qsTr("Clear")
                    iconName: "trash"
                    quiet: true
                    textColor: Theme.danger
                    borderColor: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.38)
                    implicitWidth: 72
                    implicitHeight: 30
                    onClicked: root.clearRequested()
                }
                Button {
                    implicitWidth: 30
                    implicitHeight: 30
                    flat: true
                    contentItem: LineIcon {
                        name: "chevron-left"
                        color: parent.hovered ? Theme.accent : Theme.textSecondary
                        width: 16
                        height: 16
                    }
                    background: Rectangle {
                        radius: 7
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                    }
                    onClicked: root.expanded = false
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - Theme.paddingLarge * 2
                    spacing: Theme.paddingSmall
                    visible: root.dubbing.history.length === 0
                    LineIcon { name: "history"; color: Theme.textSecondary; opacity: 0.6; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
                    Text { Layout.fillWidth: true; text: qsTr("No saved projects"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    Text { Layout.fillWidth: true; text: qsTr("Saved dubbing projects will appear here."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                }

                ListView {
                    anchors.fill: parent
                    visible: root.dubbing.history.length > 0
                    model: root.dubbing.history
                    spacing: Theme.paddingSmall
                    clip: true

                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: historyEntry.implicitHeight + Theme.paddingMedium * 2
                        radius: Theme.radiusSmall
                        color: Qt.rgba(1, 1, 1, 0.025)
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1

                        ColumnLayout {
                            id: historyEntry
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: 3
                            Text { Layout.fillWidth: true; text: modelData.projectName || qsTr("Untitled project"); color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true; elide: Text.ElideRight }
                            Text { Layout.fillWidth: true; text: modelData.sourceName || modelData.sourceMediaPath || qsTr("No source media"); color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideMiddle }
                            Text { Layout.fillWidth: true; text: (modelData.sourceLanguage || "") + " → " + (modelData.targetLanguage || "") + " · " + (modelData.segmentCount || 0) + qsTr(" segments"); color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                            Text { Layout.fillWidth: true; text: modelData.timestamp || ""; color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                PrimaryButton {
                                    text: qsTr("Open")
                                    iconName: "edit"
                                    quiet: true
                                    textColor: Theme.accentLight
                                    borderColor: Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.42)
                                    implicitWidth: 82
                                    implicitHeight: 30
                                    enabled: !root.dubbing.processing
                                    onClicked: {
                                        if (root.dubbing.openProject(modelData.projectPath || "")) root.projectOpened()
                                    }
                                }
                                PrimaryButton {
                                    text: qsTr("Delete")
                                    iconName: "trash"
                                    quiet: true
                                    textColor: Theme.danger
                                    borderColor: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.42)
                                    implicitWidth: 82
                                    implicitHeight: 30
                                    onClicked: root.deleteRequested(modelData.id || "")
                                }
                            }
                        }
                    }
                }
            }
        }
    }

}
