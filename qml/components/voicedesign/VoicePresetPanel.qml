import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"
import "../shared"

Item {
    id: root

    property string familyId: ""
    property string currentDesignName: ""
    property string currentDesignDescription: ""
    property var presets: []

    signal presetSelected(string description, string name)

    function refreshPresets() {
        if (familyId !== "") {
            root.presets = AppController.voiceDesignPresets.presetsForFamily(familyId)
        } else {
            root.presets = []
        }
    }

    function openLibrary(prefillCurrent) {
        if (root.familyId === "") return
        libraryDialog.initialMode = "design"
        libraryDialog.open()
        if (prefillCurrent)
            Qt.callLater(libraryDialog.applyCurrentDesign)
    }

    Component.onCompleted: refreshPresets()
    onFamilyIdChanged: refreshPresets()

    Connections {
        target: AppController.voiceDesignPresets
        function onPresetsChanged(famId) {
            if (famId === root.familyId)
                root.refreshPresets()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("Designed Voices")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                Layout.fillWidth: true
            }

            PrimaryButton {
                text: qsTr("Manage")
                iconName: "settings"
                quiet: true
                implicitWidth: 96
                implicitHeight: 32
                enabled: root.familyId !== ""
                onClicked: root.openLibrary(false)
            }
        }

        PrimaryButton {
            Layout.fillWidth: true
            text: qsTr("Save Current Description")
            iconName: "save"
            quiet: true
            enabled: root.familyId !== "" && root.currentDesignDescription.trim() !== ""
            onClicked: root.openLibrary(true)
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            visible: root.presets.length > 0

            ColumnLayout {
                width: parent.width - 8
                spacing: Theme.paddingSmall

                Repeater {
                    model: root.presets

                    delegate: Rectangle {
                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: itemLayout.implicitHeight + Theme.paddingSmall * 2
                        radius: Theme.radiusSmall
                        color: itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(1, 1, 1, 0.015)
                        border.color: itemMouse.containsMouse ? Qt.rgba(0.49, 0.30, 1.0, 0.25) : Qt.rgba(1, 1, 1, 0.04)
                        border.width: 1

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.presetSelected(modelData.description, modelData.name)
                        }

                        RowLayout {
                            id: itemLayout
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingSmall

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSmall + 1
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                }
                            }

                            LineIcon {
                                name: "chevron-right"
                                color: Theme.textSecondary
                                opacity: itemMouse.containsMouse ? 0.8 : 0.35
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.presets.length === 0
            spacing: Theme.paddingMedium
            Layout.topMargin: 40

            LineIcon {
                name: "spark"
                color: Theme.textSecondary
                opacity: 0.35
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }

            Text {
                text: qsTr("No designed voices saved yet.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    VoiceLibraryDialog {
        id: libraryDialog
        parent: Overlay.overlay
        familyId: root.familyId
        currentDesignName: root.currentDesignName
        currentDesignDescription: root.currentDesignDescription
        onDesignVoiceSelected: function(description, name) {
            root.presetSelected(description, name)
        }
    }
}