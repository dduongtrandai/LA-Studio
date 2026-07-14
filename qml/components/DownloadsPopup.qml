import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Popup {
    id: root

    width: 420
    height: 520
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1

        // Drop shadow effect using nested rectangles
        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            color: "#00000025"
            radius: Theme.radiusMedium + 8
            z: -1
        }
    }

    padding: 0

    readonly property var allItems: AppController.downloads.allDownloads

    function getDisplayName(item) {
        if (item.metadata && item.metadata.engineName) {
            var name = item.metadata.engineName;
            var ver = item.metadata.version ? " (" + item.metadata.version + ")" : "";
            return name + ver;
        }
        return item.filename;
    }

    function isRuntimeItem(item) {
        if (item.localPath) {
            return item.localPath.indexOf("backends") !== -1;
        }
        if (item.metadata && item.metadata.kind === "runtimeDependency") {
            return true;
        }
        if (item.metadata && item.metadata.engineName) {
            return true;
        }
        return false;
    }

    function formatBytes(bytes) {
        if (bytes <= 0) return "0.00 MB";
        if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(2) + " GB";
        return (bytes / 1048576).toFixed(2) + " MB";
    }

    // Filter items based on the search query
    readonly property var filteredItems: {
        var txt = filterInput.text.toLowerCase().trim();
        if (txt === "") return allItems;
        return allItems.filter(function(item) {
            var dispName = getDisplayName(item).toLowerCase();
            var fileName = item.filename.toLowerCase();
            return dispName.indexOf(txt) !== -1 || fileName.indexOf(txt) !== -1;
        });
    }

    readonly property var activeItems: filteredItems.filter(function(item) { return !item.done; })
    readonly property var completedItems: filteredItems.filter(function(item) { return item.done; })

    AppContextMenu {
        id: itemMenu
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Dialog Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Downloads")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }

                // Close Button
                Button {
                    id: closeBtn
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: root.close()
                    contentItem: LineIcon {
                        name: "close"
                        color: closeBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                    }
                    background: Rectangle {
                        radius: 6
                        color: closeBtn.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        // Search/Filter box
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingLarge
            Layout.bottomMargin: Theme.paddingSmall
            color: "transparent"

            TextField {
                id: filterInput
                anchors.fill: parent
                placeholderText: qsTr("Filter downloads...")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textSecondary
                font.pixelSize: Theme.fontMedium
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.paddingMedium
                rightPadding: Theme.paddingMedium
                background: Rectangle {
                    color: Theme.background
                    radius: Theme.radiusSmall
                    border.color: filterInput.activeFocus ? Theme.accent : Theme.surfaceAlt
                    border.width: 1
                }
            }
        }

        // Scrollable Downloads list
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: filteredItems.length > 0
            clip: true
            leftPadding: Theme.paddingLarge
            rightPadding: Theme.paddingLarge

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.paddingMedium

                // Active Downloads Section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    visible: activeItems.length > 0

                    Text {
                        text: qsTr("Downloading")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        Layout.fillWidth: true
                        Layout.bottomMargin: 4
                    }

                    Repeater {
                        model: activeItems
                        delegate: Rectangle {
                            id: activeRow
                            Layout.fillWidth: true
                            height: 64
                            color: activeHover.hovered ? Qt.rgba(1, 1, 1, 0.02) : "transparent"
                            radius: Theme.radiusSmall

                            HoverHandler { id: activeHover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingSmall
                                spacing: Theme.paddingMedium

                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 6
                                    color: Theme.surfaceAlt
                                    border.color: Qt.rgba(1, 1, 1, 0.04)

                                    LineIcon {
                                        anchors.centerIn: parent
                                        name: isRuntimeItem(modelData) ? "cpu" : "file"
                                        color: Theme.accentLight
                                        width: 18
                                        height: 18
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: getDisplayName(modelData)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: {
                                                if (modelData.bytesTotal <= 0) return qsTr("Starting...");
                                                var pct = ((modelData.bytesReceived / modelData.bytesTotal) * 100).toFixed(1);
                                                return qsTr("Download • %1 of %2 (%3%)").arg(formatBytes(modelData.bytesReceived)).arg(formatBytes(modelData.bytesTotal)).arg(pct);
                                            }
                                            color: Theme.textSecondary
                                            font.pixelSize: 11
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 4
                                        radius: 2
                                        color: Theme.background

                                        Rectangle {
                                            width: parent.width * (modelData.bytesTotal > 0 ? modelData.bytesReceived / modelData.bytesTotal : 0)
                                            height: parent.height
                                            radius: 2
                                            color: Theme.accent
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Completed Downloads Section
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    visible: completedItems.length > 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 4

                        Text {
                            text: qsTr("Completed")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Button {
                            id: clearBtn
                            visible: completedItems.length > 0
                            flat: true
                            implicitWidth: 46
                            implicitHeight: 22
                            contentItem: Text {
                                text: qsTr("Clear")
                                color: clearBtn.hovered ? Theme.accentLight : Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                horizontalAlignment: Text.AlignRight
                            }
                            background: Item {}
                            onClicked: AppController.downloads.clearCompleted()
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                    }

                    Repeater {
                        model: completedItems
                        delegate: Rectangle {
                            id: completedRow
                            Layout.fillWidth: true
                            height: 56
                            color: completedHover.hovered ? Qt.rgba(1, 1, 1, 0.02) : "transparent"
                            radius: Theme.radiusSmall

                            HoverHandler { id: completedHover }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.paddingSmall
                                anchors.rightMargin: Theme.paddingSmall
                                spacing: Theme.paddingMedium

                                Rectangle {
                                    width: 32
                                    height: 32
                                    radius: 6
                                    color: Theme.surfaceAlt
                                    border.color: Qt.rgba(1, 1, 1, 0.04)

                                    LineIcon {
                                        anchors.centerIn: parent
                                        name: isRuntimeItem(modelData) ? "cpu" : "file"
                                        color: modelData.status === "failed" ? Theme.danger : Theme.textSecondary
                                        width: 18
                                        height: 18
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: getDisplayName(modelData)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontMedium
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: {
                                            if (modelData.status === "failed") {
                                                return qsTr("Failed • %1").arg(modelData.errorMsg || qsTr("Unknown error"));
                                            }
                                            return qsTr("Download • %1").arg(formatBytes(modelData.bytesTotal));
                                        }
                                        color: modelData.status === "failed" ? Theme.danger : Theme.textSecondary
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                // Open folder button
                                Button {
                                    id: folderBtn
                                    visible: modelData.status === "completed" && !!modelData.localPath
                                    implicitWidth: 30
                                    implicitHeight: 30
                                    onClicked: {
                                        if (modelData.localPath) {
                                            var path = modelData.localPath;
                                            path = path.replace(/\\/g, "/");
                                            var folder = path.substring(0, path.lastIndexOf('/'));
                                            Qt.openUrlExternally("file:///" + folder);
                                        }
                                    }
                                    contentItem: LineIcon {
                                        name: "folder"
                                        color: folderBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: folderBtn.hovered ? Qt.rgba(1, 1, 1, 0.045) : "transparent"
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }

                                // Actions (Three-dot) Menu button
                                Button {
                                    id: actionBtn
                                    implicitWidth: 30
                                    implicitHeight: 30
                                    onClicked: {
                                        var path = modelData.localPath;
                                        if (path) {
                                            path = path.replace(/\\/g, "/");
                                        }

                                        itemMenu.items = [
                                            {
                                                text: qsTr("Clear"),
                                                iconName: "trash",
                                                action: function() {
                                                    AppController.downloads.removeDownload(modelData.identifier, modelData.filename);
                                                }
                                            },
                                            {
                                                text: qsTr("Copy local path"),
                                                iconName: "copy",
                                                enabled: !!path,
                                                action: function() {
                                                    AppController.copyToClipboard(path);
                                                }
                                            }
                                        ];
                                        itemMenu.popup(actionBtn);
                                    }
                                    contentItem: LineIcon {
                                        name: "more-vertical"
                                        color: actionBtn.hovered ? Theme.textPrimary : Theme.textSecondary
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: actionBtn.hovered ? Qt.rgba(1, 1, 1, 0.045) : "transparent"
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }

                }
            }
        }

        // Empty State (No items / No search results)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: filteredItems.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.paddingSmall

                LineIcon {
                    name: "download"
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                    width: 28
                    height: 28
                    opacity: 0.5
                }

                Text {
                    text: allItems.length === 0 ? qsTr("No downloads yet") : qsTr("No downloads found")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
        }

        // Dialog Footer (Link to downloads folder)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                spacing: Theme.paddingSmall

                Item { Layout.fillWidth: true }

                Button {
                    id: openFolderLink
                    flat: true
                    implicitHeight: 32
                    onClicked: {
                        var path = AppController.models.modelsRoot;
                        path = path.replace(/\\/g, "/");
                        Qt.openUrlExternally("file:///" + path);
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: qsTr("Open downloads directory")
                            color: openFolderLink.hovered ? Theme.accentLight : Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.underline: openFolderLink.hovered
                        }
                        LineIcon {
                            name: "external-link"
                            color: openFolderLink.hovered ? Theme.accentLight : Theme.textSecondary
                            width: 12
                            height: 12
                        }
                    }
                    background: Item {}
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }
    }

    component EmptyState: Rectangle {
        id: emptyState
        property string text: ""
        width: parent ? parent.width : 0
        height: 52
        color: "transparent"
        Text {
            anchors.centerIn: parent
            text: emptyState.text
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
    }
}
