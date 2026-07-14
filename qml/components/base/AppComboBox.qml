import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

ComboBox {
    id: root

    property string secondaryTextRole: ""
    property bool searchable: false
    property string filterText: ""
    property var filteredModel: []

    property bool isModelSelector: false
    property var modelFamily: null
    property var modelRequirement: null
    property string defaultFile: ""
    property string defaultSize: ""

    function getModelCount() {
        if (!root.model) return 0;
        if (typeof root.model.count === "number") return root.model.count;
        if (typeof root.model.length === "number") return root.model.length;
        return 0;
    }

    function getModelItem(index) {
        if (!root.model) return null;
        if (typeof root.model.get === "function") return root.model.get(index);
        return root.model[index];
    }

    function updateFilteredModel() {
        var count = getModelCount();
        var list = [];
        var term = filterText.trim().toLowerCase();
        for (var i = 0; i < count; i++) {
            var item = getModelItem(i);
            var primary = root.primaryTextFor(item);
            var secondary = root.secondaryTextFor(item);
            
            if (term === "" || 
                primary.toLowerCase().indexOf(term) !== -1 || 
                secondary.toLowerCase().indexOf(term) !== -1) {
                list.push({
                    originalIndex: i,
                    itemData: item
                });
            }
        }
        filteredModel = list;
    }

    onModelChanged: {
        if (root.searchable) {
            updateFilteredModel();
        }
    }

    onSearchableChanged: {
        if (root.searchable) {
            updateFilteredModel();
        }
    }

    Component.onCompleted: {
        if (root.searchable) {
            updateFilteredModel();
        }
    }

    function selectedItem() {
        if (!root.model || root.currentIndex < 0) return null
        return getModelItem(root.currentIndex)
    }

    function primaryTextFor(item) {
        if (item && typeof item === "object") {
            return item[root.textRole] || item.text || ""
        }
        return item || ""
    }

    function secondaryTextFor(item) {
        if (!item || root.secondaryTextRole === "" || typeof item !== "object") return ""
        return item[root.secondaryTextRole] || ""
    }

    property var familiesModel: null

    function estimateSize(filename, defaultFile, defaultSizeStr) {
        if (familiesModel && typeof familiesModel.estimateSize === "function") {
            return familiesModel.estimateSize(filename, defaultFile, defaultSizeStr);
        }
        return defaultSizeStr || "";
    }

    function isModelSuitable(filename, family, requirement) {
        if (familiesModel && typeof familiesModel.isModelSuitable === "function") {
            return familiesModel.isModelSuitable(filename, family, requirement || null);
        }
        return true;
    }

    function isDownloaded(filename) {
        if (familiesModel && typeof familiesModel.isFileInstalled === "function") {
            return familiesModel.isFileInstalled(root.modelFamily, filename, root.modelRequirement);
        }
        return false;
    }

    function getQuantization(filename) {
        var lower = filename.toLowerCase();
        var match = filename.match(/-([qQ]\d+_[A-Za-z0-9_]+|[qQ]\d+_[A-Za-z0-9]|[qQ]\d+|[bB][fF]16|[fF]16|[fF]32)\.gguf$/);
        if (match) {
            return match[1].toUpperCase();
        }
        if (lower.indexOf("q4_k_m") !== -1) return "Q4_K_M";
        if (lower.indexOf("q8_0") !== -1) return "Q8_0";
        if (lower.indexOf("bf16") !== -1) return "BF16";
        if (lower.indexOf("f16") !== -1) return "F16";
        if (lower.indexOf("f32") !== -1) return "F32";
        return "";
    }

    function getCleanedName(filename) {
        var quant = getQuantization(filename);
        var clean = filename;
        if (clean.endsWith(".gguf")) {
            clean = clean.substring(0, clean.length - 5);
        }
        if (quant !== "") {
            var idx = clean.toUpperCase().lastIndexOf("-" + quant.toUpperCase());
            if (idx !== -1) {
                clean = clean.substring(0, idx);
            } else {
                idx = clean.toUpperCase().lastIndexOf("_" + quant.toUpperCase());
                if (idx !== -1) {
                    clean = clean.substring(0, idx);
                }
            }
        }
        return clean;
    }

    function getVariantInfo(filename) {
        if (!root.modelRequirement || !root.modelRequirement.variants) return null;
        var variants = root.modelRequirement.variants;
        for (var i = 0; i < variants.length; i++) {
            var v = variants[i];
            var vName = v.name || v.file || "";
            if (vName === filename) {
                return v;
            }
        }
        return null;
    }

    function getVariantDisplayName(filename) {
        var info = getVariantInfo(filename);
        if (info) {
            var label = "";
            if (info.speaker) {
                label = info.speaker;
                if (info.language) {
                    label += " (" + info.language + ")";
                }
            } else if (info.language) {
                label = info.language;
            }
            if (label !== "") {
                return label;
            }
        }
        return root.getCleanedName(filename);
    }

    implicitHeight: 40

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusSmall
        border.color: root.activeFocus ? Theme.accent : Theme.surfaceAlt
        border.width: 1
    }

    contentItem: Text {
        text: {
            const item = root.selectedItem()
            if (!item) return "Select..."
            const primary = root.primaryTextFor(item)
            if (root.isModelSelector) {
                var quant = root.getQuantization(primary)
                var displayName = root.getVariantDisplayName(primary)
                var size = root.estimateSize(primary, root.defaultFile, root.defaultSize)
                if (quant !== "") {
                    return displayName + " [" + quant + "]  ·  " + size
                }
                return displayName + "  ·  " + size
            }
            const secondary = root.secondaryTextFor(item)
            if (primary !== "" && secondary !== "") return primary + "  ·  " + secondary
            return primary !== "" ? primary : "Select..."
        }
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: Theme.fontMedium
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.paddingMedium
        rightPadding: Theme.paddingXL
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        width: comboPopup.width
        height: root.isModelSelector ? 44 : 34
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: highlighted ? Theme.surfaceAlt : "transparent"
        }
        contentItem: Loader {
            anchors.fill: parent
            sourceComponent: root.isModelSelector ? modelSelectorDelegateContent : defaultDelegateContent
            
            onLoaded: {
                if (root.isModelSelector) {
                    item.filename = modelData;
                } else {
                    item.itemData = modelData;
                }
            }
        }
    }

    popup: Popup {
        id: comboPopup
        x: root.isModelSelector ? Math.min(0, root.width - width) : 0
        y: root.height + 4
        width: root.isModelSelector ? Math.max(root.width, 550) : root.width
        implicitHeight: contentItem.implicitHeight
        padding: 0
        
        onOpened: {
            if (root.searchable) {
                root.filterText = ""
                root.updateFilteredModel()
                
                // Find matching originalIndex in filteredModel to set initial currentIndex
                var found = 0
                for (var i = 0; i < root.filteredModel.length; i++) {
                    if (root.filteredModel[i].originalIndex === root.currentIndex) {
                        found = i
                        break
                    }
                }
                filteredList.currentIndex = found
                searchField.forceActiveFocus()
            }
        }
        
        contentItem: ColumnLayout {
            spacing: 0
            
            Rectangle {
                id: searchFieldContainer
                Layout.fillWidth: true
                implicitHeight: root.searchable ? 44 : 0
                visible: root.searchable
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
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "Search..."
                        text: root.filterText
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        font.pixelSize: Theme.fontMedium
                        verticalAlignment: TextInput.AlignVCenter
                        background: null
                        padding: 0
                        
                        onTextChanged: {
                            if (root.filterText !== text) {
                                root.filterText = text
                                root.updateFilteredModel()
                                filteredList.currentIndex = 0
                            }
                        }
                        
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Down) {
                                if (filteredList.count > 0) {
                                    filteredList.forceActiveFocus()
                                    filteredList.currentIndex = 0
                                }
                                event.accepted = true
                            } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                                if (root.filteredModel.length > 0) {
                                    var targetIdx = root.filteredModel[0].originalIndex
                                    root.currentIndex = targetIdx
                                    root.activated(targetIdx)
                                    comboPopup.close()
                                }
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                comboPopup.close()
                                event.accepted = true
                            }
                        }
                    }
                    
                    Text {
                        text: "✕"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        visible: root.filterText !== ""
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                searchField.text = ""
                                searchField.forceActiveFocus()
                            }
                        }
                        
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }
            }
            
            ListView {
                id: filteredList
                Layout.fillWidth: true
                implicitHeight: visible ? Math.min(contentHeight, 260) : 0
                clip: true
                visible: root.searchable
                model: root.filteredModel
                
                delegate: ItemDelegate {
                    id: delegateRoot
                    width: filteredList.width
                    height: root.isModelSelector ? 44 : 34
                    highlighted: index === filteredList.currentIndex
                    
                    background: Rectangle {
                        color: (delegateRoot.highlighted || delegateRoot.hovered) ? Theme.surfaceAlt : "transparent"
                    }
                    
                    contentItem: Loader {
                        anchors.fill: parent
                        sourceComponent: root.isModelSelector ? modelSelectorDelegateContent : defaultFilteredDelegateContent
                        
                        onLoaded: {
                            if (root.isModelSelector) {
                                item.filename = modelData.itemData;
                            } else {
                                item.itemData = modelData.itemData;
                            }
                        }
                    }
                    
                    onClicked: {
                        var originalIdx = modelData.originalIndex
                        root.currentIndex = originalIdx
                        root.activated(originalIdx)
                        comboPopup.close()
                    }
                }
                
                boundsBehavior: Flickable.StopAtBounds
                
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Up) {
                        if (currentIndex <= 0) {
                            searchField.forceActiveFocus()
                            event.accepted = true
                        }
                    } else if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if (currentIndex >= 0 && currentIndex < count) {
                            var originalIdx = model[currentIndex].originalIndex
                            root.currentIndex = originalIdx
                            root.activated(originalIdx)
                            comboPopup.close()
                        }
                        event.accepted = true
                    }
                }
            }
            
            ListView {
                id: normalList
                Layout.fillWidth: true
                implicitHeight: visible ? Math.min(contentHeight, 260) : 0
                clip: true
                visible: !root.searchable
                model: comboPopup.visible ? root.model : null
                currentIndex: root.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds

                delegate: ItemDelegate {
                    id: normalDelegateRoot
                    width: normalList.width
                    height: root.isModelSelector ? 44 : 34
                    highlighted: index === normalList.currentIndex

                    background: Rectangle {
                        color: (normalDelegateRoot.highlighted || normalDelegateRoot.hovered) ? Theme.surfaceAlt : "transparent"
                    }

                    contentItem: Loader {
                        anchors.fill: parent
                        sourceComponent: root.isModelSelector ? modelSelectorDelegateContent : defaultDelegateContent

                        onLoaded: {
                            if (root.isModelSelector) {
                                item.filename = modelData
                            } else {
                                item.itemData = modelData
                            }
                        }
                    }

                    onClicked: {
                        root.currentIndex = index
                        root.activated(index)
                        comboPopup.close()
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

    indicator: LineIcon {
        x: root.width - width - Theme.paddingMedium
        anchors.verticalCenter: parent.verticalCenter
        name: "arrow-down"
        color: Theme.textSecondary
        width: 14
        height: 14
    }

    Component {
        id: modelSelectorDelegateContent
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            spacing: Theme.paddingSmall

            property string filename: ""
            property var variantInfo: root.getVariantInfo(filename)
            
            Rectangle {
                visible: filename.toLowerCase().endsWith(".gguf")
                implicitWidth: 38
                implicitHeight: 18
                radius: 4
                color: "#1e88e5"
                Layout.alignment: Qt.AlignVCenter
                
                Text {
                    anchors.centerIn: parent
                    text: "GGUF"
                    color: "#ffffff"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
            
            Text {
                text: variantInfo && variantInfo.speaker ? variantInfo.speaker : root.getCleanedName(filename)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            
            // Language column
            Item {
                visible: variantInfo && variantInfo.language !== undefined && variantInfo.language !== ""
                Layout.preferredWidth: visible ? 110 : 0
                Layout.preferredHeight: 22
                Layout.alignment: Qt.AlignVCenter
                
                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)
                    border.width: 1
                    
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        
                        LineIcon {
                            name: "globe"
                            color: Theme.accent
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                        }
                        
                        Text {
                            text: variantInfo ? variantInfo.language : ""
                            color: Theme.accent
                            font.pixelSize: 10
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.maximumWidth: 80
                        }
                    }
                }
            }

            Item {
                visible: root.getQuantization(filename) !== ""
                Layout.preferredWidth: visible ? 65 : 0
                Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignVCenter
                
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    border.width: 1
                    
                    Text {
                        id: quantText
                        anchors.centerIn: parent
                        text: root.getQuantization(filename)
                        color: Theme.textSecondary
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }
            
            // Suitability column
            Item {
                Layout.preferredWidth: root.isModelSuitable(filename, root.modelFamily, root.modelRequirement) ? 24 : 100
                Layout.preferredHeight: 22
                Layout.alignment: Qt.AlignVCenter
                
                Loader {
                    anchors.centerIn: parent
                    property bool suitable: root.isModelSuitable(filename, root.modelFamily, root.modelRequirement)
                    sourceComponent: suitable ? suitableComponent : tooLargeComponent
                    
                    Component {
                        id: suitableComponent
                        Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            radius: 5
                            color: "transparent"
                            border.color: Theme.accent
                            border.width: 1.5
                            
                            LineIcon {
                                anchors.centerIn: parent
                                name: "check"
                                color: Theme.accent
                                width: 10
                                height: 10
                            }
                        }
                    }
                    
                    Component {
                        id: tooLargeComponent
                        Rectangle {
                            implicitWidth: tooLargeLayout.implicitWidth + 12
                            implicitHeight: 22
                            radius: 6
                            color: Qt.rgba(0.93, 0.33, 0.36, 0.12)
                            border.color: Qt.rgba(0.93, 0.33, 0.36, 0.5)
                            border.width: 1
                            
                            RowLayout {
                                id: tooLargeLayout
                                anchors.centerIn: parent
                                spacing: 4
                                
                                LineIcon {
                                    name: "close"
                                    color: Theme.danger
                                    Layout.preferredWidth: 10
                                    Layout.preferredHeight: 10
                                }
                                Text {
                                    text: qsTr("Likely too large")
                                    color: Theme.danger
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }

            // Downloaded column
            Item {
                visible: root.isDownloaded(filename)
                Layout.preferredWidth: visible ? 85 : 0
                Layout.preferredHeight: 22
                Layout.alignment: Qt.AlignVCenter
                
                Loader {
                    anchors.centerIn: parent
                    sourceComponent: downloadedComponent
                    
                    Component {
                        id: downloadedComponent
                        Rectangle {
                            implicitWidth: downloadedLayout.implicitWidth + 12
                            implicitHeight: 22
                            radius: 6
                            color: Qt.rgba(0.40, 0.73, 0.42, 0.10)
                            border.color: Qt.rgba(0.40, 0.73, 0.42, 0.5)
                            border.width: 1
                            
                            RowLayout {
                                id: downloadedLayout
                                anchors.centerIn: parent
                                spacing: 4
                                
                                Text {
                                    text: qsTr("Downloaded")
                                    color: Theme.success
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
            
            Text {
                text: root.estimateSize(filename, root.defaultFile, root.defaultSize)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.preferredWidth: 70
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Component {
        id: defaultDelegateContent
        Column {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            
            property var itemData: null

            Text {
                text: root.primaryTextFor(itemData)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                visible: root.secondaryTextFor(itemData) !== ""
                text: root.secondaryTextFor(itemData)
                color: Theme.textSecondary
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                width: parent.width
            }
        }
    }

    Component {
        id: defaultFilteredDelegateContent
        Column {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            
            property var itemData: null

            Text {
                text: root.primaryTextFor(itemData)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                visible: root.secondaryTextFor(itemData) !== ""
                text: root.secondaryTextFor(itemData)
                color: Theme.textSecondary
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                width: parent.width
            }
        }
    }
}

