import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../shared"
import "../base"
import "../tts"

StudioShell {
    id: root

    family: null
    families: []
    capability: "voice-cloning"
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

    property string referenceAudioPath: ""
    property string playingType: "none"
    property string detectedLanguage: "en"
    property bool outputReady: AppController.tts.lastSampleCount > 0 && AppController.tts.isCloneAction
    property string selectedLanguageCode: "en"
    property real mainHorizontalInset: Theme.paddingXL
    property real promptInset: Theme.paddingSmall
    readonly property bool inputsLocked: AppController.tts.processing
    readonly property var nonVerbalTags: {
        if (family && family.studio && family.studio[root.capability] && family.studio[root.capability].nonVerbalTags)
            return family.studio[root.capability].nonVerbalTags
        return []
    }
    readonly property var availableExamples: AppController.examples.examplesForTask("voice-cloning", root.family || ({}))

    property var studioConfig: ({})
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs ? studioConfig.inputs.indexOf("language") !== -1 : true

    function refreshCapabilityMetadata() {
        root.studioConfig = AppController.tts.studioConfigForCapability("voice-cloning")
    }

    Component.onCompleted: refreshCapabilityMetadata()

    Connections {
        target: AppController.tts
        function onFamilyConfigChanged() { root.refreshCapabilityMetadata() }
        function onSchemaChanged() { root.refreshCapabilityMetadata() }
    }

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

    readonly property var defaultLanguages: [
        { text: qsTr("English"), value: "en", detail: qsTr("Language code: en") },
        { text: qsTr("Chinese"), value: "zh", detail: qsTr("Language code: zh") },
        { text: qsTr("Japanese"), value: "ja", detail: qsTr("Language code: ja") },
        { text: qsTr("Korean"), value: "ko", detail: qsTr("Language code: ko") },
        { text: qsTr("French"), value: "fr", detail: qsTr("Language code: fr") },
        { text: qsTr("Spanish"), value: "es", detail: qsTr("Language code: es") },
        { text: qsTr("German"), value: "de", detail: qsTr("Language code: de") },
        { text: qsTr("Vietnamese"), value: "vi", detail: qsTr("Language code: vi") }
    ]

    function normalizeLanguageItems(items) {
        var result = []
        for (var i = 0; i < items.length; i++) {
            var item = items[i] || {}
            var code = item.value || item.code || ""
            var label = item.text || item.name || code
            if (item.name && item.code) {
                label = item.name + " (" + item.code + ")"
            }
            result.push({
                text: label,
                value: code,
                detail: item.detail || (code !== "" ? "Language code: " + code : "")
            })
        }
        return result
    }

    function languageIndexFor(value) {
        for (var i = 0; i < root.languageItems.length; ++i) {
            if (root.languageItems[i].value === value) return i
        }
        return -1
    }

    readonly property var languageItems: {
        if (family) {
            if (family.supportedLanguageSetId) {
                var list = AppController.catalog.languageSet(family.supportedLanguageSetId)
                if (list && list.length > 0) {
                    return normalizeLanguageItems(list)
                }
            }
            if (family.supportedLanguages && family.supportedLanguages.length > 0) {
                return normalizeLanguageItems(family.supportedLanguages)
            }
            if (family.featuredLanguages && family.featuredLanguages.length > 0) {
                return normalizeLanguageItems(family.featuredLanguages)
            }
        }
        return normalizeLanguageItems(defaultLanguages)
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

    function applyExample(example) {
        if (!example || root.inputsLocked) return
        var inputs = example.inputs || {}
        var settings = example.settings || {}
        if (inputs.text !== undefined) {
            inputText.text = inputs.text
        }
        if (inputs.referenceText !== undefined) {
            referenceBox.referenceText = inputs.referenceText
        }
        if (inputs.referenceAudioPath !== undefined && inputs.referenceAudioPath !== "") {
            referenceBox.audioPath = inputs.referenceAudioPath
        }
        if (settings["lang"] !== undefined) {
            root.selectedLanguageCode = settings["lang"]
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
            if (AppController.tts.lastGenerationMode !== "voice-cloning") return;
            var text = inputText.text.trim()
            var modelName = root.family ? root.family.title : "Cloned Voice"
            var voiceName = "Clone"
            AppController.history.addTtsHistoryItem(text, modelName, voiceName)
        }
    }

    Timer {
        id: languageDetectTimer
        interval: 250
        repeat: false
        onTriggered: root.detectedLanguage = VoiceCloningUtils.detectLanguageCode(inputText.text)
    }

    onDetectedLanguageChanged: {
        var detectedIndex = root.languageIndexFor(root.detectedLanguage)
        if (detectedIndex >= 0 && root.selectedLanguageCode !== root.detectedLanguage) {
            root.selectedLanguageCode = root.detectedLanguage
        }
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
                                text: qsTr("Select a Voice Cloning model")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontLarge
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("The studio stays lightweight until you choose a compatible configuration.")
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
                Layout.leftMargin: root.mainHorizontalInset
                Layout.rightMargin: root.mainHorizontalInset
                Layout.topMargin: Theme.paddingLarge
                Layout.bottomMargin: Theme.paddingLarge
                spacing: Theme.paddingMedium

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall
                    color: Theme.surface
                    border.color: Qt.rgba(1, 1, 1, 0.07)
                    border.width: 1
                    clip: true

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingLarge
                        spacing: Theme.paddingLarge

                        Rectangle {
                            Layout.preferredWidth: 360
                            Layout.fillHeight: true
                            radius: Theme.radiusSmall
                            color: Qt.rgba(1, 1, 1, 0.02)
                            border.color: Qt.rgba(1, 1, 1, 0.07)
                            border.width: 1
                            clip: true

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingMedium
                                clip: true
                                contentWidth: availableWidth

                                ReferenceInputBox {
                                    id: referenceBox
                                    width: parent.width
                                    showTips: false
                                    showHeader: true
                                    locked: root.inputsLocked
                                    familyId: root.family ? root.family.id : ""
                                    isPlaying: root.playingType === "reference"
                                    onAudioPathChanged: {
                                        root.referenceAudioPath = referenceBox.audioPath
                                    }
                                    onPlayClicked: {
                                        root.playingType = "reference"
                                        AppController.player.playFile(root.referenceAudioPath)
                                    }
                                    onStopClicked: AppController.player.stop()
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Theme.radiusSmall
                                color: Qt.rgba(1, 1, 1, 0.02)
                                border.color: Qt.rgba(1, 1, 1, 0.07)
                                border.width: 1
                                clip: true

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.paddingMedium
                                    clip: true
                                    contentWidth: availableWidth

                                    ColumnLayout {
                                        width: parent.width
                                        spacing: Theme.paddingMedium

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.paddingSmall
                                            visible: root.hasLanguageInput

                                            LineIcon {
                                                name: "globe"
                                                color: Theme.textSecondary
                                                Layout.preferredWidth: 16
                                                Layout.preferredHeight: 16
                                            }

                                            Text {
                                                text: qsTr("Target Language")
                                                color: Theme.textPrimary
                                                font.pixelSize: Theme.fontSmall
                                                font.bold: true
                                                Layout.fillWidth: true
                                            }

                                            AppComboBox {
                                                id: langCombo
                                                Layout.preferredWidth: 220
                                                model: root.languageItems
                                                textRole: "text"
                                                currentIndex: {
                                                    var selectedIndex = root.languageIndexFor(root.selectedLanguageCode)
                                                    if (selectedIndex >= 0) return selectedIndex
                                                    var detectedIndex = root.languageIndexFor(root.detectedLanguage)
                                                    if (detectedIndex >= 0) return detectedIndex
                                                    return 0
                                                }
                                                searchable: model.length > 15
                                                enabled: !root.inputsLocked
                                                onCurrentIndexChanged: {
                                                    if (!model || currentIndex < 0 || currentIndex >= model.length) return
                                                    var value = model[currentIndex].value
                                                    if (root.selectedLanguageCode !== value) {
                                                        root.selectedLanguageCode = value
                                                    }
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.paddingSmall

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.paddingSmall

                                                LineIcon {
                                                    name: "file"
                                                    color: Theme.textSecondary
                                                    Layout.preferredWidth: 16
                                                    Layout.preferredHeight: 16
                                                }

                                                Text {
                                                    text: qsTr("Target Prompt")
                                                    color: Theme.textPrimary
                                                    font.pixelSize: Theme.fontSmall
                                                    font.bold: true
                                                    Layout.fillWidth: true
                                                }

                                                PrimaryButton {
                                                    id: examplesButton
                                                    text: qsTr("Examples (%1)").arg(root.availableExamples.length)
                                                    iconName: "file"
                                                    quiet: true
                                                    textColor: Theme.textPrimary
                                                    Layout.preferredWidth: 140
                                                    Layout.preferredHeight: 30
                                                    visible: root.availableExamples.length > 0
                                                    enabled: !root.inputsLocked && root.availableExamples.length > 0
                                                    onClicked: examplePicker.open()
                                                }

                                                PrimaryButton {
                                                    text: qsTr("Import .txt")
                                                    iconName: "folder"
                                                    quiet: true
                                                    implicitHeight: 28
                                                    implicitWidth: 105
                                                    enabled: !root.inputsLocked
                                                    onClicked: promptFileDialogLoader.active = true
                                                }

                                                Text {
                                                    text: inputText.text.length + " chars"
                                                    color: Theme.textSecondary
                                                    font.pixelSize: Theme.fontSmall
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 420
                                                color: Theme.surface
                                                border.color: Theme.surfaceAlt
                                                border.width: 1
                                                radius: Theme.radiusSmall

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: Theme.paddingSmall
                                                    spacing: Theme.paddingSmall

                                                    ScrollView {
                                                        id: promptTextScroll
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        clip: true
                                                        contentWidth: availableWidth
                                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                                        AppTextArea {
                                                            id: inputText
                                                            width: promptTextScroll.availableWidth
                                                            height: Math.max(promptTextScroll.availableHeight, contentHeight + topPadding + bottomPadding)
                                                            placeholderText: qsTr("Enter the text you want the cloned voice to say...")
                                                            font.pixelSize: Theme.fontMedium
                                                            background: Rectangle { color: "transparent" }
                                                            readOnly: root.inputsLocked
                                                            onTextChanged: languageDetectTimer.restart()
                                                        }
                                                    }

                                                    PromptTagPicker {
                                                        Layout.fillWidth: true
                                                        tags: root.nonVerbalTags
                                                        targetEditor: inputText
                                                        locked: root.inputsLocked
                                                        visible: root.nonVerbalTags.length > 0
                                                    }

                                                    Text {
                                                        Layout.fillWidth: true
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
                                        }

                                        GeneratedAudioOutput {
                                            Layout.fillWidth: true
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
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingMedium

                                PrimaryButton {
                                    text: qsTr("Clone Voice")
                                    iconName: "spark"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    visible: !AppController.tts.processing
                                    enabled: {
                                        var isQwen3 = root.family && root.family.id && root.family.id.indexOf("qwen3") !== -1
                                        var baseEnabled = (root.studioController ? root.studioController.canProcess : false) && AppController.tts.modelLoaded && inputText.text.length > 0 && root.referenceAudioPath !== ""
                                        baseEnabled = baseEnabled && !root.inputsLocked
                                        if (isQwen3) {
                                            return baseEnabled && referenceBox.referenceText.trim().length > 0
                                        }
                                        return baseEnabled
                                    }
                                    onClicked: {
                                        var settings = settingsPanel.getSettingsObject(root.selectedLanguageCode, inputText.text, referenceBox.referenceText)
                                        AppController.tts.cloneVoice(VoiceCloningUtils.normalizeText(inputText.text), root.referenceAudioPath, settings)
                                    }
                                }

                                PrimaryButton {
                                    text: qsTr("Stop")
                                    iconName: "stop"
                                    buttonColor: Theme.danger
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    visible: AppController.tts.processing
                                    onClicked: AppController.tts.cancelProcessing()
                                }
                            }
                        }
                    }
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
                        text: qsTr("Choose a model to unlock settings")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Model parameters and audio controls appear after the configuration is loaded.")
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

            VoiceSettingsPanel {
                id: settingsPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                family: root.family
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
        taskTitle: qsTr("Voice Cloning Examples")
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
            title: "Select Target Prompt"
            nameFilters: ["Text files (*.txt)", "All files (*)"]

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
