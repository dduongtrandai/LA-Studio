import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property var family: null
    property bool ready: family !== null && missingCount() === 0
    property string selectedRuntimeId: compatibleRuntimeId()
    property string selectedRuntimePath: selectedRuntimeId !== "" ? AppController.runtimes.getRuntimePath(selectedRuntimeId) : ""

    property string selectedModelFile: ""
    property string selectedTokenizerFile: ""
    property string selectedReferenceFile: ""

    signal readyToLoad(string modelPath, string tokenizerPath, string runtimePath)

    radius: Theme.radiusSmall
    color: Theme.surface
    border.color: Theme.surfaceAlt
    border.width: 1

    function familyName() {
        return family ? family.title : "Voice cloning"
    }

    function isFileInstalled(fileName) {
        if (!family || !fileName) return false
        var version = AppController.models.version
        if (AppController.models.hasFile(family.modelId, fileName)) return true
        if (family.localDir && AppController.models.hasFile(family.localDir, fileName)) return true
        if (family.localDir && AppController.files.localFileExists(AppController.settings.modelsPath + "/" + family.localDir + "/" + fileName)) return true
        for (var i = 0; i < family.requiredFiles.length; i++) {
            var req = family.requiredFiles[i]
            if (req.role !== "model" && req.role !== "tokenizer" && req.role !== "reference") continue
            if (req.modelId && AppController.models.hasFile(req.modelId, fileName)) return true
        }
        return false
    }

    function localFilePath(fileName) {
        if (!family || !fileName) return ""
        var version = AppController.models.version
        var path = AppController.models.filePath(family.modelId, fileName)
        if (path !== "") return path
        if (family.localDir) {
            path = AppController.models.filePath(family.localDir, fileName)
            if (path !== "") return path
            path = AppController.settings.modelsPath + "/" + family.localDir + "/" + fileName
            if (AppController.files.localFileExists(path)) return path
        }
        for (var i = 0; i < family.requiredFiles.length; i++) {
            var req = family.requiredFiles[i]
            if (req.role !== "model" && req.role !== "tokenizer" && req.role !== "reference") continue
            if (!req.modelId) continue
            path = AppController.models.filePath(req.modelId, fileName)
            if (path !== "") return path
        }
        return ""
    }

    function getSelectedFileForRole(role, selectedModel, selectedTokenizer, selectedReference) {
        var selModel = selectedModel !== undefined ? selectedModel : selectedModelFile
        var selTok = selectedTokenizer !== undefined ? selectedTokenizer : selectedTokenizerFile
        var selRef = selectedReference !== undefined ? selectedReference : selectedReferenceFile
        if (role === "model") return selModel !== "" ? selModel : getBestFileForRole("model")
        if (role === "tokenizer") return selTok !== "" ? selTok : getBestFileForRole("tokenizer")
        if (role === "reference") return selRef !== "" ? selRef : getBestFileForRole("reference")
        return ""
    }

    function setSelectedFileForRole(role, fileName) {
        if (role === "model") selectedModelFile = fileName
        else if (role === "tokenizer") selectedTokenizerFile = fileName
        else if (role === "reference") selectedReferenceFile = fileName
        Qt.callLater(maybeEmitReady)
    }

    function getBestFileForRole(role) {
        if (!family) return ""
        var req = null
        for (var i = 0; i < family.requiredFiles.length; i++) {
            if (family.requiredFiles[i].role === role) {
                req = family.requiredFiles[i]
                break
            }
        }
        if (!req) return ""
        var sourceModelId = req.modelId ? req.modelId : family.modelId
        if (req.candidates && req.candidates.length > 0) {
            for (var c = 0; c < req.candidates.length; c++) {
                if (AppController.models.hasFile(sourceModelId, req.candidates[c]) ||
                    AppController.models.hasFile(family.modelId, req.candidates[c]) ||
                    (family.localDir && AppController.models.hasFile(family.localDir, req.candidates[c])) ||
                    (family.localDir && AppController.files.localFileExists(AppController.settings.modelsPath + "/" + family.localDir + "/" + req.candidates[c]))) {
                    return req.candidates[c]
                }
            }
        }
        return req.file
    }

    function initializeSelectedFiles() {
        if (!family) {
            selectedModelFile = ""
            selectedTokenizerFile = ""
            selectedReferenceFile = ""
            return
        }
        selectedModelFile = getBestFileForRole("model")
        selectedTokenizerFile = getBestFileForRole("tokenizer")
        selectedReferenceFile = getBestFileForRole("reference")
    }

    function compatibleRuntimeId() {
        if (!family) return ""
        var registry = AppController.runtimes.allRuntimes
        var saved = AppController.settings.selectedTtsRuntime
        for (var s = 0; s < family.runtimes.length; s++) {
            if (family.runtimes[s].id === saved && runtimeVersionInstalled(family.runtimes[s])) {
                return saved
            }
        }
        for (var i = 0; i < family.runtimes.length; i++) {
            if (runtimeVersionInstalled(family.runtimes[i])) {
                return family.runtimes[i].id
            }
        }
        return ""
    }

    function compatibleRuntimeName() {
        if (!family) return "Compatible runtime"
        for (var i = 0; i < family.runtimes.length; i++) {
            if (family.runtimes[i].id === selectedRuntimeId) return family.runtimes[i].name
        }
        return "Compatible runtime"
    }

    function runtimeInstalled(runtimeId) {
        if (!runtimeId) return false
        var registry = AppController.runtimes.allRuntimes
        for (var i = 0; i < registry.length; i++) {
            if (registry[i].id === runtimeId) return true
        }
        return false
    }

    function runtimeVersionInstalled(runtime) {
        if (!runtime || !runtime.id) return false
        var versions = AppController.runtimes.runtimeVersions(runtime.id)
        if (!runtime.version || runtime.version === "") return versions.length > 0
        for (var i = 0; i < versions.length; i++) {
            if (versions[i].version === runtime.version) return true
        }
        return false
    }

    function selectedRuntimeVersion(runtimeId) {
        if (!family || !runtimeId) return ""
        var versions = AppController.runtimes.runtimeVersions(runtimeId)
        for (var r = 0; r < family.runtimes.length; r++) {
            if (family.runtimes[r].id !== runtimeId) continue
            var required = family.runtimes[r].version || ""
            if (required === "") break
            for (var i = 0; i < versions.length; i++) {
                if (versions[i].version === required) return required
            }
            return ""
        }
        return versions.length > 0 ? versions[0].version : ""
    }

    function activeDownload(fileName) {
        var downloads = AppController.downloads.activeDownloads
        for (var i = 0; i < downloads.length; i++) {
            if (downloads[i].filename === fileName) return downloads[i]
        }
        return null
    }

    function downloadProgressText(fileName) {
        var item = activeDownload(fileName)
        if (!item) return ""
        if (item.bytesTotal <= 0) return "Starting..."
        var pct = Math.round((item.bytesReceived / item.bytesTotal) * 100)
        var remaining = Math.max(0, item.bytesTotal - item.bytesReceived)
        return pct + "% - " + (remaining / 1048576).toFixed(1) + " MB remaining"
    }

    function virtualModelMetadata() {
        if (!family) return {}
        return {
            "kind": "modelFile",
            "familyId": family.id || "",
            "virtualModelId": family.modelId || "",
            "modelYaml": family.modelYaml || {},
            "hubFiles": family.hubFiles || {}
        }
    }

    function persistVirtualModelFiles() {
        var metadata = virtualModelMetadata()
        AppController.downloadInstall.writeVirtualModelFiles(metadata)
        return metadata
    }

    function enqueueModelFile(fileName) {
        if (!family || !fileName) return
        var metadata = persistVirtualModelFiles()
        if (isFileInstalled(fileName)) return
        var sourceModelId = family.modelId
        for (var i = 0; i < family.requiredFiles.length; i++) {
            var req = family.requiredFiles[i]
            if (req.role !== "model" && req.role !== "tokenizer" && req.role !== "reference") continue
            var files = req.candidates && req.candidates.length > 0 ? req.candidates : [req.file]
            for (var j = 0; j < files.length; j++) {
                if (files[j] === fileName) {
                    sourceModelId = req.modelId ? req.modelId : family.modelId
                    break
                }
            }
        }
        var dest = AppController.models.concreteModelDir(sourceModelId)
        AppController.downloads.enqueue(sourceModelId, fileName, dest, metadata)
    }

    function enqueueRuntime(runtime) {
        if (!runtime || runtimeVersionInstalled(runtime)) return
        persistVirtualModelFiles()
        var baseUrl = runtime.source + runtime.version + "/"
        AppController.downloads.enqueueUrl(baseUrl + runtime.asset, runtime.asset, AppController.runtimes.backendsPath, {
            "id": runtime.id,
            "version": runtime.version,
            "engineName": runtime.name,
            "engineFamily": runtime.engineFamily || "",
            "type": "tts",
            "library": runtime.library || "",
            "metadata": {
                "backend": runtime.backend || "",
                "modelFamily": root.family ? root.family.id : "",
                "modelId": root.family ? root.family.modelId : "",
                "modelVersion": runtime.modelVersion || "",
                "runtimeVersion": runtime.version,
                "asset": runtime.asset,
                "source": baseUrl + runtime.asset
            }
        })
    }

    function missingCount() {
        if (!family) return 1
        var missing = 0
        for (var i = 0; i < family.requiredFiles.length; i++) {
            var role = family.requiredFiles[i].role
            var selectedFile = getSelectedFileForRole(role)
            if (!isFileInstalled(selectedFile)) missing++
        }
        if (compatibleRuntimeId() === "") missing++
        return missing
    }

    function primaryModelPath() {
        if (!family) return ""
        return localFilePath(getSelectedFileForRole("model"))
    }

    function tokenizerPath() {
        if (!family) return ""
        return localFilePath(getSelectedFileForRole("tokenizer"))
    }

    function maybeEmitReady() {
        if (!family || missingCount() !== 0) return
        var runtimeId = compatibleRuntimeId()
        var runtimeVersion = selectedRuntimeVersion(runtimeId)
        var runtimePath = runtimeId !== "" ? AppController.runtimes.getRuntimePathForVersion(runtimeId, runtimeVersion) : ""
        if (runtimeId !== "" && AppController.settings.selectedTtsRuntime !== runtimeId) {
            AppController.settings.selectedTtsRuntime = runtimeId
        }
        if (runtimeVersion !== "" && AppController.settings.selectedTtsRuntimeVersion !== runtimeVersion) {
            AppController.settings.selectedTtsRuntimeVersion = runtimeVersion
        }
        root.readyToLoad(primaryModelPath(), tokenizerPath(), runtimePath)
    }

    onFamilyChanged: {
        initializeSelectedFiles()
        Qt.callLater(maybeEmitReady)
    }

    Connections {
        target: AppController.downloads
        function onActiveDownloadsChanged() {
            Qt.callLater(root.maybeEmitReady)
        }
    }

    Connections {
        target: AppController.models
        function onRegistryUpdated() {
            Qt.callLater(root.maybeEmitReady)
        }
    }

    Connections {
        target: AppController.runtimes
        function onRegistryUpdated() {
            Qt.callLater(root.maybeEmitReady)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingXL
        spacing: Theme.paddingLarge

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: qsTr("%1 Setup").arg(familyName())
                color: Theme.textPrimary
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }

            Text {
                text: root.ready ? qsTr("All required components are installed. Loading studio...") : qsTr("Install the required model files and one compatible runtime to unlock the studio.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontMedium
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Repeater {
            model: root.family ? root.family.requiredFiles : []

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: componentLayout.implicitHeight + Theme.paddingLarge
                radius: Theme.radiusSmall
                color: Theme.background
                border.color: Theme.surfaceAlt
                border.width: 1

                property string role: modelData.role
                property string selectedFile: root.getSelectedFileForRole(role, root.selectedModelFile, root.selectedTokenizerFile, root.selectedReferenceFile)
                property bool installed: root.isFileInstalled(selectedFile)
                property var download: root.activeDownload(selectedFile)

                ColumnLayout {
                    id: componentLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingMedium

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMedium
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: !modelData.candidates || modelData.candidates.length === 0
                                Layout.fillWidth: true
                                text: modelData.purpose + " - " + modelData.file + " - " + modelData.size
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                elide: Text.ElideMiddle
                            }

                            RowLayout {
                                visible: modelData.candidates && modelData.candidates.length > 0
                                spacing: Theme.paddingMedium

                                Text {
                                    text: modelData.purpose + ":"
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                }

                                AppComboBox {
                                    id: candidateCombo
                                    Layout.preferredWidth: 320
                                    Layout.preferredHeight: 34
                                    model: modelData.candidates ? modelData.candidates : []
                                    currentIndex: {
                                        var idx = model ? model.indexOf(selectedFile) : -1
                                        return idx !== -1 ? idx : 0
                                    }
                                    isModelSelector: true
                                    modelFamily: family
                                    modelRequirement: modelData
                                    defaultFile: modelData.file
                                    defaultSize: modelData.size
                                    onActivated: (index) => {
                                        root.setSelectedFileForRole(role, model[index])
                                    }
                                }
                            }
                        }

                        Text {
                            text: installed ? qsTr("Installed") : (download ? root.downloadProgressText(selectedFile) : qsTr("Not installed"))
                            color: installed ? Theme.success : (download ? Theme.warning : Theme.textSecondary)
                            font.pixelSize: Theme.fontSmall
                            font.bold: installed
                        }

                        PrimaryButton {
                            text: download ? qsTr("Downloading") : qsTr("Download")
                            enabled: !installed
                            implicitWidth: 112
                            implicitHeight: 34
                            quiet: !!download
                            onClicked: {
                                if (download) {
                                    Theme.requestShowDownloads()
                                } else {
                                    root.enqueueModelFile(selectedFile)
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: Theme.surfaceAlt
                        visible: download !== null

                        Rectangle {
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                            width: download && download.bytesTotal > 0 ? parent.width * (download.bytesReceived / download.bytesTotal) : 0
                            Behavior on width { NumberAnimation { duration: 150 } }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: runtimeItem
            Layout.fillWidth: true
            implicitHeight: runtimeLayout.implicitHeight + Theme.paddingLarge
            radius: Theme.radiusSmall
            color: Theme.background
            border.color: Theme.surfaceAlt
            border.width: 1

            property bool installed: root.selectedRuntimeId !== ""

            ColumnLayout {
                id: runtimeLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Compatible Inference Runtime")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontMedium
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: runtimeItem.installed ? root.compatibleRuntimeName() : qsTr("CPU, CUDA, or Vulkan build for %1").arg(root.familyName())
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        text: runtimeItem.installed ? qsTr("Installed") : qsTr("Not installed")
                        color: runtimeItem.installed ? Theme.success : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        font.bold: runtimeItem.installed
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: root.family ? root.family.runtimes : []

                        PrimaryButton {
                            property bool installed: root.runtimeVersionInstalled(modelData)
                            property bool downloading: root.activeDownload(modelData.asset) !== null

                            text: installed ? qsTr("%1 Installed").arg(modelData.label) : (downloading ? qsTr("Downloading %1").arg(modelData.label) : qsTr("Download %1").arg(modelData.label))
                            enabled: !installed
                            implicitHeight: 32
                            implicitWidth: Math.max(138, contentItem.implicitWidth + 28)
                            buttonColor: installed ? Theme.surfaceAlt : Theme.accent
                            textColor: installed ? Theme.success : "#ffffff"
                            quiet: downloading
                            onClicked: {
                                if (downloading) {
                                    Theme.requestShowDownloads()
                                } else {
                                    root.enqueueRuntime(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
