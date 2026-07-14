import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../components"
import "../components/base"
import "../components/shared"
import "../components/shared/settings"

Rectangle {
    id: root

    color: Theme.background

    readonly property int contentHorizontalPadding: width >= 1600 ? 24 : (width >= 1100 ? 20 : 14)
    readonly property int contentVerticalPadding: width >= 1100 ? Theme.paddingLarge : Theme.paddingMedium
    readonly property int fieldLabelWidth: 132
    readonly property bool wideLayout: width >= 1120
    readonly property string configuredBaseUrl: "http://" + (AppController.apiServer.allowLan ? "0.0.0.0" : "127.0.0.1") + ":" + AppController.apiServer.port
    readonly property string activeBaseUrl: AppController.apiServer.baseUrl !== "" ? AppController.apiServer.baseUrl : configuredBaseUrl
    readonly property var endpoints: [
        { method: "GET", path: "/health", note: qsTr("Server health, settings, and loaded engine readiness.") },
        { method: "GET", path: "/v1/models", note: qsTr("OpenAI-style list of every loaded resident model instance.") },
        { method: "GET", path: "/v1/audio/voices", note: qsTr("Voice choices exposed by the current TTS model.") },
        { method: "POST", path: "/v1/audio/speech", note: qsTr("Create WAV or PCM speech from text.") },
        { method: "POST", path: "/v1/audio/transcriptions", note: qsTr("Transcribe multipart audio upload or audio_base64 JSON.") }
    ]
    readonly property var loadedModelItems: (function() {
        var rows = []
        root.appendLoadedModelItems(rows, ttsPickerController, qsTr("Text to Speech"), "/v1/audio/speech")
        root.appendLoadedModelItems(rows, sttPickerController, qsTr("Speech to Text"), "/v1/audio/transcriptions")
        root.appendLoadedModelItems(rows, clonePickerController, qsTr("Voice Cloning"), "/v1/audio/voices")
        root.appendLoadedModelItems(rows, designPickerController, qsTr("Voice Design"), "/v1/audio/voice_designs")
        return rows
    })()
    property string pickerCapability: "tts"
    readonly property var pickerController: {
        if (pickerCapability === "stt") return sttPickerController
        if (pickerCapability === "voice-cloning") return clonePickerController
        if (pickerCapability === "voice-design") return designPickerController
        return ttsPickerController
    }

    function statusText() {
        if (AppController.apiServer.running) return qsTr("Running")
        if (AppController.apiServer.enabled) return qsTr("Starting")
        return qsTr("Stopped")
    }

    function statusColor() {
        if (AppController.apiServer.running) return Theme.success
        if (AppController.apiServer.enabled) return Theme.warning
        return Theme.textSecondary
    }

    function modelText(kind) {
        if (kind === "tts") {
            return AppController.settings.selectedTtsFamily !== "" ? AppController.settings.selectedTtsFamily : qsTr("No TTS model selected")
        }
        if (kind === "stt") {
            return AppController.settings.selectedSttFamily !== "" ? AppController.settings.selectedSttFamily : qsTr("No STT model selected")
        }
        if (kind === "voice-cloning") {
            return clonePickerController.selectedFamilyId !== "" ? clonePickerController.selectedFamilyId : qsTr("No voice cloning model selected")
        }
        if (kind === "voice-design") {
            return designPickerController.selectedFamilyId !== "" ? designPickerController.selectedFamilyId : qsTr("No voice design model selected")
        }
        return qsTr("No model selected")
    }

    function selectedFamilyId(kind) {
        if (kind === "tts") return AppController.settings.selectedTtsFamily
        if (kind === "stt") return AppController.settings.selectedSttFamily
        if (kind === "voice-cloning") return clonePickerController.selectedFamilyId
        if (kind === "voice-design") return designPickerController.selectedFamilyId
        return ""
    }

    function appendLoadedModelItems(rows, controller, label, endpoint) {
        if (!controller || !controller.loadedModels) {
            return
        }

        var models = controller.loadedModels
        for (var i = 0; i < models.length; i++) {
            var model = models[i]
            rows.push({
                label: label,
                modelName: model.title || qsTr("Loaded model"),
                modelId: model.id || "",
                runtime: model.runtime || "",
                endpoint: endpoint,
                status: model.status || qsTr("Ready"),
                active: !!model.active,
                controller: controller
            })
        }
    }

    function firstLoadedModelId(controller) {
        if (!controller || !controller.loadedModels || controller.loadedModels.length === 0) {
            return ""
        }
        for (var i = 0; i < controller.loadedModels.length; i++) {
            if (controller.loadedModels[i].active) {
                return controller.loadedModels[i].id || ""
            }
        }
        return controller.loadedModels[0].id || ""
    }

    function openLoadModelMenu() {
        loadModelPopup.open()
    }

    function openModelPicker(capability, familyId) {
        pickerCapability = capability
        pickerController.openConfiguration(familyId)
        modelPickerDialog.open()
    }

    StudioPageController {
        id: ttsPickerController
        autoLoadOnSync: false
        capabilityId: "tts"
    }

    StudioPageController {
        id: sttPickerController
        autoLoadOnSync: false
        capabilityId: "stt"
    }

    StudioPageController {
        id: clonePickerController
        autoLoadOnSync: false
        capabilityId: "voice-cloning"
    }

    StudioPageController {
        id: designPickerController
        autoLoadOnSync: false
        capabilityId: "voice-design"
    }

    Popup {
        id: loadModelPopup
        parent: Overlay.overlay
        width: 260
        padding: 8
        x: {
            const point = loadModelButton.mapToItem(parent, 0, loadModelButton.height + 8)
            return Math.round(Math.max(12, Math.min(point.x + loadModelButton.width - width, parent.width - width - 12)))
        }
        y: {
            const point = loadModelButton.mapToItem(parent, 0, loadModelButton.height + 8)
            return Math.round(Math.max(12, Math.min(point.y, parent.height - implicitHeight - 12)))
        }
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.surface
            border.color: Qt.rgba(255, 255, 255, 0.08)
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Text {
                text: qsTr("Load Model")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
            }

            Repeater {
                model: [
                    { id: "tts", label: qsTr("Text to Speech"), icon: "mic" },
                    { id: "stt", label: qsTr("Speech to Text"), icon: "waveform" },
                    { id: "voice-cloning", label: qsTr("Voice Cloning"), icon: "users" },
                    { id: "voice-design", label: qsTr("Voice Design"), icon: "sparkles" }
                ]

                delegate: PrimaryButton {
                    text: modelData.label
                    iconName: modelData.icon
                    buttonColor: Theme.surfaceAlt
                    quiet: true
                    Layout.fillWidth: true
                    implicitHeight: 34
                    onClicked: {
                        loadModelPopup.close()
                        root.openModelPicker(modelData.id, root.selectedFamilyId(modelData.id))
                    }
                }
            }
        }
    }

    Dialog {
        id: modelPickerDialog
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        parent: Overlay.overlay
        width: Math.min(1260, Math.max(980, parent.width - 48))
        height: Math.min(780, Math.max(560, parent.height - 48))
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        background: Rectangle {
            color: Qt.rgba(0.06, 0.06, 0.09, 0.94)
            radius: Theme.radiusMedium
            border.color: Qt.rgba(1, 1, 1, 0.10)
            border.width: 1
        }

        contentItem: Item {
            width: modelPickerDialog.width
            height: modelPickerDialog.height

            CapabilityGallery {
                id: modelGallery
                anchors.fill: parent
                capability: root.pickerCapability
                modalMode: true
                familiesModel: root.pickerController.familiesModel
                selectedFamilyId: root.pickerController.selectedFamilyId
                onFamilySelected: function(familyId) {
                    initialSelectedFiles = ({})
                    root.pickerController.selectFamily(familyId)
                }
                onConfigurationAccepted: function(familyId, runtimeId, runtimeVersion, selectedFiles) {
                    root.pickerController.commitConfigurationSelection(familyId, runtimeId, runtimeVersion, selectedFiles)
                }
            }
        }
    }

    Connections {
        target: ttsPickerController
        function onConfigurationDialogClosed() {
            if (root.pickerCapability === "tts") modelPickerDialog.close()
        }
    }

    Connections {
        target: sttPickerController
        function onConfigurationDialogClosed() {
            if (root.pickerCapability === "stt") modelPickerDialog.close()
        }
    }

    Connections {
        target: clonePickerController
        function onConfigurationDialogClosed() {
            if (root.pickerCapability === "voice-cloning") modelPickerDialog.close()
        }
    }

    Connections {
        target: designPickerController
        function onConfigurationDialogClosed() {
            if (root.pickerCapability === "voice-design") modelPickerDialog.close()
        }
    }

    function curlSpeechExample() {
        var auth = AppController.apiServer.apiKey !== "" ? " \\\n  -H \"Authorization: Bearer " + AppController.apiServer.apiKey + "\"" : ""
        var model = root.firstLoadedModelId(ttsPickerController)
        if (model === "") model = "your-loaded-model-id"
        return "curl " + activeBaseUrl + "/v1/audio/speech \\\n  -H \"Content-Type: application/json\"" + auth + " \\\n  -d \"{\\\"model\\\":\\\"" + model + "\\\",\\\"input\\\":\\\"Xin chao LA Studio\\\",\\\"response_format\\\":\\\"wav\\\"}\" \\\n  --output speech.wav"
    }

    function curlSttExample() {
        var auth = AppController.apiServer.apiKey !== "" ? " \\\n  -H \"Authorization: Bearer " + AppController.apiServer.apiKey + "\"" : ""
        var model = root.firstLoadedModelId(sttPickerController)
        var modelForm = model !== "" ? " \\\n  -F \"model=" + model + "\"" : ""
        return "curl " + activeBaseUrl + "/v1/audio/transcriptions" + auth + " \\\n  -F \"file=@sample.wav\"" + modelForm + " \\\n  -F \"language=auto\""
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: Math.max(760, scroll.availableWidth - root.contentHorizontalPadding * 2)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingLarge

            Item { Layout.preferredHeight: root.contentVerticalPadding }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingLarge

                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 10
                    color: Qt.rgba(0.49, 0.30, 1.0, 0.14)
                    border.color: Qt.rgba(0.64, 0.49, 1.0, 0.35)
                    border.width: 1

                    LineIcon {
                        anchors.centerIn: parent
                        name: "code"
                        color: Theme.accentLight
                        width: 24
                        height: 24
                        strokeWidth: 1.9
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: qsTr("Developer")
                        color: Theme.textPrimary
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Text {
                        text: qsTr("Run LA Studio as a local audio API for tools, scripts, and client apps.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                StatusPill {
                    label: root.statusText()
                    colorValue: root.statusColor()
                }

                PrimaryButton {
                    text: AppController.apiServer.enabled ? qsTr("Stop Server") : qsTr("Start Server")
                    iconName: AppController.apiServer.enabled ? "stop" : "play"
                    implicitWidth: 140
                    implicitHeight: 36
                    onClicked: AppController.apiServer.enabled = !AppController.apiServer.enabled
                }

                PrimaryButton {
                    id: loadModelButton
                    text: qsTr("Load Model")
                    iconName: "gallery"
                    buttonColor: Theme.surfaceAlt
                    implicitWidth: 120
                    implicitHeight: 36
                    quiet: true
                    onClicked: {
                        if (loadModelPopup.opened) {
                            loadModelPopup.close()
                        } else {
                            root.openLoadModelMenu()
                        }
                    }
                }
            }

            Panel {
                title: qsTr("Local Server")
                subtitle: qsTr("OpenAI-compatible audio endpoints backed by the models already loaded in LA Studio.")
                Layout.fillWidth: true

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    LineIcon {
                        name: AppController.apiServer.running ? "activity" : "power"
                        color: root.statusColor()
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                    }

                    Text {
                        text: AppController.apiServer.running ? qsTr("Listening at") : qsTr("Configured at")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }

                    CodePill {
                        text: root.activeBaseUrl
                        Layout.fillWidth: true
                    }

                    PrimaryButton {
                        text: qsTr("Copy URL")
                        iconName: "copy"
                        buttonColor: Theme.surfaceAlt
                        implicitWidth: 104
                        implicitHeight: 32
                        quiet: true
                        onClicked: AppController.copyToClipboard(root.activeBaseUrl)
                    }
                }

                ThinDivider {}

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wideLayout ? 4 : 2
                    columnSpacing: Theme.paddingLarge
                    rowSpacing: Theme.paddingMedium

                    ServerToggle {
                        title: qsTr("Server")
                        detail: AppController.apiServer.enabled ? qsTr("Accepting requests") : qsTr("Stopped")
                        checked: AppController.apiServer.enabled
                        onToggled: AppController.apiServer.enabled = checked
                    }

                    ServerToggle {
                        title: qsTr("LAN Access")
                        detail: AppController.apiServer.allowLan ? qsTr("Bind 0.0.0.0") : qsTr("Localhost only")
                        checked: AppController.apiServer.allowLan
                        onToggled: AppController.apiServer.allowLan = checked
                    }

                    FieldBlock {
                        title: qsTr("Port")
                        detail: qsTr("HTTP listen port")
                        TextField {
                            id: apiPortField
                            text: String(AppController.apiServer.port)
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 65535 }
                            Layout.fillWidth: true
                            implicitHeight: 32
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            selectionColor: Theme.accent
                            selectedTextColor: "#ffffff"
                            font.pixelSize: Theme.fontMedium
                            leftPadding: Theme.paddingSmall
                            rightPadding: Theme.paddingSmall
                            selectByMouse: true
                            verticalAlignment: Text.AlignVCenter
                            background: Rectangle {
                                color: Qt.rgba(0, 0, 0, 0.15)
                                radius: Theme.radiusSmall
                                border.color: apiPortField.activeFocus ? Theme.accent : Qt.rgba(255, 255, 255, 0.08)
                                border.width: 1
                            }
                            onEditingFinished: {
                                const value = parseInt(text, 10)
                                if (!isNaN(value)) AppController.apiServer.port = value
                            }

                            Connections {
                                target: AppController.apiServer
                                function onPortChanged() {
                                    apiPortField.text = String(AppController.apiServer.port)
                                }
                            }
                        }
                    }

                    FieldBlock {
                        title: qsTr("API Key")
                        detail: AppController.apiServer.allowLan ? qsTr("Required on LAN") : qsTr("Optional locally")
                        TextField {
                            id: apiKeyField
                            text: AppController.apiServer.apiKey
                            placeholderText: qsTr("No key configured")
                            echoMode: TextInput.Normal
                            Layout.fillWidth: true
                            implicitHeight: 32
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            selectionColor: Theme.accent
                            selectedTextColor: "#ffffff"
                            font.pixelSize: Theme.fontMedium
                            leftPadding: Theme.paddingSmall
                            rightPadding: Theme.paddingSmall
                            selectByMouse: true
                            verticalAlignment: Text.AlignVCenter
                            background: Rectangle {
                                color: Qt.rgba(0, 0, 0, 0.15)
                                radius: Theme.radiusSmall
                                border.color: apiKeyField.activeFocus ? Theme.accent : Qt.rgba(255, 255, 255, 0.08)
                                border.width: 1
                            }
                            onEditingFinished: AppController.apiServer.apiKey = text

                            Connections {
                                target: AppController.apiServer
                                function onApiKeyChanged() {
                                    apiKeyField.text = AppController.apiServer.apiKey
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Item { Layout.fillWidth: true }

                    PrimaryButton {
                        text: qsTr("Regenerate Key")
                        buttonColor: Theme.surfaceAlt
                        implicitWidth: 132
                        implicitHeight: 32
                        quiet: true
                        onClicked: AppController.apiServer.apiKey = ""
                    }

                    PrimaryButton {
                        text: qsTr("Copy Key")
                        iconName: "copy"
                        buttonColor: Theme.surfaceAlt
                        implicitWidth: 104
                        implicitHeight: 32
                        quiet: true
                        enabled: AppController.apiServer.apiKey !== ""
                        onClicked: AppController.copyToClipboard(AppController.apiServer.apiKey)
                    }
                }

                Text {
                    visible: AppController.apiServer.lastError !== ""
                    text: AppController.apiServer.lastError
                    color: Theme.danger
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Panel {
                title: qsTr("Loaded Models")
                subtitle: qsTr("Every resident model is listed here. Use the model id in API calls to route requests to a specific loaded instance.")
                Layout.fillWidth: true

                EmptyState {
                    visible: root.loadedModelItems.length === 0
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    visible: root.loadedModelItems.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: root.loadedModelItems

                        delegate: LoadedModelRow {
                            Layout.fillWidth: true
                            label: modelData.label
                            modelName: modelData.modelName
                            runtime: modelData.runtime
                            endpoint: modelData.endpoint
                            status: modelData.status
                            active: modelData.active
                            modelId: modelData.modelId
                            controller: modelData.controller
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.wideLayout ? 2 : 1
                columnSpacing: Theme.paddingLarge
                rowSpacing: Theme.paddingLarge

                Panel {
                    title: qsTr("Supported Endpoints")
                    subtitle: qsTr("The API surface is intentionally small and focused on audio workflows.")
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: root.wideLayout ? 560 : 0

                    Repeater {
                        model: root.endpoints
                        delegate: EndpointRow {
                            method: modelData.method
                            path: modelData.path
                            note: modelData.note
                        }
                    }
                }

                Panel {
                    title: qsTr("API Usage")
                    subtitle: qsTr("Copy a command and call the local server from another app or script.")
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: root.wideLayout ? 520 : 0

                    CodeExample {
                        title: qsTr("Create speech")
                        code: root.curlSpeechExample()
                    }

                    CodeExample {
                        title: qsTr("Transcribe audio")
                        code: root.curlSttExample()
                    }
                }
            }

            Item { Layout.preferredHeight: root.contentVerticalPadding }
        }
    }

    component Panel: Rectangle {
        property string title: ""
        property string subtitle: ""
        default property alias content: panelContent.data

        implicitHeight: panelLayout.implicitHeight + Theme.paddingLarge * 2
        radius: Theme.radiusMedium
        color: Qt.rgba(255, 255, 255, 0.022)
        border.color: Qt.rgba(255, 255, 255, 0.075)
        border.width: 1

        ColumnLayout {
            id: panelLayout
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    visible: subtitle !== ""
                    text: subtitle
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                id: panelContent
                Layout.fillWidth: true
                spacing: Theme.paddingMedium
            }
        }
    }

    component EmptyState: Rectangle {
        implicitHeight: 92
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.018)
        border.color: Qt.rgba(255, 255, 255, 0.06)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            LineIcon {
                name: "gallery"
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                color: Theme.textSecondary
                strokeWidth: 1.8
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("No loaded models")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("Use Load Model to choose a workflow and load a model into the local API server.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }

    component LoadedModelRow: Rectangle {
        property string label: ""
        property string modelName: ""
        property string runtime: ""
        property string endpoint: ""
        property string status: ""
        property bool active: false
        property string modelId: ""
        property var controller: null

        implicitHeight: 82
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.018)
        border.color: Qt.rgba(255, 255, 255, 0.06)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            spacing: Theme.paddingMedium

            StatusPill {
                label: active ? qsTr("DEFAULT") : qsTr("LOADED")
                colorValue: Theme.success
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Text {
                        text: label
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: runtime !== ""
                        text: runtime
                        color: Theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    CodePill {
                        text: modelName
                        Layout.fillWidth: true
                    }

                    CodePill {
                        text: modelId !== "" ? modelId : endpoint
                        Layout.preferredWidth: 230
                    }
                }
            }

            PrimaryButton {
                text: qsTr("Unload")
                iconName: "power"
                buttonColor: Qt.rgba(1.0, 0.28, 0.28, 0.20)
                borderColor: Qt.rgba(1.0, 0.28, 0.28, 0.36)
                textColor: "#ff8a8a"
                quiet: true
                implicitWidth: 104
                implicitHeight: 32
                enabled: controller !== null && status !== "Loading" && status !== "Unloading"
                onClicked: {
                    if (controller) {
                        controller.unloadLoadedModel(modelId)
                    }
                }
            }
        }
    }

    component ServerToggle: Rectangle {
        property string title: ""
        property string detail: ""
        property bool checked: false
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: 82
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.028)
        border.color: checked ? Qt.rgba(0.64, 0.49, 1.0, 0.32) : Qt.rgba(255, 255, 255, 0.07)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: detail
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            ToggleRow {
                text: ""
                checked: parent.parent.checked
                Layout.preferredWidth: 42
                onToggled: parent.parent.toggled(checked)
            }
        }
    }

    component FieldBlock: Rectangle {
        property string title: ""
        property string detail: ""
        default property alias content: slot.data

        Layout.fillWidth: true
        implicitHeight: 82
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.028)
        border.color: Qt.rgba(255, 255, 255, 0.07)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                Text {
                    text: title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: detail
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.maximumWidth: 130
                }
            }

            RowLayout {
                id: slot
                Layout.fillWidth: true
                spacing: Theme.paddingSmall
            }
        }
    }

    component EndpointRow: Rectangle {
        property string method: ""
        property string path: ""
        property string note: ""

        Layout.fillWidth: true
        implicitHeight: 58
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.018)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            spacing: Theme.paddingMedium

            Rectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 24
                radius: 5
                color: method === "GET" ? Qt.rgba(0.20, 0.78, 0.42, 0.13) : Qt.rgba(0.49, 0.30, 1.0, 0.15)
                border.color: method === "GET" ? Qt.rgba(0.20, 0.78, 0.42, 0.38) : Qt.rgba(0.49, 0.30, 1.0, 0.38)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: method
                    color: method === "GET" ? Theme.success : Theme.accentLight
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: path
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.family: "Consolas"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: note
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            PrimaryButton {
                text: qsTr("Copy")
                iconName: "copy"
                buttonColor: Theme.surfaceAlt
                implicitWidth: 78
                implicitHeight: 30
                quiet: true
                onClicked: AppController.copyToClipboard(root.activeBaseUrl + path)
            }
        }
    }

    component CodeExample: ColumnLayout {
        property string title: ""
        property string code: ""

        Layout.fillWidth: true
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Text {
                text: title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
            }

            PrimaryButton {
                text: qsTr("Copy")
                iconName: "copy"
                buttonColor: Theme.surfaceAlt
                implicitWidth: 78
                implicitHeight: 30
                quiet: true
                onClicked: AppController.copyToClipboard(code)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(118, codeText.implicitHeight + Theme.paddingMedium * 2)
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.18)
            border.color: Qt.rgba(255, 255, 255, 0.065)
            border.width: 1

            Text {
                id: codeText
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                text: code
                color: Theme.textPrimary
                font.pixelSize: 11
                font.family: "Consolas"
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    component StatusPill: Rectangle {
        property string label: ""
        property color colorValue: Theme.textSecondary

        Layout.preferredHeight: 28
        implicitWidth: pillText.implicitWidth + 24
        radius: 7
        color: Qt.rgba(colorValue.r, colorValue.g, colorValue.b, 0.12)
        border.color: Qt.rgba(colorValue.r, colorValue.g, colorValue.b, 0.36)
        border.width: 1

        Text {
            id: pillText
            anchors.centerIn: parent
            text: label
            color: colorValue
            font.pixelSize: 11
            font.bold: true
        }
    }

    component CodePill: Rectangle {
        property string text: ""

        implicitHeight: 30
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.04)
        border.color: Qt.rgba(255, 255, 255, 0.08)
        border.width: 1

        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            text: parent.text
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.family: "Consolas"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }
    }

    component ThinDivider: Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Qt.rgba(255, 255, 255, 0.07)
    }
}
