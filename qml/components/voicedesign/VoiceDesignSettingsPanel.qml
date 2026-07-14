import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../shared/settings"
import "../base"

ColumnLayout {
    id: root

    property var family: null
    property var dynamicSettings: ({})
    property string selectedLanguage: "en"
    property bool denoise: true
    property bool preprocessPrompt: true
    property bool randomSeed: true
    property int customSeed: 42
    property bool locked: false

    property var capabilitySchema: []
    property var basicSchema: []
    property var advancedSchema: []
    property var studioConfig: ({})
    property bool advancedOpen: false
    property bool hasSelectedVoiceAttributes: false
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs ? studioConfig.inputs.indexOf("language") !== -1 : true
    readonly property bool freeTextInstruct: studioConfig && studioConfig.freeTextInstruct !== undefined ? !!studioConfig.freeTextInstruct : true
    readonly property bool requiresInstruct: studioConfig && studioConfig.requiredInputs ? studioConfig.requiredInputs.indexOf("instruct") !== -1 : false
    readonly property bool usesStructuredVoiceDesign: {
        for (var i = 0; i < capabilitySchema.length; ++i) {
            var param = capabilitySchema[i] || {}
            if (isVoiceDesignAttribute(param.id)) return true
        }
        return false
    }

    signal settingsChanged()
    signal closeRequested()

    function splitSchema(schema, advanced) {
        var result = []
        for (var i = 0; i < schema.length; ++i) {
            var item = schema[i] || {}
            if (!!item.advanced === advanced) {
                result.push(item)
            }
        }
        return result
    }

    function refreshCapabilityMetadata() {
        root.capabilitySchema = AppController.tts.schemaForCapability("voice-design")
        root.studioConfig = AppController.tts.studioConfigForCapability("voice-design")
        root.basicSchema = splitSchema(root.capabilitySchema, false)
        root.advancedSchema = splitSchema(root.capabilitySchema, true)
        var defaults = {}
        for (var i = 0; i < root.capabilitySchema.length; ++i) {
            var param = root.capabilitySchema[i] || {}
            if (param.id && param["default"] !== undefined) {
                defaults[param.id] = param["default"]
            }
        }
        root.dynamicSettings = defaults
        root.refreshVoiceDesignState()
    }

    function updateDynamicSetting(paramId, value) {
        var temp = JSON.parse(JSON.stringify(root.dynamicSettings))
        temp[paramId] = value
        root.dynamicSettings = temp
        root.refreshVoiceDesignState()
        root.settingsChanged()
    }

    function isVoiceDesignAttribute(paramId) {
        return paramId && paramId.indexOf("voice_design_") === 0
    }

    function isEnglishTargetLanguage(langCode) {
        return !langCode || langCode === "en" || langCode.indexOf("en-") === 0
    }

    function isChineseTargetLanguage(langCode) {
        return !langCode || langCode === "zh" || langCode.indexOf("zh-") === 0
               || langCode === "cmn" || langCode === "yue" || langCode === "nan"
    }

    function normalizedChoiceValue(param, rawValue) {
        if (rawValue === undefined || rawValue === null)
            return ""

        var cleaned = rawValue.toString().trim()
        if (cleaned === "")
            return ""

        var options = param.choices || param.options || []
        if (!options || options.length === 0)
            return cleaned

        for (var i = 0; i < options.length; ++i) {
            var option = options[i] || {}
            var optionValue = option.value !== undefined && option.value !== null ? option.value.toString().trim() : ""
            var optionText = option.text !== undefined && option.text !== null ? option.text.toString().trim() : ""
            if (cleaned === optionValue || cleaned === optionText)
                return optionValue
        }

        return ""
    }

    function voiceDesignAttributes(langCode) {
        var values = []
        for (var i = 0; i < root.capabilitySchema.length; ++i) {
            var param = root.capabilitySchema[i] || {}
            var paramId = param.id
            if (!isVoiceDesignAttribute(paramId)) continue

            if (paramId === "voice_design_english_accent" && !isEnglishTargetLanguage(langCode)) continue
            if (paramId === "voice_design_chinese_dialect" && !isChineseTargetLanguage(langCode)) continue

            var value = root.dynamicSettings[paramId]
            if (value === undefined && param["default"] !== undefined)
                value = param["default"]
            var normalized = normalizedChoiceValue(param, value)
            if (normalized !== "")
                values.push(normalized)
        }
        return values
    }

    function hasVoiceDesignAttributes(langCode) {
        return voiceDesignAttributes(langCode).length > 0
    }

    function composedVoiceDescription(langCode, instructText) {
        var values = []
        var custom = instructText ? instructText.trim() : ""
        if (root.freeTextInstruct && custom !== "")
            values.push(custom)
        var attrs = voiceDesignAttributes(langCode)
        for (var i = 0; i < attrs.length; ++i)
            values.push(attrs[i])
        return values.join(", ")
    }

    function hasVoiceDescription(langCode, instructText) {
        return composedVoiceDescription(langCode, instructText).trim().length > 0
    }

    function refreshVoiceDesignState() {
        root.hasSelectedVoiceAttributes = voiceDesignAttributes(root.selectedLanguage).length > 0
    }

    onSelectedLanguageChanged: refreshVoiceDesignState()

    function getSynthesisSettings(langCode, instructText) {
        var settings = {}
        for (var i = 0; i < root.capabilitySchema.length; ++i) {
            var param = root.capabilitySchema[i] || {}
            var paramId = param.id
            if (!paramId) continue
            if (isVoiceDesignAttribute(paramId)) continue
            if (paramId === "seed" || paramId === "mg_seed") {
                if (root.randomSeed) {
                    settings[paramId] = -1
                } else {
                    settings[paramId] = root.customSeed
                }
            } else if (root.dynamicSettings[paramId] !== undefined) {
                settings[paramId] = root.dynamicSettings[paramId]
            } else if (param["default"] !== undefined) {
                settings[paramId] = param["default"]
            }
        }
        var acceptsLanguage = root.studioConfig && root.studioConfig.inputs && root.studioConfig.inputs.indexOf("language") !== -1
        if (acceptsLanguage && langCode) {
            settings["lang"] = langCode
        }
        var acceptsInstruct = root.studioConfig && root.studioConfig.inputs && root.studioConfig.inputs.indexOf("instruct") !== -1
        var composedInstruct = composedVoiceDescription(langCode, instructText)
        if (acceptsInstruct && composedInstruct) {
            settings["instruct"] = composedInstruct
        }
        return settings
    }

    Component.onCompleted: refreshCapabilityMetadata()

    Connections {
        target: root
        function onSettingsChanged() { root.refreshVoiceDesignState() }
    }

    Connections {
        target: AppController.tts
        function onFamilyConfigChanged() { root.refreshCapabilityMetadata() }
        function onSchemaChanged() { root.refreshCapabilityMetadata() }
    }

    // Header with title and close button
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        LineIcon {
            name: "settings"
            color: Theme.textPrimary
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Text {
            text: "Generation Settings"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
            Layout.fillWidth: true
        }

        Button {
            id: closeBtn
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            flat: true

            AppToolTip {
                text: "Hide settings"
                visible: parent.hovered
            }

            contentItem: LineIcon {
                name: "chevron-right"
                color: closeBtn.hovered ? Theme.accent : Theme.textSecondary
                anchors.centerIn: parent
                width: 16
                height: 16
            }
            background: Rectangle {
                color: closeBtn.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                border.color: closeBtn.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.55) : Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                radius: 7
            }
            onClicked: root.closeRequested()
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.07) }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width - 16
            spacing: Theme.paddingMedium

            SettingsSection {
                title: "Core"
                iconName: "file"
                visible: root.hasLanguageInput

                LanguageSelector {
                    id: langSelector
                    Layout.fillWidth: true
                    labelText: "Target Language"
                    family: root.family
                    hasLanguageInput: root.hasLanguageInput
                    useTextFieldFallback: false
                    language: root.selectedLanguage
                    enabled: !root.locked
                    onLanguageChanged: {
                        if (root.selectedLanguage !== language) {
                            root.selectedLanguage = language
                            root.settingsChanged()
                        }
                    }
                }
            }

            SettingsSection {
                title: "Model Parameters"
                iconName: "sliders"
                visible: root.basicSchema.length > 0

                ModelParameterControls {
                    enabled: !root.locked
                    schema: root.basicSchema
                    dynamicSettings: root.dynamicSettings
                    onParameterChanged: function(parameterId, value) { root.updateDynamicSetting(parameterId, value) }
                }
            }

            CollapsibleSettingsSection {
                title: "Advanced"
                iconName: "sliders"
                expanded: root.advancedOpen
                visible: root.advancedSchema.length > 0
                onToggled: root.advancedOpen = !root.advancedOpen

                ModelParameterControls {
                    enabled: !root.locked
                    schema: root.advancedSchema
                    dynamicSettings: root.dynamicSettings
                    onParameterChanged: function(parameterId, value) { root.updateDynamicSetting(parameterId, value) }
                }
            }

            SettingsSection {
                title: "Audio & System"
                visible: true
                iconName: "cpu"

                ToggleRow {
                    id: denoiseToggle
                    text: "Denoise"
                    checked: root.denoise
                    enabled: !root.locked
                    onCheckedChanged: {
                        root.denoise = checked
                        root.settingsChanged()
                    }
                }

                ToggleRow {
                    id: preprocessToggle
                    text: "Preprocess prompt"
                    checked: root.preprocessPrompt
                    enabled: !root.locked
                    onCheckedChanged: {
                        root.preprocessPrompt = checked
                        root.settingsChanged()
                    }
                }

                ToggleRow {
                    id: randomSeedToggle
                    text: "Random seed"
                    checked: root.randomSeed
                    enabled: !root.locked
                    onCheckedChanged: {
                        root.randomSeed = checked
                        root.settingsChanged()
                    }
                }

                TextField {
                    id: customSeedInput
                    Layout.fillWidth: true
                    visible: !randomSeedToggle.checked
                    enabled: !root.locked
                    text: root.customSeed.toString()
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    validator: IntValidator { bottom: 0 }
                    background: Rectangle {
                        color: Qt.rgba(1, 1, 1, 0.035)
                        radius: 7
                        border.color: customSeedInput.activeFocus ? Qt.rgba(0.49, 0.30, 1.0, 0.85) : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                    }
                    padding: Theme.paddingMedium
                    onTextChanged: {
                        root.customSeed = parseInt(text) || 0
                        root.settingsChanged()
                    }
                }
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.paddingSmall }
        }
    }
}
