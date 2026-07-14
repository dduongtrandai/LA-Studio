import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../components/base"

Rectangle {
    id: root
    color: Theme.background

    signal openStudioRequested(string capability, string familyId)

    property string activeCategory: "all" // "all", "stt", "tts", "voice-clone"
    property string searchText: ""
    property var selectedModel: null
    property var filteredModelsList: []
    property var confirmationModelToDelete: null
    property var pinnedModelIds: []
    property bool sidebarOpen: false

    property string sortBy: "name" // "name", "params", "size", "modified"
    property string sortOrder: "asc" // "asc", "desc"

    property var sortOptions: [
        { text: qsTr("Name (A-Z)"), sortBy: "name", sortOrder: "asc" },
        { text: qsTr("Name (Z-A)"), sortBy: "name", sortOrder: "desc" },
        { text: qsTr("Params (Large-Small)"), sortBy: "params", sortOrder: "desc" },
        { text: qsTr("Params (Small-Large)"), sortBy: "params", sortOrder: "asc" },
        { text: qsTr("Size (Large-Small)"), sortBy: "size", sortOrder: "desc" },
        { text: qsTr("Size (Small-Large)"), sortBy: "size", sortOrder: "asc" },
        { text: qsTr("Modified (Newest)"), sortBy: "modified", sortOrder: "desc" },
        { text: qsTr("Modified (Oldest)"), sortBy: "modified", sortOrder: "asc" }
    ]

    function loadPinnedModels() {
        var path = AppController.dataDir + "/pinned_models.json";
        if (AppController.files.localFileExists(path)) {
            try {
                var content = AppController.files.readTextFile(path);
                if (content) {
                    var parsed = JSON.parse(content);
                    if (Array.isArray(parsed)) {
                        root.pinnedModelIds = parsed;
                        return;
                    }
                }
            } catch (e) {
                console.log("Error loading pinned models:", e);
            }
        }
        root.pinnedModelIds = [];
    }

    function savePinnedModels() {
        var path = AppController.dataDir + "/pinned_models.json";
        try {
            var content = JSON.stringify(root.pinnedModelIds);
            AppController.files.writeTextFile(path, content);
        } catch (e) {
            console.log("Error saving pinned models:", e);
        }
    }

    function togglePinModel(modelId) {
        var list = [];
        for (var i = 0; i < root.pinnedModelIds.length; i++) {
            list.push(root.pinnedModelIds[i]);
        }
        var index = list.indexOf(modelId);
        if (index === -1) {
            list.push(modelId);
        } else {
            list.splice(index, 1);
        }
        root.pinnedModelIds = list;
        savePinnedModels();
        refreshModels();
    }

    function isPinned(modelId) {
        return root.pinnedModelIds.indexOf(modelId) !== -1;
    }

    // Parse model size string (e.g. "350 MB" or "1.2 GB") to MB for calculations
    function parseSizeToMB(sizeStr) {
        if (!sizeStr) return 0
        var str = sizeStr.trim().toLowerCase()
        var val = parseFloat(str)
        if (isNaN(val)) return 0
        if (str.indexOf("gb") !== -1) return val * 1024
        if (str.indexOf("kb") !== -1) return val / 1024
        return val // Default to MB
    }

    // Extract quantization level from filename
    function extractQuant(filename) {
        if (!filename) return ""
        var match = filename.match(/(?:I)?Q[0-9]_[A-Z0-9_]+/i)
        if (match) return match[0].toUpperCase()
        var matchFp = filename.match(/(?:fp|bf|f)(?:16|32)/i)
        if (matchFp) return matchFp[0].toUpperCase()
        return ""
    }

    // Parse model parameters string (e.g. "82M" or "1.7B") to numeric millions for sorting
    function parseParamsToNumber(paramsStr) {
        if (!paramsStr) return 0
        var str = paramsStr.trim().toLowerCase()
        var val = parseFloat(str)
        if (isNaN(val)) return 0
        if (str.indexOf("b") !== -1) return val * 1000
        if (str.indexOf("m") !== -1) return val
        if (str.indexOf("k") !== -1) return val / 1000
        return val
    }

    // Parse relative modification string (e.g. "Just now" or "X minutes ago") to elapsed seconds
    function parseModifiedToSeconds(modStr) {
        if (!modStr) return 999999999
        var str = modStr.trim().toLowerCase()
        if (str === "just now") return 0
        var val = parseFloat(str)
        if (isNaN(val)) return 999999999
        if (str.indexOf("minute") !== -1) return val * 60
        if (str.indexOf("hour") !== -1) return val * 3600
        if (str.indexOf("day") !== -1) return val * 86400
        return 999999999
    }

    function updateSortComboIndex() {
        if (typeof sortCombo !== "undefined" && sortCombo) {
            for (var i = 0; i < root.sortOptions.length; i++) {
                if (root.sortOptions[i].sortBy === root.sortBy && root.sortOptions[i].sortOrder === root.sortOrder) {
                    sortCombo.currentIndex = i;
                    break;
                }
            }
        }
    }

    function selectSort(field) {
        if (root.sortBy === field) {
            root.sortOrder = (root.sortOrder === "asc" ? "desc" : "asc")
        } else {
            root.sortBy = field
            if (field === "modified" || field === "size" || field === "params") {
                root.sortOrder = "desc"
            } else {
                root.sortOrder = "asc"
            }
        }
        updateSortComboIndex()
        refreshModels()
    }

    // Refresh and filter the list of models
    function refreshModels() {
        var count = AppController.models.count // Establish dependency
        var version = AppController.models.version // Establish dependency

        var stt = AppController.models.modelsForTask("stt")
        var tts = AppController.models.modelsForTask("tts")
        var vc = AppController.models.modelsForTask("voice-clone")
        var all = stt.concat(tts).concat(vc)

        var filtered = []
        for (var i = 0; i < all.length; i++) {
            var m = all[i]

            // Filter by active category
            if (root.activeCategory !== "all" && m.task !== root.activeCategory) continue

            // Filter by search text
            if (root.searchText !== "") {
                var searchLower = root.searchText.toLowerCase()
                var nameLower = m.id.toLowerCase()
                if (nameLower.indexOf(searchLower) === -1) continue
            }

            filtered.push(m)
        }

        filtered.sort(function(a, b) {
            var pinA = root.isPinned(a.id);
            var pinB = root.isPinned(b.id);
            if (pinA && !pinB) return -1;
            if (!pinA && pinB) return 1;

            var res = 0;
            if (root.sortBy === "params") {
                var valA = root.parseParamsToNumber(a.params);
                var valB = root.parseParamsToNumber(b.params);
                res = valA - valB;
                if (root.sortOrder === "desc") res = -res;
            } else if (root.sortBy === "size") {
                var valA = root.parseSizeToMB(a.size);
                var valB = root.parseSizeToMB(b.size);
                res = valA - valB;
                if (root.sortOrder === "desc") res = -res;
            } else if (root.sortBy === "modified") {
                var valA = root.parseModifiedToSeconds(a.modified);
                var valB = root.parseModifiedToSeconds(b.modified);
                if (root.sortOrder === "desc") {
                    res = valA - valB; // Newer first
                } else {
                    res = valB - valA; // Oldest first
                }
            } else {
                res = a.id.localeCompare(b.id);
                if (root.sortOrder === "desc") res = -res;
            }

            if (res === 0) {
                res = a.id.localeCompare(b.id);
            }
            return res;
        });

        root.filteredModelsList = filtered

        // Update selected model if it was removed or if none selected
        if (filtered.length === 0) {
            root.selectedModel = null
        } else {
            if (root.selectedModel) {
                var found = false
                for (var j = 0; j < filtered.length; j++) {
                    if (filtered[j].id === root.selectedModel.id) {
                        root.selectedModel = filtered[j]
                        found = true
                        break
                    }
                }
                if (!found) root.selectedModel = filtered[0]
            } else {
                root.selectedModel = filtered[0]
            }
        }
    }

    // Disk space summary calculations
    property var diskSpaceInfo: {
        var count = AppController.models.count // Establish dependency
        var version = AppController.models.version // Establish dependency

        var stt = AppController.models.modelsForTask("stt")
        var tts = AppController.models.modelsForTask("tts")
        var vc = AppController.models.modelsForTask("voice-clone")
        var all = stt.concat(tts).concat(vc)

        var totalMB = 0
        for (var i = 0; i < all.length; i++) {
            totalMB += parseSizeToMB(all[i].size)
        }

        var sizeText = ""
        if (totalMB >= 1024) {
            sizeText = (totalMB / 1024).toFixed(2) + " GB"
        } else {
            sizeText = totalMB.toFixed(1) + " MB"
        }

        return { count: all.length, sizeText: sizeText }
    }

    // Find a matching family in the registry for a model ID
    function findMatchingFamily(modelId, task) {
        var registry = AppController.registry
        var families = []
        if (task === "stt") {
            families = registry.sttFamilies
        } else if (task === "tts") {
            families = registry.ttsFamilies
        } else {
            families = registry.sttFamilies.concat(registry.ttsFamilies)
        }

        // Exact match
        for (var i = 0; i < families.length; i++) {
            var f = families[i]
            if (f.id === modelId || f.modelId === modelId || f.localDir === modelId) {
                return f
            }
        }

        // Case-insensitive, fuzzy substring match
        var lowerId = modelId.toLowerCase()
        for (var i = 0; i < families.length; i++) {
            var f = families[i]
            var familyId = f.id ? f.id.toLowerCase() : ""
            var modelIdStr = f.modelId ? f.modelId.toLowerCase() : ""
            var localDirStr = f.localDir ? f.localDir.toLowerCase() : ""
            if (familyId === lowerId || modelIdStr === lowerId || localDirStr === lowerId ||
                lowerId.indexOf(familyId) !== -1 || lowerId.indexOf(modelIdStr) !== -1 || lowerId.indexOf(localDirStr) !== -1 ||
                familyId.indexOf(lowerId) !== -1 || modelIdStr.indexOf(lowerId) !== -1 || localDirStr.indexOf(lowerId) !== -1) {
                return f
            }
        }
        return null
    }

    property var selectedModelFamily: selectedModel ? findMatchingFamily(selectedModel.id, selectedModel.task) : null

    onActiveCategoryChanged: refreshModels()
    onSearchTextChanged: refreshModels()

    Component.onCompleted: {
        loadPinnedModels()
        AppController.models.scanLocalModels()
        refreshModels()
    }

    Connections {
        target: AppController.models
        function onVersionChanged() {
            root.refreshModels()
        }
        function onCountChanged() {
            root.refreshModels()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Left Sidebar Navigation
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 220
            color: Qt.darker(Theme.background, 1.01)

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.surfaceAlt
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                // Panel Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.paddingLarge
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("My Models")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontLarge
                        font.bold: true
                    }
                }

                // Category Buttons List
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingMedium
                    Layout.rightMargin: Theme.paddingMedium
                    spacing: 4

                    CategoryButton {
                        text: qsTr("View All")
                        iconName: "gallery"
                        active: root.activeCategory === "all"
                        onClicked: root.activeCategory = "all"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.05)
                        Layout.topMargin: Theme.paddingSmall
                        Layout.bottomMargin: Theme.paddingSmall
                    }

                    CategoryButton {
                        text: qsTr("Speech to Text")
                        iconName: "mic"
                        active: root.activeCategory === "stt"
                        onClicked: root.activeCategory = "stt"
                    }

                    CategoryButton {
                        text: qsTr("Text to Speech")
                        iconName: "volume"
                        active: root.activeCategory === "tts"
                        onClicked: root.activeCategory = "tts"
                    }

                    CategoryButton {
                        text: qsTr("Voice Cloning")
                        iconName: "spark"
                        active: root.activeCategory === "voice-clone"
                        onClicked: root.activeCategory = "voice-clone"
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // 2. Central Model Table/List Pane
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Central Toolbar (Search / Header)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Theme.background

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.surfaceAlt
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.rightMargin: Theme.paddingLarge

                    Text {
                        text: {
                            if (root.activeCategory === "stt") return qsTr("Speech to Text Models")
                            if (root.activeCategory === "tts") return qsTr("Text to Speech Models")
                            if (root.activeCategory === "voice-clone") return qsTr("Voice Cloning Models")
                            return qsTr("All Local Models")
                        }
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    // Sort selector
                    AppComboBox {
                        id: sortCombo
                        model: root.sortOptions
                        textRole: "text"
                        currentIndex: 0
                        implicitWidth: 170
                        implicitHeight: 34
                        Layout.preferredHeight: 34
                        onActivated: (index) => {
                            root.sortBy = root.sortOptions[index].sortBy
                            root.sortOrder = root.sortOptions[index].sortOrder
                            root.refreshModels()
                        }
                    }

                    // Search input
                    Rectangle {
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 34
                        radius: Theme.radiusSmall
                        color: Theme.surface
                        border.color: searchField.activeFocus ? Theme.accent : Theme.surfaceAlt
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingSmall
                            anchors.rightMargin: Theme.paddingSmall
                            spacing: Theme.paddingSmall

                            LineIcon {
                                name: "search"
                                color: searchField.activeFocus ? Theme.accent : Theme.textSecondary
                                Layout.preferredWidth: 14
                                Layout.preferredHeight: 14
                            }

                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                placeholderText: qsTr("Filter models...")
                                color: Theme.textPrimary
                                placeholderTextColor: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                padding: 0
                                background: Item {}
                                onTextChanged: root.searchText = text
                            }
                        }
                    }
                }
            }

            // Table Header Row
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                color: Qt.darker(Theme.background, 1.02)

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.surfaceAlt
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingMedium

                    TableHeaderText { text: qsTr("Arch"); Layout.preferredWidth: 80 }
                    TableHeaderCell { text: qsTr("Params"); sortField: "params"; Layout.preferredWidth: 70 }
                    TableHeaderText { text: qsTr("Publisher"); Layout.preferredWidth: 130 }
                    TableHeaderCell { text: qsTr("Name / ID"); sortField: "name"; Layout.fillWidth: true }
                    TableHeaderText { text: qsTr("Quant"); Layout.preferredWidth: 80 }
                    TableHeaderCell { text: qsTr("Size"); sortField: "size"; Layout.preferredWidth: 80 }
                    TableHeaderCell { text: qsTr("Modified"); sortField: "modified"; Layout.preferredWidth: 100 }
                    TableHeaderText { text: qsTr("Actions"); Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
                }
            }

            // List of Models
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: modelsListView
                    anchors.fill: parent
                    model: root.filteredModelsList
                    clip: true
                    spacing: 0

                    delegate: Rectangle {
                        width: modelsListView.width
                        height: 52
                        color: {
                            if (root.selectedModel && root.selectedModel.id === modelData.id)
                                return Qt.rgba(1, 1, 1, 0.035)
                            return rowHover.hovered ? Qt.rgba(1, 1, 1, 0.02) : "transparent"
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: Theme.surfaceAlt
                            opacity: 0.5
                        }

                        HoverHandler { id: rowHover }

                        TapHandler {
                            onTapped: root.selectedModel = modelData
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingLarge
                            anchors.rightMargin: Theme.paddingLarge
                            spacing: Theme.paddingMedium

                            // 1. Arch
                            Item {
                                Layout.preferredWidth: 80
                                Layout.fillHeight: true

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.max(48, archText.implicitWidth + 12)
                                    height: 20
                                    radius: 4
                                    color: {
                                        var arch = modelData.arch ? modelData.arch.toLowerCase() : "";
                                        if (arch.indexOf("gemma") !== -1) return Qt.rgba(0.2, 0.4, 0.9, 0.15)
                                        if (arch.indexOf("qwen") !== -1) return Qt.rgba(0.9, 0.2, 0.2, 0.15)
                                        if (arch.indexOf("llama") !== -1) return Qt.rgba(0.1, 0.7, 0.1, 0.15)
                                        if (arch.indexOf("styletts") !== -1 || arch.indexOf("kokoro") !== -1) return Qt.rgba(0.5, 0.2, 0.8, 0.15)
                                        return Qt.rgba(1, 1, 1, 0.05)
                                    }
                                    border.color: {
                                        var arch = modelData.arch ? modelData.arch.toLowerCase() : "";
                                        if (arch.indexOf("gemma") !== -1) return Qt.rgba(0.2, 0.4, 0.9, 0.4)
                                        if (arch.indexOf("qwen") !== -1) return Qt.rgba(0.9, 0.2, 0.2, 0.4)
                                        if (arch.indexOf("llama") !== -1) return Qt.rgba(0.1, 0.7, 0.1, 0.4)
                                        if (arch.indexOf("styletts") !== -1 || arch.indexOf("kokoro") !== -1) return Qt.rgba(0.5, 0.2, 0.8, 0.4)
                                        return Qt.rgba(1, 1, 1, 0.15)
                                    }
                                    border.width: 1

                                    Text {
                                        id: archText
                                        anchors.centerIn: parent
                                        text: modelData.arch || "unknown"
                                        color: {
                                            var arch = modelData.arch ? modelData.arch.toLowerCase() : "";
                                            if (arch.indexOf("gemma") !== -1) return "#64b5f6"
                                            if (arch.indexOf("qwen") !== -1) return "#ef9a9a"
                                            if (arch.indexOf("llama") !== -1) return "#81c784"
                                            if (arch.indexOf("styletts") !== -1 || arch.indexOf("kokoro") !== -1) return "#b39ddb"
                                            return Theme.textSecondary
                                        }
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }

                            // 2. Params
                            Item {
                                Layout.preferredWidth: 70
                                Layout.fillHeight: true

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.max(38, paramsText.implicitWidth + 12)
                                    height: 20
                                    radius: 4
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                    border.color: Qt.rgba(1, 1, 1, 0.15)
                                    border.width: 1

                                    Text {
                                        id: paramsText
                                        anchors.centerIn: parent
                                        text: modelData.params || "unknown"
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }

                            // 3. Publisher
                            Text {
                                text: modelData.publisher || "local"
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                Layout.preferredWidth: 130
                                elide: Text.ElideRight
                            }

                            // 4. Name / ID
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall

                                LineIcon {
                                    name: {
                                        if (modelData.task === "stt") return "mic"
                                        if (modelData.task === "tts") return "volume"
                                        return "spark"
                                    }
                                    color: {
                                        var family = root.findMatchingFamily(modelData.id, modelData.task)
                                        return family && family.accent ? family.accent : Theme.accentLight
                                    }
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    strokeWidth: 1.8
                                }

                                LineIcon {
                                    visible: root.isPinned(modelData.id)
                                    name: "star"
                                    color: Theme.warning
                                    Layout.preferredWidth: 12
                                    Layout.preferredHeight: 12
                                    strokeWidth: 1.8
                                }

                                Text {
                                    text: modelData.id
                                    color: Theme.textPrimary
                                    font.bold: root.selectedModel && root.selectedModel.id === modelData.id
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // 5. Quant
                            Item {
                                Layout.preferredWidth: 80
                                Layout.fillHeight: true

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.max(48, quantText.implicitWidth + 12)
                                    height: 20
                                    radius: 4
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                    border.color: Qt.rgba(1, 1, 1, 0.15)
                                    border.width: 1

                                    Text {
                                        id: quantText
                                        anchors.centerIn: parent
                                        text: modelData.quant || "Unknown"
                                        color: Theme.textPrimary
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }

                            // 6. Size
                            Text {
                                text: modelData.size || "Unknown"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                Layout.preferredWidth: 80
                            }

                            // 7. Modified
                            Text {
                                text: modelData.modified || ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                Layout.preferredWidth: 100
                                elide: Text.ElideRight
                            }

                            // 8. Actions
                            Item {
                                Layout.preferredWidth: 110
                                Layout.fillHeight: true
                                Layout.alignment: Qt.AlignRight

                                Row {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 6

                                    ActionIconButton {
                                        id: moreActionsBtn
                                        iconName: "more-horizontal"
                                        toolTipText: qsTr("More actions")
                                        onClicked: {
                                            actionMenu.modelData = modelData
                                            actionMenu.popup(moreActionsBtn, 0, moreActionsBtn.height)
                                        }
                                    }

                                    ActionIconButton {
                                        iconName: "settings"
                                        toolTipText: qsTr("Edit model default config")
                                        onClicked: {
                                            root.selectedModel = modelData
                                            root.sidebarOpen = true
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Empty State
                    Text {
                        anchors.centerIn: parent
                        visible: root.filteredModelsList.length === 0
                        text: qsTr("No downloaded models found matching the filters.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontMedium
                    }
                }
            }

            // Central Footer (Space Summary)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                color: Qt.darker(Theme.background, 1.01)

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.surfaceAlt
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.rightMargin: Theme.paddingLarge

                    Text {
                        text: qsTr("You have %1 local models, taking up %2 of disk space.").arg(root.diskSpaceInfo.count).arg(root.diskSpaceInfo.sizeText)
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: qsTr("Storage: %1").arg(AppController.models.modelsRoot)
                        color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.7)
                        font.pixelSize: 11
                        font.family: "Consolas"
                        elide: Text.ElideLeft
                        Layout.maximumWidth: 320
                        
                        HoverHandler { id: storageHover }
                        AppToolTip {
                            text: AppController.models.modelsRoot
                            visible: storageHover.hovered
                        }
                    }
                }
            }
        }

        // 3. Right Detail Sidebar Pane
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 320
            color: Theme.surface
            visible: root.selectedModel !== null && root.sidebarOpen

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.surfaceAlt
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingLarge
                spacing: Theme.paddingLarge

                // Selected Model Title & CTA Button
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        Text {
                            text: root.selectedModel ? root.selectedModel.id : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLarge
                            font.bold: true
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Button {
                            implicitWidth: 26
                            implicitHeight: 26
                            onClicked: root.sidebarOpen = false
                            contentItem: LineIcon {
                                name: "close"
                                color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                                anchors.centerIn: parent
                                width: 12
                                height: 12
                            }
                            background: Rectangle {
                                radius: 4
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                    }

                    // Call to Action Button
                    PrimaryButton {
                        Layout.fillWidth: true
                        implicitHeight: 38
                        text: {
                            if (!root.selectedModel) return qsTr("Use Model")
                            if (root.selectedModel.task === "stt") return qsTr("Use in Speech to Text")
                            if (root.selectedModel.task === "tts") return qsTr("Use in Text to Speech")
                            if (root.selectedModel.task === "voice-clone") return qsTr("Use in Voice Cloning")
                            return qsTr("Use Model")
                        }
                        iconName: "play"
                        buttonColor: root.selectedModelFamily ? (root.selectedModelFamily.accent || Theme.accent) : Theme.accent
                        enabled: root.selectedModelFamily !== null
                        
                        onClicked: {
                            if (root.selectedModelFamily) {
                                root.openStudioRequested(root.selectedModel.task, root.selectedModelFamily.id)
                            }
                        }
                    }

                    Text {
                        visible: root.selectedModelFamily === null
                        text: qsTr("⚠️ Model not registered in app catalog; launch from within studio templates instead.")
                        color: Theme.warning
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                // Scrollable details section
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.paddingLarge

                        // 1. Model Info Table
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall

                            SectionTitle { text: qsTr("Model Information") }

                            DetailInfoRow {
                                label: qsTr("Model ID:")
                                value: root.selectedModel ? root.selectedModel.id : ""
                            }
                            DetailInfoRow {
                                label: qsTr("Format:")
                                value: root.selectedModel && root.selectedModel.format ? root.selectedModel.format.toUpperCase() : "BIN"
                            }
                            DetailInfoRow {
                                label: qsTr("Size:")
                                value: root.selectedModel ? root.selectedModel.size : ""
                            }
                            DetailInfoRow {
                                label: qsTr("Directory:")
                                value: root.selectedModel ? root.selectedModel.path : ""
                                isPath: true
                            }
                        }

                        // 2. Files inside model folder
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            SectionTitle { text: qsTr("Files in Folder") }

                            Repeater {
                                model: root.selectedModel ? root.selectedModel.files : []
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    height: 30
                                    radius: Theme.radiusSmall / 2
                                    color: Qt.rgba(1, 1, 1, 0.02)
                                    border.color: Qt.rgba(1, 1, 1, 0.05)
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.paddingSmall
                                        anchors.rightMargin: Theme.paddingSmall

                                        LineIcon {
                                            name: "file"
                                            color: Theme.textSecondary
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                        }

                                        Text {
                                            text: modelData
                                            color: Theme.textPrimary
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }

                        // 3. Catalog Description (if found)
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall
                            visible: root.selectedModelFamily !== null && root.selectedModelFamily.description !== ""

                            SectionTitle { text: qsTr("Catalog Description") }

                            Text {
                                text: root.selectedModelFamily ? root.selectedModelFamily.description : ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                lineHeight: 1.2
                            }
                        }
                    }
                }
            }
        }
    }

    // Modal Confirmation Dialog for Model deletion
    ConfirmationDialog {
        id: deleteConfirmDialog
        titleText: qsTr("Delete Local Model")
        messageText: root.confirmationModelToDelete 
                     ? qsTr("Are you sure you want to permanently delete the model files for '%1' from your hard drive?\n\nPath: %2").arg(root.confirmationModelToDelete.id).arg(root.confirmationModelToDelete.path)
                     : ""
        confirmText: qsTr("Delete")
        isDestructive: true
        
        onConfirmed: {
            if (root.confirmationModelToDelete) {
                AppController.models.removeModel(root.confirmationModelToDelete.id)
                root.confirmationModelToDelete = null
                AppController.models.scanLocalModels()
                root.refreshModels()
            }
        }
        onCancelled: {
            root.confirmationModelToDelete = null
        }
    }

    // Modal Dependencies Dialog for LA Studio Picks
    ModelDependenciesDialog {
        id: dependenciesDialog
    }

    Menu {
        id: actionMenu
        property var modelData: null

        background: Rectangle {
            implicitWidth: 220
            color: Theme.surface
            radius: Theme.radiusSmall
            border.color: Theme.surfaceAlt
            border.width: 1

            // Simple shadow fallback
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                color: "#00000018"
                radius: Theme.radiusSmall + 2
                z: -1
            }
        }

        topPadding: 6
        bottomPadding: 6

        ContextMenuItem {
            iconName: "folder"
            text: qsTr("Open in File Explorer")
            onTriggered: {
                if (!actionMenu.modelData) return
                var m = actionMenu.modelData
                var family = root.findMatchingFamily(m.id, m.task)
                if (family && family.isLastudioPick) {
                    dependenciesDialog.modelId = m.id
                    dependenciesDialog.virtualPath = AppController.models.virtualModelDir(m.id)
                    dependenciesDialog.concretePath = m.path
                    dependenciesDialog.modelName = family.title || m.id
                    dependenciesDialog.format = m.format ? m.format.toUpperCase() : ""
                    
                    var quantVal = ""
                    if (m.files && m.files.length > 0) {
                        quantVal = root.extractQuant(m.files[0])
                    }
                    dependenciesDialog.quant = quantVal
                    dependenciesDialog.open()
                } else {
                    Qt.openUrlExternally("file:///" + m.path)
                }
            }
        }

        ContextMenuItem {
            iconName: "star"
            text: actionMenu.modelData && root.isPinned(actionMenu.modelData.id) ? qsTr("Unpin from Top") : qsTr("Pin to Top")
            iconColor: actionMenu.modelData && root.isPinned(actionMenu.modelData.id) ? Theme.warning : Theme.textSecondary
            onTriggered: {
                if (actionMenu.modelData) {
                    root.togglePinModel(actionMenu.modelData.id)
                }
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.surfaceAlt
                opacity: 0.5
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.paddingSmall
            }
        }

        ContextMenuItem {
            iconName: "copy"
            text: qsTr("Copy Default Identifier")
            onTriggered: {
                if (actionMenu.modelData) {
                    AppController.copyToClipboard(actionMenu.modelData.id)
                }
            }
        }

        ContextMenuItem {
            iconName: "copy"
            text: qsTr("Copy Absolute Path")
            onTriggered: {
                if (actionMenu.modelData) {
                    AppController.copyToClipboard(actionMenu.modelData.path)
                }
            }
        }

        ContextMenuItem {
            iconName: "external-link"
            text: qsTr("Show on Web")
            onTriggered: {
                if (actionMenu.modelData) {
                    Qt.openUrlExternally("https://huggingface.co/" + actionMenu.modelData.id)
                }
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.surfaceAlt
                opacity: 0.5
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.paddingSmall
            }
        }

        ContextMenuItem {
            iconName: "trash"
            text: qsTr("Delete")
            textColor: Qt.rgba(239, 83, 80, 0.8)
            iconColor: Qt.rgba(239, 83, 80, 0.8)
            hoverColor: Qt.rgba(239, 83, 80, 0.1)
            onTriggered: {
                if (actionMenu.modelData) {
                    root.confirmationModelToDelete = actionMenu.modelData
                    deleteConfirmDialog.open()
                }
            }
        }
    }

    component ContextMenuItem: MenuItem {
        id: cItem
        property string iconName: ""
        property color iconColor: Theme.textSecondary
        property color textColor: Theme.textPrimary
        property color hoverColor: Qt.rgba(255, 255, 255, 0.06)

        implicitWidth: 220
        implicitHeight: 34

        background: Rectangle {
            color: cItem.hovered || cItem.highlighted || cItem.pressed ? cItem.hoverColor : "transparent"
            radius: 4
            anchors.fill: parent
            anchors.margins: 2
        }

        contentItem: RowLayout {
            spacing: Theme.paddingMedium
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium

            LineIcon {
                name: cItem.iconName
                color: cItem.hovered ? (cItem.iconName === "trash" ? Theme.danger : Theme.textPrimary) : cItem.iconColor
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                visible: cItem.iconName !== ""
            }

            Text {
                text: cItem.text
                color: cItem.hovered ? (cItem.iconName === "trash" ? Theme.danger : Theme.textPrimary) : cItem.textColor
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
            }
        }
    }

    // Custom reusable components inside MyModelsPage
    component CategoryButton: Rectangle {
        id: catBtn
        property string text: ""
        property string iconName: ""
        property bool active: false
        signal clicked()

        Layout.fillWidth: true
        height: 38
        radius: Theme.radiusSmall
        color: active ? Qt.rgba(0.49, 0.30, 1.0, 0.14) : (btnHover.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent")
        border.color: active ? Qt.rgba(0.49, 0.30, 1.0, 0.3) : "transparent"
        border.width: 1

        HoverHandler { id: btnHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: catBtn.clicked() }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            spacing: Theme.paddingSmall

            LineIcon {
                name: catBtn.iconName
                color: catBtn.active ? Theme.accentLight : (btnHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                strokeWidth: 1.8
            }

            Text {
                text: catBtn.text
                color: catBtn.active ? Theme.textPrimary : (btnHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                font.pixelSize: Theme.fontSmall
                font.bold: catBtn.active
                Layout.fillWidth: true
            }
        }
    }

    component TableHeaderCell: Item {
        id: cell
        property string text: ""
        property string sortField: "" // If empty, not sortable
        property int horizontalAlignment: Text.AlignLeft

        implicitHeight: 38

        RowLayout {
            anchors.fill: parent
            spacing: 4

            Text {
                text: cell.text
                color: (cell.sortField !== "" && root.sortBy === cell.sortField) ? Theme.textPrimary : (headerHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                horizontalAlignment: cell.horizontalAlignment
            }

            LineIcon {
                visible: cell.sortField !== "" && root.sortBy === cell.sortField
                name: root.sortOrder === "asc" ? "arrow-up" : "arrow-down"
                color: Theme.accent
                Layout.preferredWidth: 10
                Layout.preferredHeight: 10
                strokeWidth: 2
                Layout.alignment: Qt.AlignVCenter
            }
        }

        HoverHandler {
            id: headerHover
            enabled: cell.sortField !== ""
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            enabled: cell.sortField !== ""
            onTapped: root.selectSort(cell.sortField)
        }
    }

    component TableHeaderText: Text {
        color: Theme.textSecondary
        font.pixelSize: 11
        font.bold: true
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    component ActionIconButton: Button {
        id: actBtn
        property string iconName: ""
        property string toolTipText: ""
        property color hoverColor: Theme.accentLight

        implicitWidth: 28
        implicitHeight: 28
        
        contentItem: LineIcon {
            name: actBtn.iconName
            color: actBtn.hovered ? actBtn.hoverColor : Theme.textSecondary
            anchors.centerIn: parent
            width: 14
            height: 14
            strokeWidth: 1.6
        }

        background: Rectangle {
            radius: 6
            color: actBtn.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
            border.color: actBtn.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
            border.width: 1
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        
        AppToolTip {
            text: actBtn.toolTipText
            visible: actBtn.hovered
        }
    }

    component SectionTitle: Text {
        color: Theme.accentLight
        font.pixelSize: Theme.fontSmall
        font.bold: true
        Layout.fillWidth: true
        Layout.topMargin: 5
    }

    component DetailInfoRow: ColumnLayout {
        id: detailRow
        property string label: ""
        property string value: ""
        property bool isPath: false

        Layout.fillWidth: true
        spacing: 2

        Text {
            text: detailRow.label
            color: Theme.textSecondary
            font.pixelSize: 11
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: detailRow.value
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.family: detailRow.isPath ? "Consolas" : "Segoe UI"
                wrapMode: detailRow.isPath ? Text.WrapAnywhere : Text.NoWrap
                Layout.fillWidth: true
                elide: detailRow.isPath ? Text.ElideNone : Text.ElideRight
            }

            Button {
                visible: detailRow.isPath && detailRow.value !== ""
                implicitWidth: 22
                implicitHeight: 22
                onClicked: AppController.copyToClipboard(detailRow.value)
                contentItem: LineIcon {
                    name: "copy"
                    color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                }
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                AppToolTip {
                    text: qsTr("Copy path")
                    visible: parent.hovered
                }
            }

            Button {
                visible: detailRow.isPath && detailRow.value !== ""
                implicitWidth: 22
                implicitHeight: 22
                onClicked: {
                    var modelData = root.selectedModel
                    if (modelData) {
                        var family = root.findMatchingFamily(modelData.id, modelData.task)
                        if (family && family.isLastudioPick) {
                            dependenciesDialog.modelId = modelData.id
                            dependenciesDialog.virtualPath = AppController.models.virtualModelDir(modelData.id)
                            dependenciesDialog.concretePath = modelData.path
                            dependenciesDialog.modelName = family.title || modelData.id
                            dependenciesDialog.format = modelData.format ? modelData.format.toUpperCase() : ""
                            
                            var quantVal = ""
                            if (modelData.files && modelData.files.length > 0) {
                                quantVal = root.extractQuant(modelData.files[0])
                            }
                            dependenciesDialog.quant = quantVal
                            dependenciesDialog.open()
                        } else {
                            Qt.openUrlExternally("file:///" + modelData.path)
                        }
                    }
                }
                contentItem: LineIcon {
                    name: "folder"
                    color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                }
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                AppToolTip {
                    text: qsTr("Open folder")
                    visible: parent.hovered
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
            opacity: 0.3
            Layout.topMargin: 4
        }
    }
}
