import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    required property var dubbing
    property string selectedProvider: "lmstudio"
    property string connectionMessage: ""
    property bool connectionSuccess: false
    signal localModelRequested()

    function loadConfiguration() {
        var config = dubbing.translationFixConfiguration || {}
        selectedProvider = config.provider || "lmstudio"
        serverUrlField.text = config.serverUrl || "http://127.0.0.1:1234"
        modelField.text = config.model || "qwen3.5-2b"
        apiKeyField.text = config.apiKey || ""
        connectionMessage = ""
        connectionSuccess = false
    }

    function currentConfiguration() {
        var saved = dubbing.translationFixConfiguration || {}
        return {
            provider: selectedProvider,
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
        dubbing.dubbingQuality = "adaptive"
        connectionMessage = qsTr("Local LLM configuration saved. The model is not loaded from Settings.")
        connectionSuccess = true
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
                Text { text: qsTr("Configure Adaptive dubbing"); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                Text { Layout.fillWidth: true; text: qsTr("Choose the LLM used for context-aware translation and timing repair."); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight }
            }
            PrimaryButton { iconName: "close"; iconOnly: true; quiet: true; onClicked: root.close() }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge; Layout.topMargin: Theme.paddingLarge
                    text: qsTr("LLM SOURCE")
                    color: Theme.textSecondary; font.pixelSize: Theme.fontSmall; font.bold: true; font.letterSpacing: 1.1
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall
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
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall
                    visible: root.selectedProvider !== "local"
                    Text { text: root.selectedProvider === "api" ? qsTr("API base URL") : qsTr("LM Studio server URL"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: serverUrlField; Layout.fillWidth: true; placeholderText: root.selectedProvider === "api" ? "https://api.example.com" : "http://127.0.0.1:1234"; onTextEdited: root.connectionSuccess = false }
                    Text { Layout.topMargin: Theme.paddingSmall; text: qsTr("Model identifier"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: modelField; Layout.fillWidth: true; placeholderText: root.selectedProvider === "api" ? "model-id" : "qwen3.5-2b"; onTextEdited: root.connectionSuccess = false }
                    Text { Layout.topMargin: Theme.paddingSmall; text: qsTr("API key (optional for local servers)"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                    ConfigField { id: apiKeyField; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Stored locally in LA Studio settings"); onTextEdited: root.connectionSuccess = false }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge; Layout.rightMargin: Theme.paddingLarge
                    implicitHeight: localLayout.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    visible: root.selectedProvider === "local"
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
                              : qsTr("Adaptive mode requires an LLM source before final generation.")
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
                text: qsTr("Test connection"); iconName: "activity"; quiet: true
                enabled: serverUrlField.text.trim() !== "" && modelField.text.trim() !== ""
                onClicked: { root.connectionMessage = qsTr("Checking provider…"); root.connectionSuccess = false; root.dubbing.testTranslationFixConnection(root.currentConfiguration()) }
            }
            Item { Layout.fillWidth: true }
            PrimaryButton { text: qsTr("Cancel"); quiet: true; onClicked: root.close() }
            PrimaryButton {
                text: qsTr("Use Adaptive"); iconName: "spark"
                enabled: root.selectedProvider === "local" ? root.localModelConfiguredState() : root.connectionSuccess
                onClicked: { root.dubbing.setAdaptiveConfiguration(root.currentConfiguration()); root.dubbing.dubbingQuality = "adaptive"; root.close() }
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
