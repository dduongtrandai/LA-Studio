import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../shared"
import "../base"

StudioShell {
    id: root

    property var sttSession: null

    family: {
        if (!studioController) return null
        var fams = studioController.families
        var selectedId = studioController.selectedFamilyId
        for (var i = 0; i < fams.length; i++) {
            if (fams[i].id === selectedId) return fams[i]
        }
        return null
    }
    families: studioController ? studioController.families : []
    capability: "stt"
    studioTitle: studioController ? studioController.studioHeaderTitle : qsTr("Speech-to-Text Studio")
    studioReady: studioController ? studioController.studioReady : false
    isSettingsOpen: true
    showLeftPanel: true
    isLeftPanelOpen: true
    modalSelectionMode: true
    showSwitcher: false
    modalSelectionTitle: family ? family.title : qsTr("Model + Runtime")
    selectedFamilyId: studioController ? studioController.selectedFamilyId : ""
    studioContext: null
    modalSelectionValue: studioController ? studioController.statusText : qsTr("Select model and runtime")
    modalSelectionDetail: studioController ? studioController.statusDetail : ""
    backToolTip: qsTr("Change model and runtime")

    leftPanelContent: [
        SttHistoryPanel {
            id: historyPanel
            anchors.fill: parent
            sttSession: root.sttSession
            onCloseRequested: root.isLeftPanelOpen = false
            onLoadHistoryRequested: (text, filePath) => {
                if (root.sttSession) {
                    root.sttSession.loadHistoryItem(text, filePath)
                }
            }
        }
    ]

    signal backToGallery()

    onRequestBack: root.backToGallery()
    onRequestConfigurationPicker: root.backToGallery()
    onRequestReload: if (studioController) studioController.reload()
    onRequestEject: if (studioController) studioController.unload()

    mainContent: [
        StackLayout {
            anchors.fill: parent
            currentIndex: root.studioReady ? 1 : 0

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.max(320, Math.min(520, parent.width - Theme.paddingXL * 2))
                    spacing: Theme.paddingLarge

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radiusSmall
                        color: Theme.surface
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1
                        implicitHeight: 180

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingLarge
                            spacing: Theme.paddingMedium

                            LineIcon {
                                name: "gallery"
                                color: Theme.accent
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Select an STT model and runtime")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontLarge
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("The studio stays lightweight until you choose a compatible STT configuration.")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Choose model and runtime")
                        iconName: "gallery"
                        onClicked: root.backToGallery()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.paddingXL
                Layout.rightMargin: Theme.paddingXL
                Layout.topMargin: Theme.paddingXL
                Layout.bottomMargin: Theme.paddingXL
                spacing: Theme.paddingXL

                SttInputSection {
                    id: inputSection
                    sttSession: root.sttSession
                    Layout.fillWidth: true
                }

                SttTranscriptionView {
                    id: transcriptionView
                    sttSession: root.sttSession
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    ]

    settingsContent: [
        StackLayout {
            anchors.fill: parent
            currentIndex: root.studioReady ? 1 : 0

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(300, parent.width)
                    spacing: Theme.paddingMedium

                    LineIcon {
                        name: "sliders"
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Choose a model to unlock STT settings")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Model parameters and transcription controls appear after the STT configuration is loaded.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Open model gallery")
                        iconName: "gallery"
                        quiet: true
                        onClicked: root.backToGallery()
                    }
                }
            }

            SttSettingsPanel {
                id: settingsPanel
                sttSession: root.sttSession
                family: root.family
                Layout.fillWidth: true
                Layout.fillHeight: true
                onCloseRequested: root.isSettingsOpen = false
            }
        }
    ]
}
