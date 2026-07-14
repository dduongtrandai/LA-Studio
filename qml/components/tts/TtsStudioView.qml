import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../shared"
import "../base"

StudioShell {
    id: root

    family: null
    families: []
    capability: "tts"
    selectedFamilyId: family ? family.id : ""
    studioContext: null
    studioReady: false
    isSettingsOpen: true
    showLeftPanel: true
    isLeftPanelOpen: true
    modalSelectionMode: true
    showSwitcher: false
    modalSelectionTitle: qsTr("Model + Runtime")
    modalSelectionValue: qsTr("Select model and runtime")
    modalSelectionDetail: ""
    backToolTip: qsTr("Change model and runtime")

    property string playingType: "none"
    property string detectedLanguage: "en"
    property bool outputReady: AppController.tts.lastSampleCount > 0 && !AppController.tts.isCloneAction
    property string lastSynthesizedText: ""
    property real mainHorizontalInset: Theme.paddingXL
    property real promptInset: Theme.paddingSmall
    readonly property bool inputsLocked: AppController.tts.processing
    readonly property var nonVerbalTags: {
        if (family && family.studio && family.studio[root.capability] && family.studio[root.capability].nonVerbalTags)
            return family.studio[root.capability].nonVerbalTags
        return []
    }
    readonly property var availableExamples: AppController.examples.examplesForTask("tts", root.family || ({}))

    signal backToGallery()
    signal reloadRequested()
    signal ejectRequested()
    signal modelSwitchRequested(string familyId)
    signal runtimeSwitchRequested(string runtimeId)

    onRequestBack: root.backToGallery()
    onRequestConfigurationPicker: root.backToGallery()
    onRequestReload: root.reloadRequested()
    onRequestEject: root.ejectRequested()
    onRequestModelSwitch: function(familyId) { root.modelSwitchRequested(familyId) }
    onRequestRuntimeSwitch: function(runtimeId) { root.runtimeSwitchRequested(runtimeId) }

    function resolveBackendType() {
        var item = root.currentRuntimeItem()
        if (!item) return ""
        var id = (item.id || "").toLowerCase()
        var name = (item.name || "").toLowerCase()
        if (id.indexOf("crispasr") >= 0 || id.indexOf("kokoro") >= 0 ||
            name.indexOf("crispasr") >= 0 || name.indexOf("kokoro") >= 0)
            return "kokoro"
        if (id.indexOf("vibevoice") >= 0 || id.indexOf("vibe") >= 0 ||
            name.indexOf("vibevoice") >= 0 || name.indexOf("vibe") >= 0)
            return "vibevoice"
        return ""
    }

    function outputDurationText() {
        if (!root.outputReady || AppController.tts.sampleRate <= 0) return "--"
        var seconds = AppController.tts.lastSampleCount / AppController.tts.sampleRate
        if (seconds < 60) return seconds.toFixed(1) + "s"
        var minutes = Math.floor(seconds / 60)
        var remain = Math.floor(seconds % 60)
        return minutes + ":" + (remain < 10 ? "0" + remain : remain)
    }

    function sampleCountText() {
        var count = AppController.tts.lastSampleCount
        if (count >= 1000000) return (count / 1000000).toFixed(1) + "M samples"
        if (count >= 1000) return (count / 1000).toFixed(1) + "k samples"
        return count + " samples"
    }

    function getSelectedVoiceName() {
        if (!root.family) return "Default"
        var schema = AppController.tts.currentSchema
        for (var i = 0; i < schema.length; i++) {
            var item = schema[i]
            if (item.id === "voice" && item.type === "choice") {
                var selectedVal = settingsPanel.dynamicSettings[item.id]
                if (selectedVal !== undefined && item.choices) {
                    for (var j = 0; j < item.choices.length; j++) {
                        if (item.choices[j].value === selectedVal) {
                            var displayName = item.choices[j].text
                            var dashIdx = displayName.indexOf(" -")
                            if (dashIdx > 0) {
                                return displayName.substring(0, dashIdx).trim()
                            }
                            return displayName
                        }
                    }
                }
            }
        }
        return "Default"
    }

    function applyExample(example) {
        if (!example || root.inputsLocked) return
        var inputs = example.inputs || {}
        var settings = example.settings || {}
        if (inputs.text !== undefined) {
            inputText.text = inputs.text
        }
        settingsPanel.applyExampleSettings(settings)
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) root.playingType = "none"
        }
    }

    Connections {
        target: AppController.tts
        function onSynthesisFinished(pcm16, sampleRate) {
            if (AppController.tts.lastGenerationMode !== "tts") return;
            var text = root.lastSynthesizedText.trim()
            if (text === "") {
                text = inputText.text.trim()
            }
            var modelName = root.family ? root.family.title : "Unknown Model"
            var voiceName = root.getSelectedVoiceName()
            AppController.history.addTtsHistoryItem(text, modelName, voiceName)
        }
    }

    Timer {
        id: languageDetectTimer
        interval: 250
        repeat: false
        onTriggered: root.detectedLanguage = VoiceCloningUtils.detectLanguageCode(inputText.text)
    }

    leftPanelContent: [
        TtsHistoryPanel {
            id: historyPanel
            anchors.fill: parent
            onCloseRequested: root.isLeftPanelOpen = false
            onLoadPromptRequested: function(text) {
                if (root.inputsLocked) return
                inputText.text = text
            }
        }
    ]

    mainContent: [
        Item {
            anchors.fill: parent

            Item {
                anchors.fill: parent
                visible: opacity > 0
                opacity: root.studioReady ? 0.0 : 1.0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
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
                                text: qsTr("Select a TTS model and runtime")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontLarge
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("The studio stays lightweight until you choose a compatible TTS configuration.")
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

            ScrollView {
                id: editorScroll
                anchors.fill: parent
                visible: opacity > 0
                opacity: root.studioReady ? 1.0 : 0.0
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

                ColumnLayout {
                    width: Math.max(0, editorScroll.availableWidth - root.mainHorizontalInset * 2)
                    height: Math.max(implicitHeight, editorScroll.availableHeight - Theme.paddingLarge * 2)
                    x: root.mainHorizontalInset
                    y: Theme.paddingLarge
                    spacing: Theme.paddingMedium

                Rectangle {
                    Layout.fillWidth: true
                    visible: false
                    implicitHeight: 48
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.paddingMedium
                        anchors.rightMargin: Theme.paddingMedium
                        spacing: Theme.paddingSmall

                        LineIcon {
                            name: "gallery"
                            color: Theme.accent
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Select model and runtime to enable generation")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                        }

                        Button {
                            text: qsTr("Choose")
                            flat: true
                            onClicked: root.backToGallery()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.promptInset
                    Layout.rightMargin: root.promptInset
                    Layout.fillHeight: true
                    Layout.minimumHeight: 360
                    Layout.preferredHeight: 520
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.color: Qt.rgba(1, 1, 1, 0.07)
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium
                            spacing: Theme.paddingSmall

                            LineIcon {
                                name: "file"
                                color: Theme.textSecondary
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Prompt")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }

                            PrimaryButton {
                                id: examplesButton
                                text: qsTr("Examples (%1)").arg(root.availableExamples.length)
                                iconName: "file"
                                quiet: true
                                textColor: Theme.textPrimary
                                Layout.preferredWidth: 140
                                Layout.preferredHeight: 30
                                enabled: root.studioReady && !root.inputsLocked && root.availableExamples.length > 0
                                visible: root.availableExamples.length > 0
                                onClicked: examplePicker.open()
                            }

                            PrimaryButton {
                                text: qsTr("Import .txt")
                                iconName: "folder"
                                quiet: true
                                textColor: Theme.textPrimary
                                Layout.preferredWidth: 110
                                Layout.preferredHeight: 30
                                enabled: root.studioReady && !root.inputsLocked
                                onClicked: promptFileDialogLoader.active = true
                            }

                            Text {
                                text: qsTr("%1 chars").arg(inputText.text.length)
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                Layout.minimumWidth: 64
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            AppTextArea {
                                id: inputText
                                placeholderText: qsTr("Enter text to synthesize...")
                                font.pixelSize: Theme.fontMedium
                                background: Rectangle { color: "transparent" }
                                enabled: root.studioReady
                                readOnly: root.inputsLocked
                                opacity: root.studioReady ? 1.0 : 0.55
                                onTextChanged: languageDetectTimer.restart()

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !root.studioReady
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.backToGallery()
                                }
                            }
                        }

                        PromptTagPicker {
                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium
                            tags: root.nonVerbalTags
                            targetEditor: inputText
                            locked: root.inputsLocked
                            visible: root.nonVerbalTags.length > 0
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium
                            Layout.bottomMargin: Theme.paddingSmall
                            text: qsTr("⚠️ VieNeu-TTS Turbo may be less stable with phrases under 5 words.")
                            color: Theme.warning
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            visible: {
                                var words = inputText.text.trim().split(/\s+/).filter(function(w) { return w.length > 0; }).length;
                                return words > 0 && words < 5 && root.family && root.family.id === "vieneu-tts-v2-turbo";
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.promptInset
                    Layout.rightMargin: root.promptInset
                    Layout.preferredHeight: 42
                    spacing: 0

                    Item { Layout.fillWidth: true }

                    PrimaryButton {
                        text: qsTr("Generate")
                        iconName: "spark"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 42
                        visible: !AppController.tts.processing
                        enabled: (root.studioController ? root.studioController.canProcess : false) && AppController.tts.modelLoaded && inputText.text.length > 0 && !root.inputsLocked
                        onClicked: {
                            root.lastSynthesizedText = inputText.text
                            var synSettings = settingsPanel.getSynthesisSettings()
                            AppController.tts.synthesize(inputText.text.normalize("NFC"), 0, 1.0, synSettings)
                        }
                    }

                    PrimaryButton {
                        text: qsTr("Stop")
                        iconName: "stop"
                        buttonColor: Theme.danger
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 42
                        visible: AppController.tts.processing
                        onClicked: AppController.tts.cancelProcessing()
                    }
                }

                GeneratedAudioOutput {
                    Layout.maximumWidth: 980
                    Layout.alignment: Qt.AlignHCenter
                    family: root.family
                    outputReady: root.outputReady
                    samples: AppController.tts.lastSamplePreview
                    durationText: root.outputDurationText()
                    sampleRate: AppController.tts.sampleRate
                    sampleCountText: root.sampleCountText()
                    isPlaying: root.playingType === "tts" && AppController.player.playing
                    isPaused: root.playingType === "tts" && AppController.player.paused
                    playbackPositionMs: root.playingType === "tts" ? AppController.player.playbackPositionMs : 0
                    playbackDurationMs: root.playingType === "tts" ? AppController.player.playbackDurationMs : 0
                    audioDurationMs: AppController.tts.sampleRate > 0
                                     ? Math.round(AppController.tts.lastSampleCount * 1000 / AppController.tts.sampleRate) : 0
                    processing: AppController.tts.processing
                    generationProgress: AppController.tts.generationProgress
                    progressEstimated: AppController.tts.generationProgressEstimated
                    progressLabel: AppController.tts.generationProgressLabel
                    onPlayClicked: {
                        root.playingType = "tts"
                        AppController.preview.playLastTts()
                    }
                    onPauseClicked: AppController.player.pause()
                    onResumeClicked: AppController.player.resume()
                    onStopClicked: AppController.player.stop()
                    onSeekRequested: function(positionMs) {
                        root.playingType = "tts"
                        if (AppController.player.playing)
                            AppController.player.seek(positionMs)
                        else
                            AppController.preview.playLastTtsAtPosition(positionMs)
                    }
                    onSaveClicked: saveDialog.open()
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.paddingLarge
                }
                }
            }
        }
    ]

    settingsContent: [
        Item {
            anchors.fill: parent

            Item {
                anchors.fill: parent
                visible: opacity > 0
                opacity: root.studioReady ? 0.0 : 1.0
                Behavior on opacity {
                    NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
                }

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
                        text: qsTr("Choose a model to unlock TTS settings")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Model parameters and audio controls appear after the TTS configuration is loaded.")
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

            TtsSettingsPanel {
                id: settingsPanel
                anchors.fill: parent
                visible: opacity > 0
                opacity: root.studioReady ? 1.0 : 0.0
                Behavior on opacity {
                    NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
                }
                family: root.family
                suggestedLanguage: root.detectedLanguage
                backendType: root.resolveBackendType()
                locked: root.inputsLocked
                onCloseRequested: root.isSettingsOpen = false
            }
        }
    ]

    FileDialog {
        id: saveDialog
        title: "Save Audio File"
        fileMode: FileDialog.SaveFile
        nameFilters: ["WAV files (*.wav)"]
        onAccepted: AppController.preview.saveWav(selectedFile.toString())
    }

    ExamplePickerDialog {
        id: examplePicker
        parent: Overlay.overlay
        examples: root.availableExamples
        taskTitle: qsTr("Text-to-Speech Examples")
        onExampleSelected: function(example) { root.applyExample(example) }
    }

    Loader {
        id: promptFileDialogLoader
        active: false
        sourceComponent: promptFileDialogComponent
    }

    Component {
        id: promptFileDialogComponent
        FileDialog {
            title: qsTr("Select Prompt Text")
            nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]

            Component.onCompleted: open()

            onAccepted: {
                if (root.inputsLocked) {
                    promptFileDialogLoader.active = false
                    return
                }
                var path = AppController.files.urlToLocalPath(selectedFile.toString())
                var content = AppController.files.readTextFile(path)
                inputText.text = content
                promptFileDialogLoader.active = false
            }
            onRejected: promptFileDialogLoader.active = false
        }
    }
}
