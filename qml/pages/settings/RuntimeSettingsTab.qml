import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../../components"
import "../../components/base"
import "../../components/shared/settings"

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    // Supported Whisper Runtime Versions list
    property var whisperRuntimeVersions: [
        { version: "v1.8.4", url: "https://github.com/ggerganov/whisper.cpp/releases/tag/v1.8.4" },
        { version: "v1.8.3", url: "https://github.com/ggerganov/whisper.cpp/releases/tag/v1.8.3" }
    ]

    property var omnivoiceRuntimeVersions: [
        { version: "v0.1.2", url: "https://github.com/dduongtrandai/omnivoice.cpp/releases/tag/v0.1.2" }
    ]

    property var vibevoiceRuntimeVersions: [
        { version: "v0.1.0", url: "https://github.com/dduongtrandai/vibevoice.cpp/releases/tag/v0.1.0" }
    ]

    property var engineGroups: [
        {
            id: "whisper",
            name: "Whisper.cpp",
            desc: "Speech-to-Text inference engine based on OpenAI's Whisper model",
            type: "stt",
            items: [
                { id: "whisper-cpu", name: "CPU whisper.cpp (x64)", desc: "Standard CPU-only engine", status: "latest", asset: "whisper-bin-x64.zip", type: "stt" },
                { id: "whisper-blas", name: "CPU BLAS whisper.cpp (x64)", desc: "CPU engine with BLAS acceleration", status: "download", asset: "whisper-blas-bin-x64.zip", type: "stt" },
                { id: "whisper-cuda11", name: "CUDA 11 whisper.cpp (x64)", desc: "CUDA 11.8.0 accelerated engine (NVIDIA)", status: "download", asset: "whisper-cublas-11.8.0-bin-x64.zip", type: "stt" },
                { id: "whisper-cuda12", name: "CUDA 12 whisper.cpp (x64)", desc: "CUDA 12.4.0 accelerated engine (NVIDIA)", status: "download", asset: "whisper-cublas-12.4.0-bin-x64.zip", type: "stt" }
            ]
        },
        {
            id: "omnivoice",
            name: "OmniVoice",
            desc: "Text-to-Speech & Voice Cloning engine based on omnivoice.cpp",
            type: "tts",
            items: [
                { id: "omnivoice.cpp-win-x86_64-cpu", name: "CPU Omnivoice (x64)", desc: "Standard CPU-only engine", status: "download", asset: "omnivoice-win-cpu.zip", type: "tts" },
                { id: "omnivoice.cpp-win-x86_64-cuda-11", name: "CUDA 11 Omnivoice (x64)", desc: "CUDA 11.8 accelerated engine", status: "download", asset: "omnivoice-win-cuda-11.zip", type: "tts" },
                { id: "omnivoice.cpp-win-x86_64-cuda-12", name: "CUDA 12 Omnivoice (x64)", desc: "CUDA 12.4 accelerated engine", status: "download", asset: "omnivoice-win-cuda-12.zip", type: "tts" },
                { id: "omnivoice.cpp-win-x86_64-vulkan", name: "Vulkan Omnivoice (x64)", desc: "Vulkan accelerated engine", status: "download", asset: "omnivoice-win-vulkan.zip", type: "tts" }
            ]
        },
        {
            id: "vibevoice",
            name: "VibeVoice",
            desc: "Text-to-Speech & Voice Cloning engine based on vibevoice.cpp",
            type: "tts",
            items: [
                { id: "vibevoice.cpp-win-x86_64-cpu", name: "CPU VibeVoice (x64)", desc: "Standard CPU-only engine", status: "download", asset: "vibevoice-win-cpu.zip", type: "tts" },
                { id: "vibevoice.cpp-win-x86_64-cuda-12", name: "CUDA 12 VibeVoice (x64)", desc: "CUDA 12.4 accelerated engine", status: "download", asset: "vibevoice-win-cuda-12.zip", type: "tts" },
                { id: "vibevoice.cpp-win-x86_64-vulkan", name: "Vulkan VibeVoice (x64)", desc: "Vulkan accelerated engine", status: "download", asset: "vibevoice-win-vulkan.zip", type: "tts" }
            ]
        }
    ]

    // Reactive mirror of the C++ allRuntimes list.
    property var installedRuntimes: AppController.runtimes.allRuntimes

    function isRuntimeDownloaded(id) {
        if (!id) return false;
        var rts = installedRuntimes;
        for (var i = 0; i < rts.length; i++) {
            if (rts[i].id === id) return true;
        }
        return false;
    }

    function getRuntimeVersion(id) {
        if (!id) return "";
        var rts = installedRuntimes;
        for (var i = 0; i < rts.length; i++) {
            if (rts[i].id === id) return rts[i].version;
        }
        return "";
    }

    ColumnLayout {
        width: parent.width - Theme.paddingMedium * 2
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.paddingLarge

        Item { Layout.preferredHeight: Theme.paddingSmall }

        // Active downloads section
        Repeater {
            model: AppController.downloads.activeDownloads
            DownloadProgressBar {
                Layout.fillWidth: true
                modelId: modelData.identifier
                filename: modelData.filename
                bytesReceived: modelData.bytesReceived
                bytesTotal: modelData.bytesTotal
            }
        }

        // Default runtime selection card
        SettingsCard {
            title: qsTr("Default Model Engine")
            description: qsTr("Manage default execution engines for Whisper speech processing and audio cloning.")
            iconName: "spark"

            SettingRow {
                label: qsTr("Speech-to-Text Runtime")
                description: qsTr("Select active Whisper backend runtime. Downloaded engines will appear in the list.")

                AppComboBox {
                    id: sttRuntimeCombo
                    model: root.installedRuntimes
                    textRole: "name"
                    secondaryTextRole: "version"
                    valueRole: "id"

                    Binding {
                        target: sttRuntimeCombo
                        property: "currentIndex"
                        value: sttRuntimeCombo.findRuntimeIndex(AppController.settings.selectedRuntime)
                    }

                    function findRuntimeIndex(runtimeId) {
                        var runtimes = root.installedRuntimes;
                        if (!runtimeId || runtimeId === "" || !runtimes || runtimes.length === 0) return -1;
                        for (var i = 0; i < runtimes.length; i++) {
                            if (String(runtimes[i].id) === String(runtimeId)) return i;
                        }
                        return -1;
                    }

                    onActivated: {
                        if (currentValue && currentValue !== "") {
                            AppController.settings.selectedRuntime = String(currentValue);
                        }
                    }

                    Layout.fillWidth: true
                    Layout.maximumWidth: 400
                }
            }
        }

        // Engines & Frameworks card
        SettingsCard {
            title: qsTr("Model Acceleration Engines")
            description: qsTr("Download and register local neural network engine acceleration runtimes.")
            iconName: "sliders"

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium
                Layout.bottomMargin: Theme.paddingSmall

                // Custom search input
                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Filter engines by name...")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    leftPadding: Theme.paddingMedium
                    rightPadding: Theme.paddingMedium
                    implicitHeight: 38

                    background: Rectangle {
                        color: Qt.rgba(0, 0, 0, 0.15)
                        radius: Theme.radiusSmall
                        border.color: searchField.activeFocus ? Theme.accent : Qt.rgba(255, 255, 255, 0.08)
                        border.width: 1
                    }
                }
                
                AppComboBox {
                    id: typeFilterCombo
                    model: [qsTr("Compatible only"), qsTr("All types")]
                    implicitWidth: 160
                }
            }

            // Engine items grouped
            Repeater {
                model: root.engineGroups

                delegate: ColumnLayout {
                    id: groupDelegate
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 0

                    property bool expanded: index === 0 // Expand first by default

                    // Group Header Row
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 52
                        color: groupHeaderMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.03) : "transparent"
                        radius: Theme.radiusSmall

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingMedium
                            anchors.rightMargin: Theme.paddingMedium
                            spacing: Theme.paddingMedium

                            // Rotating Chevron Indicator
                            LineIcon {
                                name: "chevron-right"
                                color: Theme.accentLight
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                rotation: groupDelegate.expanded ? 90 : 0
                                
                                Behavior on rotation {
                                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    text: qsTr("%1 Engine").arg(groupDelegate.modelData.name)
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    font.bold: true
                                }
                                Text {
                                    text: groupDelegate.modelData.desc
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }
                            }
                            
                            // Capability Badge (STT / TTS)
                            Rectangle {
                                color: Qt.rgba(124, 77, 255, 0.1)
                                border.color: Qt.rgba(124, 77, 255, 0.25)
                                border.width: 1
                                radius: 4
                                implicitWidth: typeBadgeText.implicitWidth + 12
                                implicitHeight: 20
                                
                                Text {
                                    id: typeBadgeText
                                    anchors.centerIn: parent
                                    text: groupDelegate.modelData.type.toUpperCase()
                                    color: Theme.accentLight
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        MouseArea {
                            id: groupHeaderMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                groupDelegate.expanded = !groupDelegate.expanded
                            }
                        }
                    }

                    // Group Items Sub-list
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.paddingXL
                        spacing: 0
                        visible: groupDelegate.expanded

                        Repeater {
                            model: groupDelegate.modelData.items
                            delegate: Rectangle {
                                id: itemDelegate
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: 56
                                color: "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.paddingSmall
                                    anchors.rightMargin: Theme.paddingSmall
                                    spacing: Theme.paddingMedium

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            text: itemDelegate.modelData.name
                                            color: Theme.textPrimary
                                            font.pixelSize: Theme.fontMedium
                                        }
                                        Text {
                                            text: itemDelegate.modelData.desc
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontSmall
                                        }
                                    }

                                    // Action Badge / Trigger
                                    Item {
                                        id: statusCol
                                        Layout.preferredWidth: 120
                                        Layout.fillHeight: true
                                        
                                        property bool isDownloaded: {
                                            var _deps = root.installedRuntimes;
                                            return root.isRuntimeDownloaded(itemDelegate.modelData.id);
                                        }

                                        // Status Pill Badge (If installed)
                                        Rectangle {
                                            visible: statusCol.isDownloaded || itemDelegate.modelData.status === "latest"
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.right: parent.right
                                            height: 22
                                            width: statusPillText.implicitWidth + 16
                                            color: Qt.rgba(102, 187, 106, 0.12)
                                            border.color: Qt.rgba(102, 187, 106, 0.28)
                                            border.width: 1
                                            radius: 11

                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Text { text: "✓"; color: Theme.success; font.bold: true; font.pixelSize: 10 }
                                                Text {
                                                    id: statusPillText
                                                    text: root.getRuntimeVersion(itemDelegate.modelData.id) || qsTr("Latest")
                                                    color: Theme.success
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                        }

                                        PrimaryButton {
                                            visible: !statusCol.isDownloaded && itemDelegate.modelData.status !== "latest"
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.right: parent.right
                                             text: qsTr("Download")
                                             implicitWidth: 90
                                             implicitHeight: 28
                                            onClicked: {
                                                manageVersionsDialog.engineId = itemDelegate.modelData.id;
                                                manageVersionsDialog.engineName = itemDelegate.modelData.name;
                                                manageVersionsDialog.assetName = itemDelegate.modelData.asset;
                                                manageVersionsDialog.engineType = itemDelegate.modelData.type;
                                                manageVersionsDialog.open();
                                            }
                                        }
                                    }

                                    // Details Trigger Column
                                    Item {
                                        Layout.preferredWidth: 32
                                        Layout.fillHeight: true
                                        
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 24
                                            height: 24
                                            radius: 12
                                            color: optionHover.hovered ? Qt.rgba(255, 255, 255, 0.05) : "transparent"
                                            
                                            LineIcon {
                                                anchors.centerIn: parent
                                                name: "more-horizontal"
                                                color: Theme.textSecondary
                                                width: 12
                                                height: 12
                                            }

                                            HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }

                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: {
                                                    engineMenu.currentId = itemDelegate.modelData.id;
                                                    engineMenu.currentEngine = itemDelegate.modelData.name;
                                                    engineMenu.currentAsset = itemDelegate.modelData.asset;
                                                    engineMenu.currentType = itemDelegate.modelData.type;
                                                    engineMenu.popup(this, 0, height);
                                                }
                                            }
                                        }
                                    }
                                }

                                // Separator Line
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: Theme.surfaceAlt
                                    opacity: 0.2
                                    anchors.bottom: parent.bottom
                                    visible: itemDelegate.index < (groupDelegate.modelData.items.length - 1)
                                }
                            }
                        }
                    }

                    // Divider between groups
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.surfaceAlt
                        opacity: 0.3
                        Layout.topMargin: Theme.paddingMedium
                        Layout.bottomMargin: Theme.paddingMedium
                        visible: groupDelegate.index < (root.engineGroups.length - 1)
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    AppContextMenu {
        id: engineMenu
        property string currentId: ""
        property string currentEngine: "Whisper.cpp Engine"
        property string currentAsset: ""
        property string currentType: "stt"

        items: [
            { text: qsTr("Installed version details"), subText: qsTr("View installed version information"), iconName: "file", action: function() { /* implement if needed */ } },
            { type: "separator" },
            { text: qsTr("Manage versions"), subText: qsTr("Install, remove or select runtime versions"), iconName: "settings", action: function() {
                    manageVersionsDialog.engineId = engineMenu.currentId;
                    manageVersionsDialog.engineName = engineMenu.currentEngine;
                    manageVersionsDialog.assetName = engineMenu.currentAsset;
                    manageVersionsDialog.engineType = engineMenu.currentType;
                    manageVersionsDialog.open();
                } }
        ]
    }

    RuntimeVersionManagerDialog {
        id: manageVersionsDialog
    }
}
