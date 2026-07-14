import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../base"
import "../shared"

// Component for reference voice input with transcript
ColumnLayout {
    id: root

    property string audioPath: ""
    property string referenceText: ""
    property bool isPlaying: false
    property bool showTips: true
    property bool showHeader: true
    property bool locked: false
    property string familyId: ""
    property var savedVoices: []
    property int selectedSavedVoiceIndex: -1

    signal audioCleared()
    signal playClicked()
    signal stopClicked()

    onAudioPathChanged: {
        AppController.preview.requestWavSamples(root.audioPath)
    }

    onReferenceTextChanged: {
        if (refTextEdit.text !== root.referenceText)
            refTextEdit.text = root.referenceText
    }

    onFamilyIdChanged: reloadSavedVoices()
    Component.onCompleted: reloadSavedVoices()

    Connections {
        target: AppController.voiceClonePresets
        function onPresetsChanged(familyId) {
            if (familyId === root.familyId)
                root.reloadSavedVoices()
        }
    }

    function reloadSavedVoices() {
        root.savedVoices = root.familyId !== "" ? AppController.voiceClonePresets.presetsForFamily(root.familyId) : []
        root.selectedSavedVoiceIndex = -1
    }

    function defaultVoiceName() {
        if (root.audioPath !== "")
            return VoiceCloningUtils.fileNameFromPath(root.audioPath)
        return "Cloned voice"
    }

    function loadSavedVoice(index) {
        if (root.locked || index < 0 || index >= root.savedVoices.length) return
        var voice = root.savedVoices[index]
        root.selectedSavedVoiceIndex = index
        root.audioPath = voice.audioPath || ""
        root.referenceText = voice.referenceText || ""
    }

    function saveCurrentVoice() {
        if (root.locked || root.familyId === "" || root.audioPath === "") return
        libraryDialog.initialMode = "reference"
        libraryDialog.open()
        Qt.callLater(libraryDialog.applyCurrentReference)
    }

    function manageVoices() {
        if (root.familyId === "") return
        libraryDialog.initialMode = "reference"
        libraryDialog.open()
    }
    spacing: Theme.paddingLarge
    Layout.fillHeight: false

    // Header
    Text {
        visible: root.showHeader
        text: "Reference Voice"
        color: Theme.textPrimary
        font.pixelSize: Theme.fontMedium
        font.bold: true
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.paddingSmall
        visible: root.familyId !== ""

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            LineIcon {
                name: "spark"
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }

            Text {
                text: "Saved Voices"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
            }
        }

        AppComboBox {
            id: savedVoiceCombo
            Layout.fillWidth: true
            model: root.savedVoices
            textRole: "name"
            secondaryTextRole: "originalAudioName"
            currentIndex: root.selectedSavedVoiceIndex
            enabled: !root.locked && root.savedVoices.length > 0
            onActivated: function(index) { root.loadSavedVoice(index) }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            PrimaryButton {
                text: "Save Current"
                quiet: true
                implicitHeight: 34
                implicitWidth: 120
                enabled: !root.locked && root.audioPath !== ""
                onClicked: root.saveCurrentVoice()
            }

            PrimaryButton {
                text: "Manage"
                iconName: "settings"
                quiet: true
                implicitHeight: 34
                implicitWidth: 105
                enabled: root.familyId !== ""
                onClicked: root.manageVoices()
            }
        }
    }

    // Input tabs box
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 160
        color: Qt.rgba(1, 1, 1, 0.02)
        radius: Theme.radiusSmall
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium
            visible: root.audioPath === ""

            InputSourceTabs {
                id: inputTabs
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !root.locked
                recordingSampleRate: root.familyId === "vieneu-tts-v3-turbo" ? 48000 : 24000
                recommendedAudioHint: root.familyId === "vieneu-tts-v3-turbo"
                                      ? "48kHz mono WAV recommended for VieNeu v3 native cloning"
                                      : "24kHz mono WAV recommended"

                onAudioLoaded: (path) => {
                    if (root.locked) return
                    root.audioPath = path
                }
            }
        }

        // Display loaded audio
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            visible: root.audioPath !== ""
            spacing: Theme.paddingMedium

            Text {
                text: "Loaded: " + VoiceCloningUtils.fileNameFromPath(root.audioPath)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            WaveformView {
                id: refWaveform
                Layout.fillWidth: true
                Layout.fillHeight: true
                samples: (root.audioPath !== "" && AppController.preview.wavSamplesSourcePath === root.audioPath) ? AppController.preview.wavSamples : []
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.paddingMedium

                PrimaryButton {
                    text: "Change / Record"
                    quiet: true
                    implicitHeight: 32
                    enabled: !root.locked
                    onClicked: {
                        if (root.isPlaying)
                            root.stopClicked()
                        root.audioPath = ""
                        root.audioCleared()
                    }
                }

                PrimaryButton {
                    text: root.isPlaying ? "Stop" : "Play"
                    buttonColor: root.isPlaying ? Theme.danger : Theme.success
                    implicitHeight: 32
                    onClicked: {
                        if (root.isPlaying) {
                            root.stopClicked()
                        } else {
                            root.playClicked()
                        }
                    }
                }
            }
        }
    }

    // Reference Transcript
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            LineIcon {
                name: "file"
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
            Text {
                text: "Reference Transcript"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
            }
            PrimaryButton {
                text: "Import .txt"
                iconName: "folder"
                quiet: true
                implicitHeight: 28
                implicitWidth: 105
                enabled: !root.locked
                onClicked: txtFileDialogLoader.active = true
            }
        }

        Text {
            text: "Helps the model align the reference audio correctly."
            color: Theme.textSecondary
            font.pixelSize: 11
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: Theme.surface
            border.color: Theme.surfaceAlt
            border.width: 1
            radius: Theme.radiusSmall

            AppTextArea {
                id: refTextEdit
                anchors.fill: parent
                anchors.margins: 4
                placeholderText: "Type what is spoken in the reference audio here..."
                font.pixelSize: Theme.fontSmall
                background: Rectangle { color: "transparent" }
                readOnly: root.locked

                onTextChanged: root.referenceText = text
            }
        }
    }

    // Tips
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: tipsCol.implicitHeight + Theme.paddingMedium * 2
        color: Qt.rgba(0.49, 0.30, 1.0, 0.05)
        radius: Theme.radiusSmall
        border.color: Qt.rgba(0.49, 0.30, 1.0, 0.1)
        border.width: 1
        visible: root.showTips

        ColumnLayout {
            id: tipsCol
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: 4

            RowLayout {
                LineIcon {
                    name: "info"
                    color: Theme.accent
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }
                Text {
                    text: "Tips for better cloning"
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }
            }

            Text {
                text: "• Clean audio without background noise\n• 5-15 seconds of clear speech\n• Natural tone and prosody"
                color: Theme.textSecondary
                font.pixelSize: 11
                lineHeight: 1.3
                Layout.fillWidth: true
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }

    VoiceLibraryDialog {
        id: libraryDialog
        parent: Overlay.overlay
        familyId: root.familyId
        currentReferenceAudioPath: root.audioPath
        currentReferenceText: root.referenceText
        onReferenceVoiceSelected: function(audioPath, referenceText, name) {
            if (root.locked) return
            root.audioPath = audioPath
            root.referenceText = referenceText
        }
    }
    Loader {
        id: txtFileDialogLoader
        active: false
        sourceComponent: txtFileDialogComponent
    }

    Component {
        id: txtFileDialogComponent
        FileDialog {
            title: "Select Reference Transcript"
            nameFilters: ["Text files (*.txt)", "All files (*)"]

            Component.onCompleted: open()

            onAccepted: {
                if (root.locked) {
                    txtFileDialogLoader.active = false
                    return
                }
                var path = AppController.files.urlToLocalPath(selectedFile.toString())
                var content = AppController.files.readTextFile(path)
                refTextEdit.text = content
                txtFileDialogLoader.active = false
            }
            onRejected: txtFileDialogLoader.active = false
        }
    }
}
