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
    capability: "voice-design"
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
    property bool outputReady: AppController.tts.lastSampleCount > 0 && AppController.tts.lastGenerationMode === "voice-design"
    property string lastSynthesizedText: ""
    property string lastSynthesizedDescription: ""
    property string selectedPresetName: ""
    property string selectedLanguage: "en"
    property string activeLeftTab: "presets"
    readonly property bool inputsLocked: AppController.tts.processing

    readonly property var studioConfig: AppController.tts.studioConfigForCapability("voice-design")
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs ? studioConfig.inputs.indexOf("language") !== -1 : false
    readonly property var nonVerbalTags: {
        if (family && family.studio && family.studio[root.capability] && family.studio[root.capability].nonVerbalTags)
            return family.studio[root.capability].nonVerbalTags
        return []
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

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) root.playingType = "none"
        }
    }

    Connections {
        target: AppController.tts
        function onSynthesisFinished(pcm16, sampleRate) {
            if (AppController.tts.lastGenerationMode !== "voice-design") return;
            var text = root.lastSynthesizedText.trim()
            if (text === "") {
                text = targetText.text.trim()
            }
            var desc = root.lastSynthesizedDescription.trim()
            if (desc === "") {
                desc = voiceDescriptionText.text.trim()
            }
            var modelName = root.family ? root.family.title : "Unknown Model"
            var familyId = root.family ? root.family.id : ""
            var presetName = root.selectedPresetName || ""
            AppController.history.addVoiceDesignHistoryItem(text, desc, presetName, familyId, modelName)
        }
    }

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

    function languageIndex(items, value) {
        if (!items || items.length === 0) return 0
        for (var i = 0; i < items.length; ++i) {
            if (items[i].value === value) return i
        }
        return 0
    }

    readonly property var supportedLanguages: {
        var autoLanguage = [{ text: qsTr("Auto"), value: "", detail: qsTr("Auto-detect target language") }]
        if (family) {
            if (family.supportedLanguageSetId) {
                var list = AppController.catalog.languageSet(family.supportedLanguageSetId)
                if (list && list.length > 0) {
                    return autoLanguage.concat(normalizeLanguageItems(list))
                }
            }
            if (family.supportedLanguages && family.supportedLanguages.length > 0) {
                return autoLanguage.concat(normalizeLanguageItems(family.supportedLanguages))
            }
            if (family.featuredLanguages && family.featuredLanguages.length > 0) {
                return autoLanguage.concat(normalizeLanguageItems(family.featuredLanguages))
            }
        }
        return autoLanguage.concat(normalizeLanguageItems(defaultLanguages))
    }

    leftPanelContent: [
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header/Tab Buttons
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: "transparent"

                RowLayout {
                    anchors.centerIn: parent
                    spacing: Theme.paddingSmall

                    AppTabButton {
                        text: qsTr("Presets")
                        iconName: "spark"
                        selected: root.activeLeftTab === "presets"
                        onClicked: root.activeLeftTab = "presets"
                    }

                    AppTabButton {
                        text: qsTr("History")
                        iconName: "history"
                        selected: root.activeLeftTab === "history"
                        onClicked: root.activeLeftTab = "history"
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.activeLeftTab === "presets" ? 0 : 1

                VoicePresetPanel {
                    id: presetPanel
                    familyId: root.selectedFamilyId
                    currentDesignName: root.selectedPresetName
                    currentDesignDescription: voiceDescriptionText.text
                    onPresetSelected: function(description, name) {
                        if (root.inputsLocked) return
                        voiceDescriptionText.text = description
                        root.selectedPresetName = name
                    }
                }

                VoiceDesignHistoryPanel {
                    id: historyPanel
                    onLoadDescriptionRequested: function(description, text) {
                        if (root.inputsLocked) return
                        voiceDescriptionText.text = description
                        targetText.text = text
                        root.selectedPresetName = ""
                    }
                }
            }
        }
    ]

    mainContent: [
        Item {
            anchors.fill: parent

            // Unloaded / Loading state overlay
            Item {
                anchors.fill: parent
                visible: false
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
                                text: qsTr("Select a Voice Design model")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontLarge
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Load a model family supporting voice design to begin styling voices from descriptions.")
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

            // Main Editor Panel
            ScrollView {
                id: designEditorScroll
                anchors.fill: parent
                visible: opacity > 0
                opacity: 1.0
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

                ColumnLayout {
                    width: Math.max(0, designEditorScroll.availableWidth - Theme.paddingXL * 2)
                    height: Math.max(implicitHeight, designEditorScroll.availableHeight - Theme.paddingLarge * 2)
                    x: Theme.paddingXL
                    y: Theme.paddingLarge
                    spacing: Theme.paddingMedium

                // Voice Description Text Box
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
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
                            Layout.preferredHeight: 36
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium

                            LineIcon {
                                name: "spark"
                                color: Theme.accentLight
                                Layout.preferredWidth: 15
                                Layout.preferredHeight: 15
                            }

                            Text {
                                text: qsTr("Voice Description (Instruct)")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Text {
                                text: root.selectedPresetName !== "" ? qsTr("Preset: %1").arg(root.selectedPresetName) : qsTr("Custom Description")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall - 1
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium
                            Layout.topMargin: Theme.paddingSmall
                            Layout.bottomMargin: Theme.paddingSmall
                            visible: !settingsPanel.freeTextInstruct
                            radius: 7
                            color: Qt.rgba(1.0, 0.76, 0.20, 0.08)
                            border.color: Qt.rgba(1.0, 0.76, 0.20, 0.25)
                            border.width: 1
                            implicitHeight: noticeText.implicitHeight + Theme.paddingMedium * 1.5

                            Text {
                                id: noticeText
                                anchors.fill: parent
                                anchors.margins: Theme.paddingMedium
                                text: qsTr("This model does not support free-text instruct. Use the options in Generation Settings on the right.")
                                color: Theme.warning
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            AppTextArea {
                                id: voiceDescriptionText
                                placeholderText: qsTr("Describe the voice (e.g., A calm, friendly female voice with a warm tone, speaking slowly in a quiet room...)")
                                font.pixelSize: Theme.fontMedium
                                background: Rectangle { color: "transparent" }
                                enabled: root.studioReady
                                readOnly: root.inputsLocked || !settingsPanel.freeTextInstruct
                                opacity: root.studioReady ? 1.0 : 0.55
                                onTextChanged: {
                                    if (activeFocus) {
                                        root.selectedPresetName = ""
                                    }
                                }
                            }
                        }
                    }
                }

                // Target Text Input Box
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 150
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
                            Layout.preferredHeight: 36
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium

                            LineIcon {
                                name: "file"
                                color: Theme.textSecondary
                                Layout.preferredWidth: 15
                                Layout.preferredHeight: 15
                            }

                            Text {
                                text: qsTr("Target Text")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Text {
                                text: qsTr("%1 chars").arg(targetText.text.length)
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            AppTextArea {
                                id: targetText
                                placeholderText: qsTr("Enter the text you want the voice to speak...")
                                font.pixelSize: Theme.fontMedium
                                background: Rectangle { color: "transparent" }
                                enabled: root.studioReady
                                readOnly: root.inputsLocked
                                opacity: root.studioReady ? 1.0 : 0.55
                            }
                        }

                        PromptTagPicker {
                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.paddingMedium
                            Layout.rightMargin: Theme.paddingMedium
                            tags: root.nonVerbalTags
                            targetEditor: targetText
                            locked: root.inputsLocked
                            visible: root.nonVerbalTags.length > 0
                        }
                    }
                }

                // Action Bar
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    spacing: Theme.paddingMedium

                    Item { Layout.fillWidth: true }

                    PrimaryButton {
                        text: qsTr("Generate Voice")
                        iconName: "spark"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 40
                        visible: !AppController.tts.processing
                        enabled: (root.studioController ? root.studioController.canProcess : false)
                                 && !root.inputsLocked
                                 && AppController.tts.modelLoaded
                                 && targetText.text.length > 0
                                 && (!settingsPanel.requiresInstruct
                                     || (settingsPanel.freeTextInstruct
                                     ? (voiceDescriptionText.text.trim().length > 0 || settingsPanel.hasSelectedVoiceAttributes)
                                     : settingsPanel.hasSelectedVoiceAttributes))
                        onClicked: {
                            var composedDescription = settingsPanel.composedVoiceDescription(settingsPanel.selectedLanguage, voiceDescriptionText.text.normalize("NFC"))
                            root.lastSynthesizedText = targetText.text
                            root.lastSynthesizedDescription = composedDescription
                            var synSettings = settingsPanel.getSynthesisSettings(settingsPanel.selectedLanguage, voiceDescriptionText.text.normalize("NFC"))
                            AppController.tts.designVoice(targetText.text.normalize("NFC"), synSettings)
                        }
                    }

                    PrimaryButton {
                        text: qsTr("Stop")
                        iconName: "stop"
                        buttonColor: Theme.danger
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 40
                        visible: AppController.tts.processing
                        onClicked: AppController.tts.cancelProcessing()
                    }
                }

                // Audio Output Player
                GeneratedAudioOutput {
                    family: root.family
                    outputReady: root.outputReady
                    samples: AppController.tts.lastSamplePreview
                    durationText: root.outputDurationText()
                    sampleRate: AppController.tts.sampleRate
                    sampleCountText: root.sampleCountText()
                    isPlaying: root.playingType === "voice-design" && AppController.player.playing
                    isPaused: root.playingType === "voice-design" && AppController.player.paused
                    playbackPositionMs: root.playingType === "voice-design" ? AppController.player.playbackPositionMs : 0
                    playbackDurationMs: root.playingType === "voice-design" ? AppController.player.playbackDurationMs : 0
                    audioDurationMs: AppController.tts.sampleRate > 0
                                     ? Math.round(AppController.tts.lastSampleCount * 1000 / AppController.tts.sampleRate) : 0
                    processing: AppController.tts.processing
                    generationProgress: AppController.tts.generationProgress
                    progressEstimated: AppController.tts.generationProgressEstimated
                    progressLabel: AppController.tts.generationProgressLabel
                    onPlayClicked: {
                        root.playingType = "voice-design"
                        AppController.preview.playLastTts()
                    }
                    onPauseClicked: AppController.player.pause()
                    onResumeClicked: AppController.player.resume()
                    onStopClicked: AppController.player.stop()
                    onSeekRequested: function(positionMs) {
                        root.playingType = "voice-design"
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
                visible: false
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }

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
                        text: qsTr("Load model to view parameters")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            VoiceDesignSettingsPanel {
                id: settingsPanel
                anchors.fill: parent
                visible: opacity > 0
                opacity: 1.0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
                family: root.family
                selectedLanguage: root.selectedLanguage
                locked: root.inputsLocked
                onSelectedLanguageChanged: root.selectedLanguage = selectedLanguage
                onCloseRequested: root.isSettingsOpen = false
            }
        }
    ]

    FileDialog {
        id: saveDialog
        title: "Save Designed Voice Audio"
        fileMode: FileDialog.SaveFile
        nameFilters: ["WAV files (*.wav)"]
        onAccepted: AppController.preview.saveWav(selectedFile.toString())
    }
}
