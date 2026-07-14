import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../../components/base"

Rectangle {
    id: root

    property string displayMode: "modal" // "modal", "page"
    property bool managementMode: false
    property string capability: "tts" // "tts", "voice-cloning", "stt", "all"
    property string selectedFamilyId: ""
    property string searchText: ""
    property string filterType: "all" // "all", "tts", "stt", "cloning", "installed", "missing"
    property bool modalMode: displayMode === "modal"
    property string pendingRuntimeId: ""
    property string pendingRuntimeVersion: ""
    property var initialSelectedFiles: ({})

    property var familiesModel: null

    Loader {
        id: localControllerLoader
        active: !root.familiesModel
        sourceComponent: StudioPageController {
            capabilityId: root.capability
        }
    }

    property var activeModel: root.familiesModel ? root.familiesModel : (localControllerLoader.item ? localControllerLoader.item.familiesModel : null)

    signal familySelected(string familyId)
    signal openStudio(string capability, string familyId)
    signal configurationAccepted(string familyId, string runtimeId, string runtimeVersion, var selectedFiles)

    color: Theme.background

    onCapabilityChanged: updateActiveModelCapability()
    onFilterTypeChanged: updateActiveModelCapability()
    onSearchTextChanged: {
        if (activeModel && activeModel.setSearchText) activeModel.setSearchText(searchText)
        ensureSelection()
    }
    onSelectedFamilyIdChanged: {
        if (activeModel) {
            activeModel.setSelectedFamilyId(selectedFamilyId)
            if (initialSelectedFiles && Object.keys(initialSelectedFiles).length > 0) {
                activeModel.setInitialSelectedFiles(selectedFamilyId, initialSelectedFiles)
            }
        }
        syncPendingRuntime(true)
    }

    onInitialSelectedFilesChanged: {
        if (activeModel && selectedFamilyId !== "") {
            activeModel.setInitialSelectedFiles(selectedFamilyId, initialSelectedFiles)
        }
    }

    Component.onCompleted: {
        updateActiveModelCapability()
        ensureSelection()
        if (activeModel) {
            activeModel.setSelectedFamilyId(selectedFamilyId)
        }
    }

    function updateActiveModelCapability() {
        if (!activeModel) return
        if (activeModel.setStatusFilter) activeModel.setStatusFilter(filterType)
        if (activeModel.setSearchText) activeModel.setSearchText(searchText)
        if (capability === "all") {
            if (filterType === "all") activeModel.setCapability("all")
            else if (filterType === "tts") activeModel.setCapability("tts")
            else if (filterType === "stt") activeModel.setCapability("stt")
            else if (filterType === "cloning") activeModel.setCapability("voice-cloning")
            else activeModel.setCapability("all")
        } else {
            activeModel.setCapability(capability)
        }
        ensureSelection()
    }

    function ensureSelection() {
        if (!activeModel || !activeModel.itemForFamily || !activeModel.firstFamilyId) return
        var current = selectedFamilyId !== "" ? activeModel.itemForFamily(selectedFamilyId) : null
        if (current && current.familyId) return
        var firstId = activeModel.firstFamilyId()
        if (firstId !== "") selectedFamilyId = firstId
    }

    function selectedFamily() {
        var item = selectedFamilyItem()
        return item ? item.rawMetadata : null
    }

    function selectedFamilyItem() {
        if (!activeModel || !activeModel.itemForFamily || !activeModel.firstFamilyId) return null
        var item = selectedFamilyId !== "" ? activeModel.itemForFamily(selectedFamilyId) : null
        if (item && item.familyId) return item
        var firstId = activeModel.firstFamilyId()
        return firstId !== "" ? activeModel.itemForFamily(firstId) : null
    }

    function selectedFilesForFamily(familyItem) {
        return familyItem && familyItem.selectedFiles ? familyItem.selectedFiles : ({})
    }

    function activeDownload(fileName, runtimeId, version) {
        var downloads = AppController.downloads.activeDownloads
        for (var i = 0; i < downloads.length; i++) {
            if (downloads[i].filename !== fileName) continue
            if (runtimeId && version && downloads[i].metadata) {
                if (downloads[i].metadata.id !== runtimeId || downloads[i].metadata.version !== version) continue
            }
            return downloads[i]
        }
        return null
    }

    function downloadProgressText(fileName, runtimeId, version) {
        var item = activeDownload(fileName, runtimeId, version)
        if (!item) return ""
        if (item.bytesTotal <= 0) return "Starting..."
        var pct = Math.round((item.bytesReceived / item.bytesTotal) * 100)
        return pct + "%"
    }

    function selectRuntime(runtime) {
        if (!runtime || !runtime.compatible || !runtime.installed) return
        pendingRuntimeId = runtime.id || ""
        pendingRuntimeVersion = runtime.version || ""
        var item = selectedFamilyItem()
        if (item && activeModel && activeModel.saveSelectionForFamily) {
            activeModel.saveSelectionForFamily(item.familyId, pendingRuntimeId, pendingRuntimeVersion, selectedFilesForFamily(item))
        }
    }

    function selectedFilesWithOverride(familyItem, role, fileName) {
        var source = selectedFilesForFamily(familyItem)
        var out = {}
        for (var key in source) out[key] = source[key]
        if (role) out[role] = fileName
        return out
    }

    function runtimeInstalled(runtimeId) {
        return runtimeId !== "" && AppController.runtimes.runtimeVersions(runtimeId).length > 0
    }

    function openRuntimeVersionManager(runtime) {
        if (!runtime) return
        runtimeVersionDialog.engineId = runtime.id || ""
        runtimeVersionDialog.engineName = runtime.name || qsTr("Runtime")
        runtimeVersionDialog.engineFamily = runtime.engineFamily || ""
        runtimeVersionDialog.assetName = runtime.asset || ""
        runtimeVersionDialog.sourceUrl = runtime.source || ""
        runtimeVersionDialog.defaultVersion = runtime.latestVersion || runtime.defaultVersion || runtime.version || ""
        runtimeVersionDialog.availableVersions = runtime.availableVersions || []
        runtimeVersionDialog.engineType = detailPanel.f && (detailPanel.f.familyCapability === "stt" || detailPanel.f.familyCapability === "voice-isolation")
                                          ? "stt"
                                          : (detailPanel.f && detailPanel.f.familyCapability === "forced-alignment" ? "alignment" : "tts")
        runtimeVersionDialog.accentColor = detailPanel.f ? (detailPanel.f.accent || Theme.accent) : Theme.accent
        runtimeVersionDialog.open()
    }

    function syncPendingRuntime(force) {
        var item = selectedFamilyItem()
        if (!item) return
        if (!force && pendingRuntimeId !== "") {
            for (var i = 0; i < item.runtimeOptions.length; i++) {
                var current = item.runtimeOptions[i]
                if (current.id === pendingRuntimeId && current.compatible && current.installed) return
            }
        }
        pendingRuntimeId = item.preferredRuntimeId || ""
        pendingRuntimeVersion = item.preferredRuntimeVersion || ""
    }

    function selectedStatus() {
        var item = selectedFamilyItem()
        if (!item) return { kind: "setup", title: qsTr("Setup Required") }
        var rawTitle = item.statusReason || "Setup Required"
        var translatedTitle = rawTitle
        if (rawTitle === "Ready") translatedTitle = qsTr("Ready")
        else if (rawTitle === "Setup Required") translatedTitle = qsTr("Setup Required")
        else if (rawTitle === "Incompatible") translatedTitle = qsTr("Incompatible")
        return { kind: item.statusKind || "setup", title: translatedTitle }
    }

    function setupInProgress(familyItem) {
        if (!familyItem) return false
        var files = familyItem.requiredFiles || []
        for (var i = 0; i < files.length; i++) {
            if (files[i].installState === 1 || files[i].installState === 2) return true
        }
        var runtimes = familyItem.runtimeOptions || []
        for (var r = 0; r < runtimes.length; r++) {
            if (runtimes[r].installState === 1 || runtimes[r].installState === 2) return true
        }
        return false
    }

    function primaryActionText(familyItem) {
        if (!familyItem) return qsTr("Use")
        if (familyItem.ready) return qsTr("Use")
        if (root.setupInProgress(familyItem)) return qsTr("Installing...")
        return qsTr("Quick install")
    }

    function primaryActionIcon(familyItem) {
        if (!familyItem) return "check"
        if (familyItem.ready) return root.modalMode ? "check" : "chevron-right"
        if (root.setupInProgress(familyItem)) return "download"
        return "download"
    }

    function primaryActionEnabled(familyItem) {
        if (!familyItem) return false
        return familyItem.ready || familyItem.statusKind !== "incompatible" || root.setupInProgress(familyItem)
    }

    function useSelectedFamily() {
        var familyItem = detailPanel.f
        if (!familyItem || !familyItem.ready) return
        if (root.modalMode) {
            var runtimeId = root.pendingRuntimeId !== "" ? root.pendingRuntimeId : familyItem.preferredRuntimeId
            var runtimeVersion = root.pendingRuntimeVersion !== "" ? root.pendingRuntimeVersion : familyItem.preferredRuntimeVersion
            root.configurationAccepted(familyItem.familyId, runtimeId, runtimeVersion, root.selectedFilesForFamily(familyItem))
        } else {
            root.openStudio(familyItem.familyCapability, familyItem.familyId)
        }
    }

    function installSelectedFamily() {
        var familyItem = detailPanel.f
        if (!familyItem) return
        if (root.setupInProgress(familyItem)) {
            Theme.requestShowDownloads()
            return
        }
        if (root.activeModel && root.activeModel.saveSelectionForFamily) {
            root.activeModel.saveSelectionForFamily(
                familyItem.familyId,
                familyItem.preferredRuntimeId || "",
                familyItem.preferredRuntimeVersion || "",
                root.selectedFilesForFamily(familyItem))
        }
        if (AppController.downloadInstall.enqueueRecommendedSetup(familyItem)) {
            Theme.requestShowDownloads()
        }
    }

    function runPrimaryAction() {
        var familyItem = detailPanel.f
        if (!familyItem) return
        if (familyItem.ready) root.useSelectedFamily()
        else root.installSelectedFamily()
    }

    function badgeFill(tone) {
        if (tone === "format") return Theme.accent
        if (tone === "accent") return Qt.rgba(0.49, 0.3, 1, 0.14)
        if (tone === "info") return Qt.rgba(0.25, 0.55, 1, 0.13)
        if (tone === "success") return Qt.rgba(0.4, 0.73, 0.42, 0.11)
        return Qt.rgba(1, 1, 1, 0.035)
    }

    function badgeBorder(tone) {
        if (tone === "format") return Theme.accent
        if (tone === "accent") return Qt.rgba(0.49, 0.3, 1, 0.28)
        if (tone === "info") return Qt.rgba(0.25, 0.55, 1, 0.27)
        if (tone === "success") return Qt.rgba(0.4, 0.73, 0.42, 0.28)
        return Qt.rgba(1, 1, 1, 0.09)
    }

    function badgeTextColor(tone) {
        if (tone === "format") return "#ffffff"
        if (tone === "accent") return Theme.accentLight
        if (tone === "info") return "#8fb8ff"
        if (tone === "success") return Theme.success
        return Theme.textPrimary
    }

    property string languageFilterSearch: ""
    readonly property bool hasLanguageFilter: root.activeModel && root.activeModel.availableLanguages && root.activeModel.availableLanguages.length > 1
    readonly property bool languageFilterActive: root.activeModel && root.activeModel.languageFilter && root.activeModel.languageFilter !== "all"
    readonly property var visibleLanguageOptions: {
        var items = root.activeModel && root.activeModel.availableLanguages ? root.activeModel.availableLanguages : []
        var term = root.languageFilterSearch.trim().toLowerCase()
        if (term === "") return items
        var filtered = []
        for (var i = 0; i < items.length; ++i) {
            var label = (items[i].text || "").toLowerCase()
            var value = (items[i].value || "").toLowerCase()
            if (label.indexOf(term) !== -1 || value.indexOf(term) !== -1) {
                filtered.push(items[i])
            }
        }
        return filtered
    }

    function languageOptionCountText() {
        if (!root.activeModel || !root.activeModel.availableLanguages) return ""
        var count = Math.max(0, root.activeModel.availableLanguages.length - 1)
        return qsTr("%1 languages").arg(count)
    }

    function selectedLanguageItem() {
        var items = root.activeModel && root.activeModel.availableLanguages ? root.activeModel.availableLanguages : []
        var currentFilter = root.activeModel ? root.activeModel.languageFilter : "all"
        for (var i = 0; i < items.length; ++i) {
            if (items[i].value === currentFilter) return items[i]
        }
        return items.length > 0 ? items[0] : { text: qsTr("All languages"), value: "all" }
    }

    function selectLanguageFilter(value) {
        if (!root.activeModel || root.activeModel.languageFilter === value) return
        root.activeModel.setLanguageFilter(value)
    }

    Connections {
        target: root.activeModel
        function onRevisionChanged() {
            root.ensureSelection()
            root.syncPendingRuntime(false)
        }
    }

    Connections {
        target: AppController.downloadInstall
        function onInstallStatesChanged() {
            if (root.activeModel) {
                root.activeModel.refresh()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.modalMode ? 0 : Theme.paddingXL
        spacing: Theme.paddingLarge

        // Left list panel
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.modalMode ? 280 : 360
            color: Theme.surface
            radius: Theme.radiusSmall
            border.color: Theme.surfaceAlt
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                // Search field
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: Theme.radiusSmall
                    color: Theme.background
                    border.color: searchField.activeFocus ? Theme.accent : Theme.surfaceAlt
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.paddingMedium
                        anchors.rightMargin: Theme.paddingSmall
                        spacing: Theme.paddingSmall

                        LineIcon {
                            name: "search"
                            color: searchField.activeFocus ? Theme.accent : Theme.textSecondary
                            Layout.preferredWidth: 15
                            Layout.preferredHeight: 15
                        }

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            placeholderText: qsTr("Search models...")
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            padding: 0
                            onTextChanged: root.searchText = text
                            background: Item {}
                        }
                    }
                }

                Rectangle {
                    id: languageFilterControl
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    visible: root.hasLanguageFilter
                    radius: Theme.radiusSmall
                    color: root.languageFilterActive ? Qt.rgba(0.49, 0.3, 1, 0.10) : Theme.background
                    border.color: languageFilterTap.pressed || languageFilterPopup.opened ? Theme.accent : (root.languageFilterActive ? Qt.rgba(0.49, 0.3, 1, 0.42) : Theme.surfaceAlt)
                    border.width: 1

                    TapHandler {
                        id: languageFilterTap
                        onTapped: {
                            root.languageFilterSearch = ""
                            languageFilterPopup.open()
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.paddingMedium
                        anchors.rightMargin: Theme.paddingSmall
                        spacing: Theme.paddingSmall

                        Rectangle {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            Layout.alignment: Qt.AlignVCenter
                            radius: 7
                            color: root.languageFilterActive ? Qt.rgba(0.49, 0.3, 1, 0.16) : Qt.rgba(1, 1, 1, 0.035)
                            border.color: root.languageFilterActive ? Qt.rgba(0.49, 0.3, 1, 0.34) : Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            LineIcon {
                                anchors.centerIn: parent
                                name: "globe"
                                color: root.languageFilterActive ? Theme.accentLight : Theme.textSecondary
                                width: 15
                                height: 15
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Language")
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: root.selectedLanguageItem().value === "all" ? qsTr("All languages") : root.selectedLanguageItem().text
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: root.languageFilterActive
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: root.selectedLanguageItem().value !== "all"
                                    implicitWidth: languageCodeText.implicitWidth + 10
                                    implicitHeight: 18
                                    radius: 5
                                    color: Qt.rgba(1, 1, 1, 0.045)
                                    border.color: Qt.rgba(1, 1, 1, 0.09)
                                    border.width: 1

                                    Text {
                                        id: languageCodeText
                                        anchors.centerIn: parent
                                        text: root.selectedLanguageItem().value
                                        color: Theme.textSecondary
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        Button {
                            visible: root.languageFilterActive
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            onClicked: {
                                root.selectLanguageFilter("all")
                                languageFilterPopup.close()
                            }
                            contentItem: LineIcon {
                                name: "close"
                                color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                                anchors.centerIn: parent
                                width: 12
                                height: 12
                            }
                            background: Rectangle {
                                radius: 7
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                                border.color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                                border.width: 1
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }

                        LineIcon {
                            name: "chevron-down"
                            color: languageFilterPopup.opened ? Theme.accentLight : Theme.textSecondary
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    Popup {
                        id: languageFilterPopup
                        x: 0
                        y: parent.height + 6
                        width: parent.width
                        implicitHeight: languageFilterPopupContent.implicitHeight
                        padding: 0
                        modal: false
                        focus: true
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                        onOpened: languageSearchField.forceActiveFocus()

                        contentItem: ColumnLayout {
                            id: languageFilterPopupContent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: root.activeModel && root.activeModel.availableLanguages && root.activeModel.availableLanguages.length > 10 ? 44 : 0
                                visible: implicitHeight > 0
                                color: Theme.surface

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: Theme.surfaceAlt
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.paddingMedium
                                    anchors.rightMargin: Theme.paddingMedium
                                    spacing: Theme.paddingSmall

                                    LineIcon {
                                        name: "search"
                                        color: Theme.textSecondary
                                        Layout.preferredWidth: 14
                                        Layout.preferredHeight: 14
                                    }

                                    TextField {
                                        id: languageSearchField
                                        Layout.fillWidth: true
                                        placeholderText: qsTr("Search languages...")
                                        text: root.languageFilterSearch
                                        color: Theme.textPrimary
                                        placeholderTextColor: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        padding: 0
                                        background: Item {}
                                        onTextChanged: root.languageFilterSearch = text

                                        Keys.onPressed: (event) => {
                                            if (event.key === Qt.Key_Escape) {
                                                languageFilterPopup.close()
                                                event.accepted = true
                                            }
                                        }
                                    }
                                }
                            }

                            ListView {
                                id: languageList
                                Layout.fillWidth: true
                                implicitHeight: Math.min(contentHeight, 280)
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                model: root.visibleLanguageOptions

                                delegate: ItemDelegate {
                                    id: languageDelegate
                                    width: languageList.width
                                    height: 38

                                    readonly property bool selected: root.activeModel && modelData.value === root.activeModel.languageFilter

                                    background: Rectangle {
                                        color: languageDelegate.selected ? Qt.rgba(0.49, 0.3, 1, 0.14) : (languageDelegate.hovered ? Qt.rgba(1, 1, 1, 0.04) : "transparent")
                                    }

                                    contentItem: RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.paddingMedium
                                        anchors.rightMargin: Theme.paddingMedium
                                        spacing: Theme.paddingSmall

                                        LineIcon {
                                            name: languageDelegate.selected ? "check" : "globe"
                                            color: languageDelegate.selected ? Theme.accentLight : Theme.textSecondary
                                            Layout.preferredWidth: 13
                                            Layout.preferredHeight: 13
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.value === "all" ? qsTr("All languages") : modelData.text
                                            color: languageDelegate.selected ? Theme.textPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSmall
                                            font.bold: languageDelegate.selected
                                            elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        Text {
                                            visible: modelData.value === "all"
                                            text: root.languageOptionCountText()
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            visible: modelData.value !== "all"
                                            implicitWidth: languageOptionCode.implicitWidth + 10
                                            implicitHeight: 18
                                            radius: 5
                                            color: Qt.rgba(1, 1, 1, 0.035)
                                            border.color: Qt.rgba(1, 1, 1, 0.08)
                                            border.width: 1

                                            Text {
                                                id: languageOptionCode
                                                anchors.centerIn: parent
                                                text: modelData.value
                                                color: Theme.textSecondary
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }
                                    }

                                    onClicked: {
                                        root.selectLanguageFilter(modelData.value)
                                        languageFilterPopup.close()
                                    }
                                }
                            }
                        }

                        background: Rectangle {
                            color: Theme.background
                            radius: Theme.radiusSmall
                            border.color: Theme.surfaceAlt
                            border.width: 1
                        }
                    }
                }

                ListView {
                    id: familyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.paddingSmall
                    model: root.activeModel
                    onCountChanged: root.ensureSelection()

                    delegate: Rectangle {
                        width: familyList.width
                        height: root.modalMode ? 84 : 96
                        radius: 7
                        color: root.selectedFamilyId === model.familyId ? Qt.rgba(1, 1, 1, 0.035) : (itemHover.hovered ? Qt.rgba(1, 1, 1, 0.025) : "transparent")
                        border.color: root.selectedFamilyId === model.familyId ? (model.accent || Theme.accent) : (itemHover.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        border.width: 1

                        HoverHandler { id: itemHover }
                        TapHandler {
                            onTapped: {
                                root.selectedFamilyId = model.familyId
                                root.familySelected(model.familyId)
                            }
                        }

                        property var rowStatsBadges: model.statsBadges || []

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 7
                                color: Qt.rgba(1, 1, 1, 0.035)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1
                                clip: true
                                Image {
                                    id: listThumbnail
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    source: model.thumbnailSource
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    visible: source !== "" && status !== Image.Error
                                }
                                LineIcon {
                                    anchors.centerIn: parent
                                    name: model.iconName
                                    color: model.accent || Theme.accent
                                    width: 17
                                    height: 17
                                    visible: !listThumbnail.visible
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                RowLayout {
                                    id: listNameRow
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingSmall

                                    Row {
                                        id: nameContainer
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            width: Math.min(implicitWidth, nameContainer.width - (model.isLastudioPick ? 22 : 0))
                                            text: model.displayName
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontSmall
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            width: 16
                                            height: 16
                                            radius: 8
                                            color: "#3f7cff"
                                            border.color: "#a7c0ff"
                                            border.width: 1
                                            visible: model.isLastudioPick

                                            LineIcon {
                                                anchors.centerIn: parent
                                                name: "check"
                                                color: "#ffffff"
                                                width: 10
                                                height: 10
                                                strokeWidth: 2.2
                                            }

                                            HoverHandler { id: listPickHover }

                                            AppToolTip {
                                                visible: listPickHover.hovered
                                                text: model.pickReason || qsTr("LA Studio Pick")
                                            }
                                        }
                                    }

                                    Rectangle {
                                        id: listStatusBadge
                                        implicitWidth: statusRow.width + 10
                                        implicitHeight: 20
                                        radius: 6
                                        color: model.ready ? Qt.rgba(0.40, 0.73, 0.42, 0.10) : (model.statusReason === "Incompatible" ? Qt.rgba(0.93, 0.33, 0.36, 0.12) : Qt.rgba(1.0, 0.65, 0.15, 0.10))
                                        border.color: model.ready ? Qt.rgba(0.40, 0.73, 0.42, 0.55) : (model.statusReason === "Incompatible" ? Qt.rgba(0.93, 0.33, 0.36, 0.60) : Qt.rgba(1.0, 0.65, 0.15, 0.55))
                                        border.width: 1

                                        Row {
                                            id: statusRow
                                            anchors.centerIn: parent
                                            spacing: 4

                                            LineIcon {
                                                name: model.ready ? "check" : (model.statusReason === "Incompatible" ? "close" : "download")
                                                color: model.ready ? Theme.success : (model.statusReason === "Incompatible" ? Theme.danger : Theme.warning)
                                                width: 10
                                                height: 10
                                            }

                                            Text {
                                                text: {
                                                    if (model.statusReason === "Ready") return qsTr("Ready")
                                                    if (model.statusReason === "Setup Required") return qsTr("Setup Required")
                                                    if (model.statusReason === "Incompatible") return qsTr("Incompatible")
                                                    return model.statusReason || ""
                                                }
                                                color: model.ready ? Theme.success : (model.statusReason === "Incompatible" ? Theme.danger : Theme.warning)
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: model.subtitle
                                    color: Theme.textSecondary
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: rowStatsBadges.length > 0

                                    Repeater {
                                        model: rowStatsBadges
                                        RowLayout {
                                            spacing: 3
                                            LineIcon {
                                                name: modelData.label.indexOf("Download") !== -1 ? "download" : "star"
                                                color: root.badgeTextColor(modelData.tone)
                                                Layout.preferredWidth: 10
                                                Layout.preferredHeight: 10
                                                strokeWidth: 1.8
                                            }
                                            Text {
                                                text: modelData.value
                                                color: root.badgeTextColor(modelData.tone)
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Right details panel
        Rectangle {
            id: detailPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusSmall
            border.color: Theme.surfaceAlt
            border.width: 1

            property var f: root.activeModel && root.activeModel.revision >= 0
                ? root.selectedFamilyItem() : null

            ScrollView {
                id: detailScroll
                anchors.fill: parent
                clip: true

                ColumnLayout {
                    x: Theme.paddingLarge
                    y: Theme.paddingLarge
                    width: Math.max(0, detailScroll.availableWidth - (Theme.paddingLarge * 2))
                    spacing: Theme.paddingLarge

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingMedium

                        Rectangle {
                            width: 54
                            height: 54
                            radius: 8
                            color: Theme.background
                            border.color: detailPanel.f ? (detailPanel.f.accent || Theme.accent) : Theme.accent
                            border.width: 1
                            clip: true
                            Image {
                                id: detailThumbnail
                                anchors.fill: parent
                                anchors.margins: 7
                                source: detailPanel.f ? detailPanel.f.thumbnailSource : ""
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                visible: source !== "" && status !== Image.Error
                            }
                            LineIcon {
                                anchors.centerIn: parent
                                name: detailPanel.f ? detailPanel.f.iconName : "volume"
                                color: detailPanel.f ? (detailPanel.f.accent || Theme.accent) : Theme.accent
                                width: 24
                                height: 24
                                visible: !detailThumbnail.visible
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Row {
                                id: detailNameRow
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall

                                Text {
                                    width: Math.min(implicitWidth, detailNameRow.width - (detailPanel.f && detailPanel.f.isLastudioPick ? 26 : 0))
                                    text: detailPanel.f ? detailPanel.f.displayName : ""
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontLarge
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: "#3f7cff"
                                    border.color: "#a7c0ff"
                                    border.width: 1
                                    visible: detailPanel.f && detailPanel.f.isLastudioPick

                                    LineIcon {
                                        anchors.centerIn: parent
                                        name: "check"
                                        color: "#ffffff"
                                        width: 11
                                        height: 11
                                        strokeWidth: 2.2
                                    }

                                    HoverHandler { id: detailPickHover }

                                    AppToolTip {
                                        visible: detailPickHover.hovered
                                        text: detailPanel.f ? (detailPanel.f.pickReason || qsTr("LA Studio Pick")) : ""
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: detailPanel.f ? detailPanel.f.rawMetadata.modelId : ""
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                elide: Text.ElideRight
                            }
                        }

                        PrimaryButton {
                            text: root.primaryActionText(detailPanel.f)
                            iconName: root.primaryActionIcon(detailPanel.f)
                            enabled: root.primaryActionEnabled(detailPanel.f)
                            implicitWidth: 132
                            implicitHeight: 40
                            onClicked: root.runPrimaryAction()
                        }

                        Rectangle {
                            implicitWidth: headerState.implicitWidth + 16
                            implicitHeight: 28
                            radius: Theme.radiusSmall
                            color: Theme.background
                            border.color: {
                                var status = root.selectedStatus()
                                if (status.kind === "ready") return Theme.success
                                if (status.kind === "incompatible") return Theme.danger
                                return Theme.warning
                            }
                            border.width: 1
                            RowLayout {
                                id: headerState
                                anchors.centerIn: parent
                                spacing: Theme.paddingSmall
                                LineIcon {
                                    name: {
                                        var status = root.selectedStatus()
                                        if (status.kind === "ready") return "check"
                                        if (status.kind === "incompatible") return "close"
                                        return "download"
                                    }
                                    color: {
                                        var status = root.selectedStatus()
                                        if (status.kind === "ready") return Theme.success
                                        if (status.kind === "incompatible") return Theme.danger
                                        return Theme.warning
                                    }
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                }
                                Text {
                                    text: root.selectedStatus().title
                                    color: {
                                        var status = root.selectedStatus()
                                        if (status.kind === "ready") return Theme.success
                                        if (status.kind === "incompatible") return Theme.danger
                                        return Theme.warning
                                    }
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                }
                            }
                        }

                        PrimaryButton {
                            text: detailPanel.f && detailPanel.f.isLastudioPick ? qsTr("LA Studio Pick") : qsTr("Model Card")
                            iconName: detailPanel.f && detailPanel.f.isLastudioPick ? "external-link" : "file"
                            quiet: true
                            implicitWidth: detailPanel.f && detailPanel.f.isLastudioPick ? 148 : 122
                            implicitHeight: 34
                            visible: detailPanel.f && detailPanel.f.modelCardUrl !== ""
                            onClicked: Qt.openUrlExternally(detailPanel.f.modelCardUrl)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.paddingSmall
                            visible: detailPanel.f && detailPanel.f.statsBadges && detailPanel.f.statsBadges.length > 0

                            Repeater {
                                model: detailPanel.f ? (detailPanel.f.statsBadges || []) : []
                                Rectangle {
                                    implicitWidth: statRow.implicitWidth + 14
                                    implicitHeight: 26
                                    radius: 6
                                    color: root.badgeFill(modelData.tone)
                                    border.color: root.badgeBorder(modelData.tone)
                                    border.width: 1
                                    HoverHandler { id: statHover }

                                    RowLayout {
                                        id: statRow
                                        anchors.centerIn: parent
                                        spacing: 6
                                        LineIcon {
                                            name: modelData.label.indexOf("Download") !== -1 ? "download" : "star"
                                            color: root.badgeTextColor(modelData.tone)
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                            strokeWidth: 1.8
                                        }
                                        Text {
                                            text: modelData.label + ": " + modelData.value
                                            color: root.badgeTextColor(modelData.tone)
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }

                                    AppToolTip {
                                        visible: statHover.hovered && modelData.source
                                        text: modelData.source
                                    }
                                }
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.paddingMedium

                            Repeater {
                                model: detailPanel.f ? (detailPanel.f.infoBadges || []) : []
                                Row {
                                    spacing: 6
                                    height: 24
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.label
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: true
                                    }
                                    Rectangle {
                                        width: infoValue.implicitWidth + 14
                                        height: 24
                                        radius: 5
                                        color: root.badgeFill(modelData.tone)
                                        border.color: root.badgeBorder(modelData.tone)
                                        border.width: 1
                                        Text {
                                            id: infoValue
                                            anchors.centerIn: parent
                                            text: modelData.value
                                            color: root.badgeTextColor(modelData.tone)
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: detailPanel.f && detailPanel.f.capabilityBadges && detailPanel.f.capabilityBadges.length > 0

                            Text {
                                height: 22
                                verticalAlignment: Text.AlignVCenter
                                text: qsTr("Capabilities:")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }

                            Repeater {
                                model: detailPanel.f ? (detailPanel.f.capabilityBadges || []) : []
                                Rectangle {
                                    implicitWidth: capabilityValue.implicitWidth + 16
                                    implicitHeight: 22
                                    radius: 5
                                    color: root.badgeFill(modelData.tone)
                                    border.color: root.badgeBorder(modelData.tone)
                                    border.width: 1
                                    Text {
                                        id: capabilityValue
                                        anchors.centerIn: parent
                                        text: modelData.value
                                        color: root.badgeTextColor(modelData.tone)
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }

                    // Description
                    Text {
                        Layout.fillWidth: true
                        text: detailPanel.f ? detailPanel.f.description : ""
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontMedium
                        wrapMode: Text.WordWrap
                    }

                    // Required files
                    Text {
                        text: qsTr("Required Files")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        visible: !!(detailPanel.f && detailPanel.f.requiredFiles && detailPanel.f.requiredFiles.length > 0)
                    }

                    Repeater {
                        model: detailPanel.f ? (detailPanel.f.requiredFiles || []) : []

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: fileLayout.implicitHeight + Theme.paddingLarge
                            radius: 7
                            color: Theme.background
                            border.color: Theme.surfaceAlt
                            border.width: 1

                            property int modelRevision: root.activeModel ? root.activeModel.revision : 0
                            property var family: modelRevision >= 0 ? detailPanel.f : null
                            property var familyMetadata: family ? family.rawMetadata : null
                            property string selectedFile: modelData.selectedFile || ""
                            property int installState: {
                                if (modelData.installState === 3) return 3; // Installed
                                if (download) return 1; // Downloading
                                if (modelData.installState !== undefined) return modelData.installState;
                                return 0;
                            }
                            property var download: root.activeDownload(modelData.selectedFile || "")

                            ColumnLayout {
                                id: fileLayout
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Theme.paddingMedium
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
                                            font.pixelSize: Theme.fontSmall
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.purpose + " - " + modelData.selectedFile + (modelData.selectedSize ? " - " + modelData.selectedSize : "")
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }

                                    AppComboBox {
                                        visible: modelData.candidates !== undefined && modelData.candidates.length > 0
                                        Layout.fillWidth: root.modalMode
                                        Layout.preferredWidth: root.modalMode ? 0 : 240
                                        Layout.minimumWidth: root.modalMode ? 220 : 160
                                        Layout.maximumWidth: root.modalMode ? 360 : 280
                                        Layout.preferredHeight: 34
                                        model: modelData.candidates ? modelData.candidates : []
                                        currentIndex: model.indexOf(modelData.selectedFile)

                                        isModelSelector: true
                                        familiesModel: root.activeModel
                                        modelFamily: familyMetadata
                                        modelRequirement: modelData
                                        defaultFile: modelData.file
                                        defaultSize: modelData.size || ""

                                        onActivated: (index) => {
                                            if (root.activeModel && family) {
                                                var selected = model[index]
                                                root.activeModel.selectFileForRequirement(family.familyId, modelData.file, selected)
                                                if (root.activeModel.saveSelectionForFamily) {
                                                    var runtimeId = root.pendingRuntimeId !== "" ? root.pendingRuntimeId : family.preferredRuntimeId
                                                    var runtimeVersion = root.pendingRuntimeVersion !== "" ? root.pendingRuntimeVersion : family.preferredRuntimeVersion
                                                    root.activeModel.saveSelectionForFamily(
                                                        family.familyId,
                                                        runtimeId,
                                                        runtimeVersion,
                                                        root.selectedFilesWithOverride(family, modelData.role, selected))
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.minimumWidth: 76
                                        text: {
                                            if (installState === 3) return qsTr("Installed")
                                            if (installState === 5) return qsTr("Update available")
                                            if (installState === 1) return root.downloadProgressText(selectedFile)
                                            return qsTr("Not installed")
                                        }
                                        color: installState === 3 ? Theme.success : ((installState === 1 || installState === 5) ? Theme.warning : Theme.textSecondary)
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: installState === 3 || installState === 5
                                        elide: Text.ElideRight
                                    }

                                    PrimaryButton {
                                        text: installState === 1 ? qsTr("Downloading") : (installState === 5 ? qsTr("Update") : qsTr("Download"))
                                        iconName: installState === 1 ? "" : "download"
                                        enabled: installState === 0 || installState === 1 || installState === 5
                                        visible: installState === 0 || installState === 1 || installState === 5
                                        implicitWidth: 100
                                        implicitHeight: 32
                                        Layout.minimumWidth: 100
                                        quiet: true
                                        onClicked: {
                                            if (installState === 1) {
                                                Theme.requestShowDownloads()
                                            } else {
                                                AppController.downloadInstall.enqueueModelFile(familyMetadata, modelData)
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 6
                                    radius: 3
                                    color: Theme.surfaceAlt
                                    visible: installState === 1
                                    Rectangle {
                                        height: parent.height
                                        radius: 3
                                        color: Theme.accent
                                        width: download && download.bytesTotal > 0 ? parent.width * (download.bytesReceived / download.bytesTotal) : 0
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !!(detailPanel.f && detailPanel.f.runtimeOptions && detailPanel.f.runtimeOptions.length > 0)

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: qsTr("Runtime")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontMedium
                                font.bold: true
                            }
                            Text {
                                text: qsTr("Compatibility is evaluated from detected CPU and GPU capabilities.")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            implicitWidth: hardwareState.implicitWidth + 16
                            implicitHeight: 28
                            radius: 7
                            color: Qt.rgba(1, 1, 1, 0.035)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1
                            RowLayout {
                                id: hardwareState
                                anchors.centerIn: parent
                                spacing: 6
                                LineIcon {
                                    name: "cpu"
                                    color: Theme.textSecondary
                                    Layout.preferredWidth: 13
                                    Layout.preferredHeight: 13
                                }
                                Text {
                                    text: HardwareManager.gpus.length > 0 ? qsTr("%1 GPU detected").arg(HardwareManager.gpus.length) : qsTr("CPU only")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Repeater {
                        model: detailPanel.f ? (detailPanel.f.runtimeOptions || []) : []

                        delegate: Rectangle {
                            id: runtimeRow
                            Layout.fillWidth: true
                            implicitHeight: runtimeLayout.implicitHeight + Theme.paddingLarge
                            radius: 7
                            color: Theme.background
                            border.color: runtimeRow.selected ? Theme.accent : Theme.surfaceAlt
                            border.width: 1

                            property int installState: {
                                if (modelData.installState === 3) return 3; // Installed
                                if (download) return 1; // Downloading
                                if (modelData.installState !== undefined) return modelData.installState;
                                return 0;
                            }
                            property string effectiveVersion: modelData.id === root.pendingRuntimeId && root.pendingRuntimeVersion !== ""
                                                              ? root.pendingRuntimeVersion
                                                              : (modelData.version || "")
                            property bool selected: modelData.id === root.pendingRuntimeId && installState === 3
                            property var download: root.activeDownload(modelData.asset, modelData.id, runtimeRow.effectiveVersion)

                            RowLayout {
                                id: runtimeLayout
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Theme.paddingMedium
                                spacing: Theme.paddingMedium

                                LineIcon {
                                    name: modelData.iconName || "spark"
                                    color: modelData.compatible ? (installState === 3 ? Theme.success : Theme.textPrimary) : Theme.textSecondary
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: Math.max(220, Math.min(320, runtimeRow.width * 0.32))
                                    Layout.minimumWidth: 180
                                    Layout.maximumWidth: 340
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name || modelData.label || modelData.id
                                        color: modelData.compatible ? Theme.textPrimary : Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description || ""
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }

                                Item {
                                    Layout.preferredWidth: 80
                                    Layout.minimumWidth: 80
                                    Layout.alignment: Qt.AlignVCenter
                                    implicitHeight: 20

                                    Rectangle {
                                        width: runtimeVersionText.implicitWidth + 12
                                        height: 20
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        radius: 6
                                        color: Qt.rgba(1, 1, 1, 0.035)
                                        border.color: Qt.rgba(1, 1, 1, 0.08)
                                        border.width: 1
                                        visible: runtimeRow.effectiveVersion !== ""
                                        Text {
                                            id: runtimeVersionText
                                            anchors.centerIn: parent
                                            text: runtimeRow.effectiveVersion
                                            color: Theme.textSecondary
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: 220
                                    Layout.minimumWidth: 178
                                    spacing: 4
                                    RowLayout {
                                        Layout.alignment: Qt.AlignRight
                                        spacing: 6
                                        LineIcon {
                                            name: modelData.compatible ? "check" : "close"
                                            color: modelData.compatible ? Theme.success : Theme.warning
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                        }
                                        Text {
                                            text: modelData.compatibilityTitle || (modelData.compatible ? qsTr("Compatible") : qsTr("Unavailable"))
                                            color: modelData.compatible ? Theme.success : Theme.warning
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.compatibilityDetail || ""
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignRight
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                PrimaryButton {
                                    text: {
                                        if (installState === 3) return runtimeRow.selected ? qsTr("Selected") : qsTr("Use");
                                        if (installState === 1) return root.downloadProgressText(modelData.asset, modelData.id, runtimeRow.effectiveVersion);
                                        if (installState === 2) return qsTr("Installing...");
                                        return qsTr("Download");
                                    }
                                    iconName: {
                                        if (installState === 3) return "check";
                                        if (installState === 2) return "";
                                        return "download";
                                    }
                                    enabled: modelData.compatible && (installState !== 2)
                                    implicitWidth: 108
                                    implicitHeight: 32
                                    quiet: (installState === 3 && !runtimeRow.selected) || installState === 1
                                    onClicked: {
                                        if (installState === 3) root.selectRuntime(modelData)
                                        else if (installState === 1) Theme.requestShowDownloads()
                                        else AppController.downloadInstall.enqueueRuntime(detailPanel.f.rawMetadata, detailPanel.f.familyCapability, detailPanel.f.familyId, modelData)
                                    }
                                }

                                Button {
                                    id: runtimeMenuButton
                                    implicitWidth: 32
                                    implicitHeight: 34
                                    onClicked: {
                                        runtimeMenu.currentRuntime = modelData
                                        runtimeMenu.rebuild()
                                        runtimeMenu.popup(runtimeMenuButton, 0, runtimeMenuButton.height + 4)
                                    }
                                    contentItem: LineIcon {
                                        name: "more-horizontal"
                                        color: runtimeMenuButton.hovered ? Theme.textPrimary : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 17
                                        height: 17
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: runtimeMenuButton.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                                        border.color: runtimeMenuButton.hovered ? Qt.rgba(1, 1, 1, 0.09) : "transparent"
                                        border.width: 1
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: readmeLayout.implicitHeight
                        radius: 7
                        color: Theme.background
                        border.color: Theme.surfaceAlt
                        border.width: 1
                        clip: true
                        visible: detailPanel.f && detailPanel.f.readmeContent !== ""

                        ColumnLayout {
                            id: readmeLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 46
                                color: Qt.rgba(1, 1, 1, 0.025)
                                border.color: Theme.surfaceAlt
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.paddingMedium
                                    anchors.rightMargin: Theme.paddingMedium
                                    spacing: Theme.paddingSmall

                                    LineIcon {
                                        name: "file"
                                        color: Theme.textPrimary
                                        Layout.preferredWidth: 15
                                        Layout.preferredHeight: 15
                                    }

                                    Text {
                                         text: qsTr("README")
                                         color: Theme.textPrimary
                                         font.pixelSize: Theme.fontMedium
                                         font.bold: true
                                         Layout.fillWidth: true
                                     }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.margins: Theme.paddingMedium
                                text: detailPanel.f ? detailPanel.f.readmeContent : ""
                                textFormat: Text.MarkdownText
                                color: Theme.textPrimary
                                linkColor: detailPanel.f ? (detailPanel.f.accent || Theme.accent) : Theme.accent
                                font.pixelSize: 13
                                lineHeight: 1.18
                                wrapMode: Text.WordWrap
                                onLinkActivated: function(link) {
                                    Qt.openUrlExternally(link)
                                }
                            }
                        }
                    }

                }
            }
        }
    }

    AppContextMenu {
        id: runtimeMenu
        property var currentRuntime: null

        items: [
            { text: qsTr("Installed version details"), subText: qsTr("View installed runtime information"), iconName: "file", enabled: function() { return runtimeMenu.currentRuntime ? root.runtimeInstalled(runtimeMenu.currentRuntime.id) : false }, action: function() {} },
            { type: "separator" },
            { text: qsTr("Manage versions"), subText: qsTr("Install or select runtime versions"), iconName: "settings", action: function() { root.openRuntimeVersionManager(runtimeMenu.currentRuntime) } }
        ]
    }

    RuntimeVersionManagerDialog {
        id: runtimeVersionDialog
        onVersionSelected: function(runtimeId, version) {
            root.pendingRuntimeId = runtimeId
            root.pendingRuntimeVersion = version
            var item = root.selectedFamilyItem()
            if (item && root.activeModel && root.activeModel.saveSelectionForFamily) {
                root.activeModel.saveSelectionForFamily(item.familyId, runtimeId, version, root.selectedFilesForFamily(item))
            }
            if (root.activeModel) root.activeModel.refresh()
        }
    }
}
