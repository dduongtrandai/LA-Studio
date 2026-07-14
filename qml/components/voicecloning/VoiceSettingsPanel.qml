import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../shared/settings"
import "../base"

// Right panel: Advanced settings for voice cloning
ColumnLayout {
    id: root

    property var family: null
    property var dynamicSettings: ({})
    property string styleInstruction: ""
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
    readonly property bool hasInstructInput: studioConfig && studioConfig.inputs ? studioConfig.inputs.indexOf("instruct") !== -1 : false

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
        root.capabilitySchema = AppController.tts.schemaForCapability("voice-cloning")
        root.studioConfig = AppController.tts.studioConfigForCapability("voice-cloning")
        root.basicSchema = splitSchema(root.capabilitySchema, false)
        root.advancedSchema = splitSchema(root.capabilitySchema, true)
        var defaults = {}
        var parameterDefaults = root.studioConfig && root.studioConfig.parameterDefaults ? root.studioConfig.parameterDefaults : ({})
        for (var i = 0; i < root.capabilitySchema.length; ++i) {
            var param = root.capabilitySchema[i] || {}
            if (param.id && parameterDefaults[param.id] !== undefined) {
                defaults[param.id] = parameterDefaults[param.id]
            } else if (param.id && param["default"] !== undefined) {
                defaults[param.id] = param["default"]
            }
        }
        root.dynamicSettings = defaults
        root.styleInstruction = ""
        instructInput.text = ""
    }

    Component.onCompleted: refreshCapabilityMetadata()

    Connections {
        target: AppController.tts
        function onFamilyConfigChanged() { root.refreshCapabilityMetadata() }
        function onSchemaChanged() { root.refreshCapabilityMetadata() }
    }

    signal settingsChanged()
    signal closeRequested()

    function updateDynamicSetting(parameterId, value) {
        var settings = JSON.parse(JSON.stringify(root.dynamicSettings))
        settings[parameterId] = value
        root.dynamicSettings = settings
        root.settingsChanged()
    }

    function applyExampleSettings(exampleSettings) {
        if (!exampleSettings) return
        var settings = JSON.parse(JSON.stringify(root.dynamicSettings))
        for (var i = 0; i < root.capabilitySchema.length; ++i) {
            var param = root.capabilitySchema[i] || {}
            if (param.id && exampleSettings[param.id] !== undefined) {
                settings[param.id] = exampleSettings[param.id]
            }
        }
        root.dynamicSettings = settings
        if (exampleSettings["instruct"] !== undefined) {
            root.styleInstruction = exampleSettings["instruct"]
            instructInput.text = root.styleInstruction
        }
        root.settingsChanged()
    }

    spacing: Theme.paddingMedium
    Layout.fillWidth: true
    Layout.fillHeight: true

    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        spacing: Theme.paddingSmall

        LineIcon {
            name: "sliders"
            color: Theme.accent
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Text {
            text: qsTr("Cloning Settings")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.bold: true
            Layout.fillWidth: true
        }

        Button {
            id: closeBtn
            implicitWidth: 30
            implicitHeight: 30
            flat: true

            ToolTip.text: qsTr("Hide settings")
            ToolTip.visible: hovered

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
                title: qsTr("Core")
                iconName: "file"
                visible: root.hasInstructInput

                FieldLabel { text: (family && family.id && family.id.indexOf("qwen3") !== -1) ? qsTr("Style Instruction") : qsTr("Style & Emotion") }

                TextField {
                    id: instructInput
                    Layout.fillWidth: true
                    visible: root.hasInstructInput
                    placeholderText: (family && family.id && family.id.indexOf("qwen3") !== -1) 
                                     ? qsTr("e.g. Speak with excitement, whisper, or deep voice...") 
                                     : qsTr("happy, whisper, dramatic...")
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    selectionColor: Theme.accent
                    selectedTextColor: "#ffffff"
                    enabled: !root.locked
                    background: Rectangle {
                        color: Qt.rgba(1, 1, 1, 0.035)
                        radius: 7
                        border.color: instructInput.activeFocus ? Qt.rgba(0.49, 0.30, 1.0, 0.75) : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                    }
                    padding: Theme.paddingMedium
                    onTextChanged: {
                        root.styleInstruction = text
                        root.settingsChanged()
                    }
                }
            }

            SettingsSection {
                title: qsTr("Model Parameters")
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
                title: qsTr("Advanced")
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
                title: qsTr("Audio & System")
                iconName: "cpu"

                ToggleRow {
                    id: denoiseToggle
                    text: qsTr("Denoise")
                    checked: true
                    enabled: !root.locked
                    onCheckedChanged: {
                        root.denoise = checked
                        root.settingsChanged()
                    }
                }

                ToggleRow {
                    id: preprocessToggle
                    text: qsTr("Preprocess prompt")
                    checked: true
                    enabled: !root.locked
                    onCheckedChanged: {
                        root.preprocessPrompt = checked
                        root.settingsChanged()
                    }
                }

                ToggleRow {
                    id: randomSeedToggle
                    text: qsTr("Random seed")
                    checked: true
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
                    text: "42"
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

    // Function to get all settings as an object
    function getSettingsObject(languageCode, inputText, referenceText) {
        return VoiceCloningUtils.buildCloneSettings(
            languageCode,
            instructInput.text,
            referenceText,
            denoiseToggle.checked,
            preprocessToggle.checked,
            root.dynamicSettings,
            randomSeedToggle.checked,
            parseInt(customSeedInput.text) || 0
        )
    }
}

