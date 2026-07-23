import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    required property var dubbing
    property string configurationMode: "adaptive"
    property bool rewriteEnabled: true
    property string selectedProvider: "lmstudio"
    property string selectedCliAgent: "claude"
    property string connectionMessage: ""
    property bool connectionSuccess: false
    property var availableCliModels: []
    signal localModelRequested()

    function openForMode(mode) {
        configurationMode = mode === "custom" ? "custom" : "adaptive"
        open()
    }

    function cliModelIndex(value) {
        for (var i = 0; i < availableCliModels.length; ++i) {
            if (availableCliModels[i].value === value)
                return i
        }
        return -1
    }

    function refreshCliModels(preferredModel) {
        var requested = preferredModel || modelField.text.trim() || "default"
        var options = dubbing.translationFixCliModelOptions(selectedCliAgent) || []
        var found = false
        for (var i = 0; i < options.length; ++i) {
            if (options[i].value === requested) {
                found = true
                break
            }
        }
        if (!found && requested !== "") {
            options = options.concat([{
                value: requested,
                text: requested,
                detail: qsTr("Saved custom model")
            }])
        }
        availableCliModels = options
        cliModelCombo.currentIndex = cliModelIndex(requested)
        if (cliModelCombo.currentIndex < 0 && availableCliModels.length > 0)
            cliModelCombo.currentIndex = 0
        if (cliModelCombo.currentIndex >= 0)
            modelField.text = availableCliModels[cliModelCombo.currentIndex].value
    }

    function selectCliAgent(agent) {
        selectedCliAgent = agent
        connectionSuccess = false
        connectionMessage = ""
        refreshCliModels("default")
    }

    function loadConfiguration() {
        var config = dubbing.translationFixConfiguration || {}
        selectedProvider = config.provider || "lmstudio"
        selectedCliAgent = config.cliAgent || "claude"
        serverUrlField.text = config.serverUrl || "http://127.0.0.1:1234"
        modelField.text = config.model || (selectedProvider === "cli" ? "default" : "qwen3.5-2b")
        apiKeyField.text = config.apiKey || ""
        refreshCliModels(modelField.text)
        connectionMessage = ""
        connectionSuccess = false
        rewriteEnabled = configurationMode !== "custom"
                         || (dubbing.durationControl || {}).autoRewrite !== false
    }

    function currentConfiguration() {
        var saved = dubbing.translationFixConfiguration || {}
        return {
            provider: selectedProvider,
            cliAgent: selectedCliAgent,
            serverUrl: serverUrlField.text.trim(),
            model: modelField.text.trim(),
            runtimeId: selectedProvider === "local" ? (saved.runtimeId || "") : "",
            runtimeVersion: selectedProvider === "local" ? (saved.runtimeVersion || "") : "",
            selectedFiles: selectedProvider === "local" ? (saved.selectedFiles || ({})) : ({}),
            apiKey: apiKeyField.text.trim(),
            maxAttempts: Number((dubbing.durationControl || {}).maxPreTtsIterations || 4),
            temperature: 0.35
        }
    }

    function localModelConfiguredState() {
        var config = dubbing.translationFixConfiguration || {}
        return selectedProvider === "local" && !!config.configured
            && !!config.model && !!config.runtimeId
    }

    function localModelConfigured(familyId, runtimeId, runtimeVersion, selectedFiles) {
        selectedProvider = "local"
        modelField.text = familyId
        var config = currentConfiguration()
        config.runtimeId = runtimeId
        config.runtimeVersion = runtimeVersion
        config.selectedFiles = selectedFiles || ({})
        dubbing.setAdaptiveConfiguration(config)
        dubbing.dubbingQuality = configurationMode
        connectionMessage = qsTr("Local LLM configuration saved. The model is not loaded from Settings.")
        connectionSuccess = true
    }

    function applyConfiguration() {
        if (configurationMode === "custom") {
            var duration = Object.assign({}, dubbing.durationControl || {})
            duration.autoRewrite = rewriteEnabled
            dubbing.durationControl = duration
        }
        if (configurationMode !== "custom" || rewriteEnabled)
            dubbing.setAdaptiveConfiguration(currentConfiguration())
        dubbing.dubbingQuality = configurationMode
        close()
    }

    onOpened: loadConfiguration()

    Connections {
        target: root.dubbing
        function onTranslationFixConnectionTested(success, message) {
            root.connectionSuccess = success
            root.connectionMessage = message
        }
    }

    parent: Overlay.overlay
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    width: Math.min(720, parent ? parent.width - Theme.paddingXL * 2 : 720)
    height: Math.min(650, parent ? parent.height - Theme.paddingXL * 2 : 650)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium
            Rectangle {
                Layout.preferredWidth: 38; Layout.preferredHeight: 38
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                LineIcon { anchors.centerIn: parent; name: "spark"; color: Theme.accentLight; width: 19; height: 19 }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text {
                    text: root.configurationMode === "custom"
                          ? qsTr("Configure Custom dubbing") : qsTr("Configure Adaptive dubbing")
                    color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: root.configurationMode === "custom"
                          ? qsTr("Control long-translation rewriting and choose its model source.")
                          : qsTr("Choose the LLM used for context-aware translation and timing repair.")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight
                }
            }
            PrimaryButton { iconName: "close"; iconOnly: true; quiet: true; onClicked: root.close() }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge
                    Layout.rightMargin: Theme.paddingLarge
                    Layout.topMargin: Theme.paddingLarge
                    implicitHeight: rewriteLayout.implicitHeight + Theme.paddingMedium * 2
                    visible: root.configurationMode === "custom"
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: Qt.rgba(1, 1, 1, 0.08)

                    RowLayout {
                        id: rewriteLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        spacing: Theme.paddingMedium
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Rewrite overlong translations")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("When disabled, segments outside the duration limit remain available for manual review.")
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                        }
                        Switch {
                            checked: root.rewriteEnabled
                            onToggled: {
                                root.rewriteEnabled = checked
                                root.connectionMessage = ""
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    text: qsTr("LLM SOURCE")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall
                    enabled: root.configurationMode !== "custom" || root.rewriteEnabled
                    opacity: enabled ? 1.0 : 0.45
                    ProviderRow {
                        title: qsTr("LLM API")
                        description: qsTr("OpenAI-compatible remote or self-hosted API")
                        iconName: "globe"
                        selected: root.selectedProvider === "api"
                        privacyText: qsTr("External API")
                        onClicked: { root.selectedProvider = "api"; root.connectionSuccess = false; if (serverUrlField.text === "http://127.0.0.1:1234") serverUrlField.text = "" }
                    }
                    ProviderRow {
                        title: qsTr("LM Studio")
                        description: qsTr("Use the local LM Studio server")
                        iconName: "activity"
                        selected: root.selectedProvider === "lmstudio"
                        privacyText: qsTr("Local")
                        onClicked: { root.selectedProvider = "lmstudio"; root.connectionSuccess = false; if (serverUrlField.text === "") serverUrlField.text = "http://127.0.0.1:1234" }
                    }
                    ProviderRow {
                        title: qsTr("LA Studio model")
                        description: qsTr("Choose a supported local LLM for the workflow")
                        iconName: "cpu"
                        selected: root.selectedProvider === "local"
                        privacyText: root.localModelConfiguredState() ? qsTr("Configured") : qsTr("Local")
                        onClicked: { root.selectedProvider = "local"; root.connectionSuccess = false }
                    }
                    ProviderRow {
                        title: qsTr("Local CLI Agent")
                        description: qsTr("Use installed Claude Code, Codex, or Antigravity CLI")
                        iconName: "spark"
                        selected: root.selectedProvider === "cli"
                        privacyText: qsTr("CLI")
                        onClicked: { root.selectedProvider = "cli"; root.connectionSuccess = false; if (modelField.text === "qwen3.5-2b" || modelField.text === "") modelField.text = "default" }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall
                    visible: root.selectedProvider === "api" || root.selectedProvider === "lmstudio"
                    enabled: root.configurationMode !== "custom" || root.rewriteEnabled
                    Text { text: root.selectedProvider === "api" ? qsTr("API base URL") : qsTr("LM Studio server URL"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: serverUrlField; Layout.fillWidth: true; placeholderText: root.selectedProvider === "api" ? "https://api.example.com" : "http://127.0.0.1:1234"; onTextEdited: root.connectionSuccess = false }
                    Text { Layout.topMargin: Theme.paddingSmall; text: qsTr("Model identifier"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: modelField; Layout.fillWidth: true; placeholderText: root.selectedProvider === "api" ? "model-id" : "qwen3.5-2b"; onTextEdited: root.connectionSuccess = false }
                    Text { Layout.topMargin: Theme.paddingSmall; text: qsTr("API key (optional for local servers)"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: apiKeyField; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Stored locally in LA Studio settings"); onTextEdited: root.connectionSuccess = false }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall
                    visible: root.selectedProvider === "cli"
                    enabled: root.configurationMode !== "custom" || root.rewriteEnabled
                    Text { text: qsTr("CLI Agent Provider"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        PrimaryButton {
                            text: "Claude Code"
                            quiet: root.selectedCliAgent !== "claude"
                            onClicked: root.selectCliAgent("claude")
                        }
                        PrimaryButton {
                            text: "Codex CLI"
                            quiet: root.selectedCliAgent !== "codex"
                            onClicked: root.selectCliAgent("codex")
                        }
                        PrimaryButton {
                            text: "Antigravity"
                            quiet: root.selectedCliAgent !== "antigravity"
                            onClicked: root.selectCliAgent("antigravity")
                        }
                    }
                    Text { Layout.topMargin: Theme.paddingSmall; text: qsTr("CLI model"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        AppComboBox {
                            id: cliModelCombo
                            Layout.fillWidth: true
                            implicitHeight: 36
                            model: root.availableCliModels
                            textRole: "text"
                            secondaryTextRole: "detail"
                            valueRole: "value"
                            searchable: root.availableCliModels.length > 6
                            onActivated: function(index) {
                                if (index < 0 || index >= root.availableCliModels.length)
                                    return
                                modelField.text = root.availableCliModels[index].value
                                root.connectionSuccess = false
                                root.connectionMessage = ""
                            }
                        }
                        PrimaryButton {
                            text: qsTr("Refresh")
                            iconName: "activity"
                            quiet: true
                            onClicked: {
                                root.refreshCliModels(modelField.text)
                                root.connectionSuccess = false
                                root.connectionMessage = qsTr("Model list refreshed from local CLI configuration.")
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Models are read locally from each CLI's settings and cache. Select another model when the current model has no remaining quota.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    implicitHeight: localLayout.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    visible: root.selectedProvider === "local"
                    enabled: root.configurationMode !== "custom" || root.rewriteEnabled
                    RowLayout {
                        id: localLayout
                        anchors.fill: parent; anchors.margins: Theme.paddingMedium; spacing: Theme.paddingMedium
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Text { text: root.localModelConfiguredState() ? qsTr("Local LLM configured") : qsTr("Select an LLM and runtime"); color: root.localModelConfiguredState() ? Theme.success : Theme.warning; font.pixelSize: Theme.fontSmall; font.bold: true }
                            Text { Layout.fillWidth: true; text: qsTr("This setting stores the model and runtime selection without loading them."); color: Theme.textSecondary; font.pixelSize: 10; wrapMode: Text.WordWrap }
                        }
                        PrimaryButton { text: qsTr("Choose model"); iconName: "gallery"; quiet: true; onClicked: root.localModelRequested() }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    implicitHeight: statusText.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: root.connectionMessage === "" ? Qt.rgba(1, 1, 1, 0.08)
                                  : Qt.rgba((root.connectionSuccess ? Theme.success : Theme.warning).r,
                                            (root.connectionSuccess ? Theme.success : Theme.warning).g,
                                            (root.connectionSuccess ? Theme.success : Theme.warning).b, 0.35)
                    Text {
                        id: statusText
                        anchors.fill: parent; anchors.margins: Theme.paddingMedium
                        text: root.connectionMessage !== "" ? root.connectionMessage
                              : (root.configurationMode === "custom" && !root.rewriteEnabled
                                 ? qsTr("Automatic rewriting is disabled; no rewrite model is required.")
                                 : qsTr("A rewrite model is required before final generation."))
                        color: root.connectionMessage === "" ? Theme.textSecondary
                              : (root.connectionSuccess ? Theme.success : Theme.warning)
                        font.pixelSize: Theme.fontSmall; wrapMode: Text.WordWrap
                    }
                }
                Item { Layout.preferredHeight: Theme.paddingSmall }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }
        RowLayout {
            Layout.fillWidth: true; Layout.margins: Theme.paddingMedium; spacing: Theme.paddingSmall
            PrimaryButton {
                visible: root.selectedProvider !== "local"
                         && (root.configurationMode !== "custom" || root.rewriteEnabled)
                text: qsTr("Test connection"); iconName: "activity"; quiet: true
                enabled: root.selectedProvider === "cli" ? true : (serverUrlField.text.trim() !== "" && modelField.text.trim() !== "")
                onClicked: { root.connectionMessage = qsTr("Checking provider…"); root.connectionSuccess = false; root.dubbing.testTranslationFixConnection(root.currentConfiguration()) }
            }
            Item { Layout.fillWidth: true }
            PrimaryButton { text: qsTr("Cancel"); quiet: true; onClicked: root.close() }
            PrimaryButton {
                text: root.configurationMode === "custom" ? qsTr("Use Custom") : qsTr("Use Adaptive")
                iconName: root.configurationMode === "custom" ? "sliders" : "spark"
                enabled: root.configurationMode === "custom" && !root.rewriteEnabled
                         ? true
                         : (root.selectedProvider === "local"
                            ? root.localModelConfiguredState() : root.connectionSuccess)
                onClicked: root.applyConfiguration()
            }
        }
    }

    component ConfigField: TextField {
        color: Theme.textPrimary; placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall; selectByMouse: true
        leftPadding: Theme.paddingMedium; rightPadding: Theme.paddingMedium; implicitHeight: 36
        background: Rectangle { radius: Theme.radiusSmall; color: Qt.rgba(0, 0, 0, 0.16); border.color: parent.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.09); border.width: parent.activeFocus ? 2 : 1 }
    }

    component ProviderRow: Button {
        id: providerButton
        required property string title
        required property string description
        required property string iconName
        required property string privacyText
        required property bool selected
        Layout.fillWidth: true; implicitHeight: 62; padding: 0
        contentItem: RowLayout {
            spacing: Theme.paddingMedium
            LineIcon { name: providerButton.iconName; color: providerButton.selected ? Theme.accentLight : Theme.textSecondary; Layout.preferredWidth: 19; Layout.preferredHeight: 19; Layout.leftMargin: Theme.paddingMedium }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text { text: providerButton.title; color: Theme.textPrimary; font.pixelSize: Theme.fontSmall; font.bold: true }
                Text { Layout.fillWidth: true; text: providerButton.description; color: Theme.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
            }
            Text { text: providerButton.privacyText; color: providerButton.selected ? Theme.accentLight : Theme.textSecondary; font.pixelSize: 10; font.bold: true; Layout.rightMargin: Theme.paddingMedium }
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: providerButton.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12) : (providerButton.hovered ? Qt.rgba(1, 1, 1, 0.045) : Qt.rgba(1, 1, 1, 0.025))
            border.color: providerButton.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }
}
