import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Dialog {
    id: root

    property string engineId: ""
    property string engineName: "Runtime"
    property string engineFamily: ""
    property string assetName: ""
    property string engineType: "tts"
    property string sourceUrl: ""
    property string defaultVersion: ""
    property var availableVersions: []
    property color accentColor: Theme.accent

    signal versionSelected(string runtimeId, string version)

    width: Math.min(620, parent ? parent.width - 48 : 620)
    height: Math.min(520, parent ? parent.height - 48 : 520)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: 0
    title: ""

    function releaseBaseUrl() {
        if (sourceUrl !== "") return sourceUrl
        if (engineId.indexOf("vibevoice") !== -1)
            return "https://github.com/dduongtrandai/vibevoice.cpp/releases/download/"
        if (engineId.indexOf("omnivoice") !== -1)
            return "https://github.com/dduongtrandai/omnivoice.cpp/releases/download/"
        if (engineId.indexOf("vieneu-tts") !== -1)
            return "https://github.com/dduongtrandai/VieNeu-TTS.cpp/releases/download/"
        if (engineId.indexOf("crispasr") !== -1)
            return "https://github.com/CrispStrobe/CrispASR/releases/download/"
        return "https://github.com/ggerganov/whisper.cpp/releases/download/"
    }

    function knownVersions() {
        if (availableVersions && availableVersions.length > 0) {
            var catalogVersions = []
            for (var i = 0; i < availableVersions.length; i++) {
                var value = availableVersions[i]
                var version = typeof value === "string" ? value : (value.version || "")
                if (version !== "") catalogVersions.push({ version: version })
            }
            if (catalogVersions.length > 0) return catalogVersions
        }
        if (engineId.indexOf("vibevoice") !== -1) return [
            { version: "v0.1.0" }
        ]
        if (engineId.indexOf("omnivoice") !== -1) return [
            { version: "v0.1.2" }
        ]
        if (engineId.indexOf("vieneu-tts") !== -1) return [
            { version: "v0.1.3" }
        ]
        if (engineId.indexOf("crispasr") !== -1) return [
            { version: "v0.8.6" }
        ]
        if (engineId.indexOf("whisper") !== -1) return [
            { version: "v1.8.4" },
            { version: "v1.8.3" }
        ]
        return []
    }

    function downloadableVersions() {
        var versions = knownVersions()
        var out = []
        for (var i = 0; i < versions.length; i++) {
            if (!hasInstalledVersion(versions[i].version)) out.push(versions[i])
        }
        return out
    }

    function installedVersions() {
        if (engineId === "") return []
        var _deps = AppController.runtimes.allRuntimes
        return AppController.runtimes.runtimeVersions(engineId)
    }

    function hasInstalledVersion(version) {
        var installed = installedVersions()
        for (var i = 0; i < installed.length; i++) {
            if (installed[i].version === version) return true
        }
        return false
    }

    function activeDownload(version) {
        var downloads = AppController.downloads.activeDownloads
        var asset = assetName !== "" ? assetName : fallbackAsset()
        for (var i = 0; i < downloads.length; i++) {
            if (downloads[i].filename !== asset) continue
            if (downloads[i].metadata) {
                if (downloads[i].metadata.id !== engineId || downloads[i].metadata.version !== version) continue
            }
            return downloads[i]
        }
        return null
    }

    function downloadText(version) {
        var item = activeDownload(version)
        if (!item) return qsTr("Download")
        if (item.bytesTotal <= 0) return qsTr("Starting")
        return Math.round((item.bytesReceived / item.bytesTotal) * 100) + "%"
    }

    function fallbackAsset() {
        if (engineId.indexOf("vibevoice") !== -1) return "vibevoice-win-cpu.zip"
        if (engineId.indexOf("omnivoice") !== -1) return "omnivoice-win-cpu.zip"
        if (engineId.indexOf("vieneu-tts") !== -1) return "vieneu-tts-win-cpu.zip"
        if (engineId.indexOf("crispasr") !== -1) return "libcrispasr-windows-x86_64.tar.gz"
        if (engineId.indexOf("whisper") !== -1) return "whisper-win-cpu.zip"
        return "whisper-bin-x64.zip"
    }

    function enqueueVersion(version) {
        var asset = assetName !== "" ? assetName : fallbackAsset()
        AppController.downloads.enqueueUrl(releaseBaseUrl() + version + "/" + asset, asset, AppController.runtimes.backendsPath, {
            "id": engineId,
            "version": version,
            "engineName": engineName,
            "engineFamily": root.engineFamily,
            "type": engineType,
            "metadata": {
                "runtimeVersion": version,
                "asset": asset,
                "source": releaseBaseUrl() + version + "/" + asset
            }
        })
    }

    function selectVersion(version) {
        if (engineType === "tts") {
            AppController.settings.selectedTtsRuntime = engineId
            AppController.settings.selectedTtsRuntimeVersion = version
        } else if (engineType === "stt") {
            AppController.settings.selectedSttRuntime = engineId
            AppController.settings.selectedSttRuntimeVersion = version
        } else {
            AppController.settings.selectedRuntime = engineId
        }
        root.versionSelected(engineId, version)
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Version Manager")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.engineName
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                }

                Button {
                    id: closeButton
                    implicitWidth: 34
                    implicitHeight: 34
                    onClicked: root.close()
                    contentItem: LineIcon {
                        name: "close"
                        color: closeButton.hovered ? Theme.textPrimary : Theme.textSecondary
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }
                    background: Rectangle {
                        radius: 7
                        color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 0

                SectionHeader { title: qsTr("Installed versions") }

                Repeater {
                    model: root.installedVersions()
                    delegate: VersionRow {
                        width: parent.width
                        version: modelData.version
                        detail: modelData.directory
                        installed: true
                        selected: (root.engineType === "tts" && AppController.settings.selectedTtsRuntime === root.engineId && AppController.settings.selectedTtsRuntimeVersion === modelData.version) ||
                                  (root.engineType === "stt" && AppController.settings.selectedSttRuntime === root.engineId && AppController.settings.selectedSttRuntimeVersion === modelData.version)
                        accentColor: root.accentColor
                        onSelectRequested: function(version) { root.selectVersion(version) }
                        onRemoveRequested: function(version) {
                            AppController.runtimes.removeRuntimeVersion(root.engineId, version)
                            if (root.engineType === "tts" && AppController.settings.selectedTtsRuntimeVersion === version) {
                                AppController.settings.selectedTtsRuntimeVersion = ""
                            } else if (root.engineType === "stt" && AppController.settings.selectedSttRuntimeVersion === version) {
                                AppController.settings.selectedSttRuntimeVersion = ""
                            }
                        }
                    }
                }

                EmptyState {
                    visible: root.installedVersions().length === 0
                    text: qsTr("No installed versions")
                }

                SectionHeader { title: qsTr("Available downloads") }

                Repeater {
                    model: root.downloadableVersions()
                    delegate: VersionRow {
                        width: parent.width
                        version: modelData.version
                        detail: root.releaseBaseUrl() + modelData.version
                        installed: false
                        selected: false
                        accentColor: root.accentColor
                        downloadLabel: root.downloadText(modelData.version)
                        downloading: root.activeDownload(modelData.version) !== null
                        onDownloadRequested: function(version) { root.enqueueVersion(version) }
                    }
                }

                EmptyState {
                    visible: root.downloadableVersions().length === 0
                    text: qsTr("No additional versions available")
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surfaceAlt }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingLarge
            spacing: Theme.paddingMedium

            Text {
                Layout.fillWidth: true
                text: qsTr("%1 installed").arg(root.installedVersions().length)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }

            PrimaryButton {
                text: qsTr("Done")
                implicitWidth: 104
                implicitHeight: 36
                onClicked: root.close()
            }
        }
    }

    component SectionHeader: Rectangle {
        id: sectionHeader
        property string title: ""
        width: parent ? parent.width : 0
        height: 40
        color: Theme.background
        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.paddingLarge
            anchors.verticalCenter: parent.verticalCenter
            text: sectionHeader.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }
    }

    component EmptyState: Rectangle {
        id: emptyState
        property string text: ""
        Layout.fillWidth: true
        implicitHeight: 52
        color: "transparent"
        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingLarge
            anchors.rightMargin: Theme.paddingLarge
            text: emptyState.text
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component VersionRow: Rectangle {
        id: row

        property string version: ""
        property string detail: ""
        property bool installed: false
        property bool selected: false
        property bool downloading: false
        property string downloadLabel: "Download"
        property color accentColor: Theme.accent

        signal selectRequested(string version)
        signal removeRequested(string version)
        signal downloadRequested(string version)

        height: 58
        color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.035) : "transparent"

        HoverHandler { id: rowHover }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingLarge
            anchors.rightMargin: Theme.paddingLarge
            spacing: Theme.paddingMedium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: row.version
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: row.detail
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }

            PrimaryButton {
                visible: row.installed
                text: row.selected ? qsTr("Selected") : qsTr("Use")
                iconName: row.selected ? "check" : ""
                implicitWidth: row.selected ? 122 : 74
                implicitHeight: 30
                quiet: row.selected
                buttonColor: row.accentColor
                textColor: row.selected ? Theme.success : "#ffffff"
                borderColor: row.selected ? Qt.rgba(0.40, 0.73, 0.42, 0.24) : Qt.rgba(1, 1, 1, 0.10)
                onClicked: {
                    if (!row.selected) row.selectRequested(row.version)
                }
            }

            Button {
                id: removeButton
                visible: row.installed
                implicitWidth: 30
                implicitHeight: 30
                onClicked: row.removeRequested(row.version)
                contentItem: LineIcon {
                    name: "trash"
                    color: removeButton.hovered ? Theme.danger : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 15
                    height: 15
                }
                background: Rectangle {
                    radius: 7
                    color: removeButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }

            PrimaryButton {
                visible: !row.installed
                text: row.downloadLabel
                iconName: row.downloading ? "" : "download"
                implicitWidth: 112
                implicitHeight: 30
                enabled: true
                quiet: row.downloading
                onClicked: {
                    if (row.downloading) {
                        Theme.requestShowDownloads()
                    } else {
                        row.downloadRequested(row.version)
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.surfaceAlt
        }
    }
}
