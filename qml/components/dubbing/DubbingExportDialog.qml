pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import ".."
import "../base"

Dialog {
    id: root

    property string projectName: ""
    property bool videoSource: false
    property bool busy: false
    property int segmentCount: 0
    property int generatedClipCount: 0
    property int selectedTab: 0
    property bool includeDubbedSubtitles: true
    property string subtitleFormat: "srt"
    property string sourceLanguageCode: ""
    property string sourceLanguageName: ""
    property string targetLanguageCode: ""
    property string targetLanguageName: ""

    signal videoExportRequested()
    signal audioExportRequested(string stem)
    signal subtitleExportRequested(string format, bool useTargetText, string languageCode)
    signal packageExportRequested()

    function compactProjectName(path) {
        if (path === "") return qsTr("Dubbing project")
        var parts = path.replace(/\\/g, "/").split("/")
        return parts[parts.length - 1].replace(/\.ladub\.json$/i, "")
    }

    width: Math.min(900, parent ? parent.width - Theme.paddingXL * 2 : 900)
    height: Math.min(520, parent ? parent.height - Theme.paddingXL * 2 : 520)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: 0
    title: ""
    closePolicy: Popup.CloseOnEscape

    onOpened: selectedTab = videoSource ? 0 : 1

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            radius: Theme.radiusMedium + 8
            color: Qt.rgba(0, 0, 0, 0.28)
            z: -1
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingMedium
            Layout.topMargin: Theme.paddingMedium
            Layout.bottomMargin: Theme.paddingMedium
            spacing: Theme.paddingSmall

            LineIcon {
                name: "download"
                color: Theme.accentLight
                Layout.preferredWidth: Theme.iconSize
                Layout.preferredHeight: Theme.iconSize
            }

            Text {
                text: qsTr("Export")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }

            Text {
                text: "·  " + root.compactProjectName(root.projectName)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }

            Button {
                id: closeButton
                implicitWidth: 32
                implicitHeight: 32
                onClicked: root.close()
                contentItem: LineIcon {
                    anchors.centerIn: parent
                    name: "close"
                    color: closeButton.hovered ? Theme.textPrimary : Theme.textSecondary
                    width: 14
                    height: 14
                }
                background: Rectangle {
                    radius: 6
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("PRESETS")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                font.letterSpacing: 1.0
            }

            Repeater {
                model: [
                    { label: qsTr("Source quality"), tab: root.videoSource ? 0 : 1 },
                    { label: qsTr("Audio review"), tab: 1 },
                    { label: qsTr("Subtitle handoff"), tab: 2 },
                    { label: qsTr("Review package"), tab: 3 }
                ]
                delegate: Button {
                    id: presetButton
                    required property var modelData
                    text: modelData.label
                    implicitHeight: 28
                    onClicked: root.selectedTab = modelData.tab
                    contentItem: Text {
                        anchors.fill: parent
                        text: presetButton.text
                        color: presetButton.hovered ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        font.bold: presetButton.hovered
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: presetButton.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.06) }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.paddingMedium
            spacing: Theme.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                Repeater {
                    model: [
                        { label: qsTr("Video"), icon: "dubbing" },
                        { label: qsTr("Audio"), icon: "volume" },
                        { label: qsTr("Subtitles"), icon: "file" },
                        { label: qsTr("Package"), icon: "folder" }
                    ]
                    delegate: Button {
                        id: exportTabButton
                        required property int index
                        required property var modelData
                        Layout.preferredWidth: 118
                        implicitHeight: 36
                        onClicked: root.selectedTab = exportTabButton.index
                        contentItem: Item {
                            LineIcon {
                                id: exportTabIcon
                                name: exportTabButton.modelData.icon
                                color: root.selectedTab === exportTabButton.index ? Theme.accentLight : Theme.textSecondary
                                width: 15
                                height: 15
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: exportTabLabel.left
                                anchors.rightMargin: 6
                            }
                            Text {
                                id: exportTabLabel
                                anchors.centerIn: parent
                                width: Math.max(0, Math.min(implicitWidth, parent.width - exportTabIcon.width - 6))
                                text: exportTabButton.modelData.label
                                color: root.selectedTab === exportTabButton.index ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                font.bold: root.selectedTab === exportTabButton.index
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }
                        background: Rectangle {
                            color: "transparent"
                            border.color: "transparent"
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 2
                                color: root.selectedTab === exportTabButton.index ? Theme.accent : "transparent"
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("%1 segments · %2 clips ready")
                          .arg(root.segmentCount).arg(root.generatedClipCount)
                    color: root.generatedClipCount === root.segmentCount && root.segmentCount > 0
                           ? Theme.success : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.selectedTab

                ExportPane {
                    title: qsTr("Dubbed video")
                    description: root.videoSource
                                 ? qsTr("Keep the source video stream and replace its audio with the rendered dubbing mix.")
                                 : qsTr("The current source is audio-only. Use the Audio tab for this project.")
                    detail: qsTr("MP4 · source video quality · AAC 192 kbps")
                    iconName: "dubbing"
                    actionText: qsTr("Export video")
                    actionEnabled: root.videoSource && root.segmentCount > 0 && !root.busy
                    onActionRequested: root.videoExportRequested()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.paddingMedium

                    ExportPane {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 154
                        paneHeight: 154
                        title: qsTr("Full dubbing mix")
                        description: qsTr("Export the timed generated voices together with the separated background track.")
                        detail: qsTr("WAV · lossless · ready for editing or mastering")
                        iconName: "waves"
                        actionText: qsTr("Export WAV")
                        actionEnabled: root.segmentCount > 0 && !root.busy
                        onActionRequested: root.audioExportRequested("mix")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.paddingMedium

                        ExportPane {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            paneHeight: 130
                            title: qsTr("Dubbed vocal")
                            description: qsTr("Export only the generated translated voice stem.")
                            detail: qsTr("WAV · lossless · independent stem")
                            iconName: "mic"
                            actionText: qsTr("Export vocal")
                            actionEnabled: root.segmentCount > 0 && !root.busy
                            onActionRequested: root.audioExportRequested("vocal")
                        }

                        ExportPane {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            paneHeight: 130
                            title: qsTr("Background")
                            description: qsTr("Export the separated background track without dubbing voices.")
                            detail: qsTr("WAV · lossless · independent stem")
                            iconName: "volume"
                            actionText: qsTr("Export background")
                            actionEnabled: root.segmentCount > 0 && !root.busy
                            onActionRequested: root.audioExportRequested("background")
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.paddingMedium

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 158
                            color: Qt.rgba(1, 1, 1, 0.025)
                            radius: Theme.radiusSmall
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingLarge
                                spacing: Theme.paddingMedium

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.paddingXL

                                    ColumnLayout {
                                        Layout.preferredWidth: 190
                                        spacing: 7
                                        Text {
                                            text: qsTr("FORMAT")
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                            font.bold: true
                                            font.letterSpacing: 1.0
                                        }
                                        RowLayout {
                                            spacing: 4
                                            Repeater {
                                                model: ["srt", "vtt"]
                                                delegate: Button {
                                                    id: formatButton
                                                    required property string modelData
                                                    implicitWidth: 78
                                                    implicitHeight: 34
                                                    onClicked: root.subtitleFormat = formatButton.modelData
                                                    contentItem: Text {
                                                        anchors.fill: parent
                                                        text: formatButton.modelData.toUpperCase()
                                                        color: root.subtitleFormat === formatButton.modelData
                                                               ? Theme.textPrimary : Theme.textSecondary
                                                        font.pixelSize: Theme.fontSmall
                                                        font.bold: true
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: Theme.radiusSmall
                                                        color: root.subtitleFormat === formatButton.modelData
                                                               ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                                                               : Qt.rgba(1, 1, 1, 0.025)
                                                        border.color: root.subtitleFormat === formatButton.modelData
                                                                      ? Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.55)
                                                                      : Qt.rgba(1, 1, 1, 0.08)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 7
                                        Text {
                                            text: qsTr("SUBTITLE LANGUAGE")
                                            color: Theme.textSecondary
                                            font.pixelSize: 10
                                            font.bold: true
                                            font.letterSpacing: 1.0
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.paddingSmall
                                            Repeater {
                                                model: [
                                                    {
                                                        target: false,
                                                        label: qsTr("Original"),
                                                        name: root.sourceLanguageName,
                                                        code: root.sourceLanguageCode
                                                    },
                                                    {
                                                        target: true,
                                                        label: qsTr("Translated"),
                                                        name: root.targetLanguageName,
                                                        code: root.targetLanguageCode
                                                    }
                                                ]
                                                delegate: Button {
                                                    id: languageButton
                                                    required property var modelData
                                                    readonly property bool selected:
                                                        root.includeDubbedSubtitles === languageButton.modelData.target
                                                    Layout.fillWidth: true
                                                    implicitHeight: 52
                                                    onClicked: root.includeDubbedSubtitles = languageButton.modelData.target
                                                    contentItem: RowLayout {
                                                        spacing: Theme.paddingSmall
                                                        Rectangle {
                                                            Layout.preferredWidth: 24
                                                            Layout.preferredHeight: 24
                                                            radius: 12
                                                            color: languageButton.selected
                                                                   ? Theme.accent : Qt.rgba(1, 1, 1, 0.06)
                                                            LineIcon {
                                                                anchors.centerIn: parent
                                                                name: languageButton.selected ? "check" : "translate"
                                                                color: languageButton.selected
                                                                       ? Theme.textPrimary : Theme.textSecondary
                                                                width: 13
                                                                height: 13
                                                            }
                                                        }
                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 1
                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: languageButton.modelData.label
                                                                color: languageButton.selected
                                                                       ? Theme.textPrimary : Theme.textSecondary
                                                                font.pixelSize: Theme.fontSmall
                                                                font.bold: true
                                                            }
                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: (languageButton.modelData.name
                                                                       || languageButton.modelData.code)
                                                                      + "  ·  "
                                                                      + languageButton.modelData.code
                                                                color: languageButton.selected
                                                                       ? Theme.accentLight : Theme.textSecondary
                                                                font.pixelSize: 10
                                                                elide: Text.ElideRight
                                                            }
                                                        }
                                                    }
                                                    background: Rectangle {
                                                        radius: Theme.radiusSmall
                                                        color: languageButton.selected
                                                               ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                                                               : Qt.rgba(1, 1, 1, 0.02)
                                                        border.color: languageButton.selected
                                                                      ? Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.48)
                                                                      : Qt.rgba(1, 1, 1, 0.08)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    radius: Theme.radiusSmall
                                    color: Qt.rgba(1, 1, 1, 0.025)
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.paddingMedium
                                        anchors.rightMargin: Theme.paddingMedium
                                        LineIcon {
                                            name: "file"
                                            color: Theme.textSecondary
                                            Layout.preferredWidth: 14
                                            Layout.preferredHeight: 14
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("Segment timing is preserved. Subtitle text is exported as UTF-8.")
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontSmall
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 58
                            radius: Theme.radiusSmall
                            color: Qt.rgba(1, 1, 1, 0.018)
                            border.color: Qt.rgba(1, 1, 1, 0.06)
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingMedium
                                spacing: Theme.paddingMedium
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: qsTr("OUTPUT")
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                        font.bold: true
                                        font.letterSpacing: 1.0
                                    }
                                    Text {
                                        text: "subtitles-"
                                              + (root.includeDubbedSubtitles
                                                 ? root.targetLanguageCode : root.sourceLanguageCode)
                                              + "." + root.subtitleFormat
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSmall
                                    }
                                }
                                PrimaryButton {
                                    text: qsTr("Export subtitles")
                                    iconName: "download"
                                    implicitWidth: 170
                                    enabled: root.segmentCount > 0
                                    onClicked: root.subtitleExportRequested(
                                                   root.subtitleFormat,
                                                   root.includeDubbedSubtitles,
                                                   root.includeDubbedSubtitles
                                                   ? root.targetLanguageCode : root.sourceLanguageCode)
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                ExportPane {
                    title: qsTr("Review package")
                    description: qsTr("Create a folder containing the project snapshot, rendered mix, available stems, subtitles, and per-segment clips.")
                    detail: qsTr("Folder package · manifest included · local files only")
                    iconName: "folder"
                    actionText: qsTr("Export package")
                    actionEnabled: root.segmentCount > 0 && !root.busy
                    onActionRequested: root.packageExportRequested()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.paddingMedium
            Layout.rightMargin: Theme.paddingMedium
            Layout.topMargin: Theme.paddingSmall
            Layout.bottomMargin: Theme.paddingSmall

            LineIcon {
                name: "folder"
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
            Text {
                text: qsTr("Exports stay on this device.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                Layout.fillWidth: true
            }
            PrimaryButton {
                text: qsTr("Close")
                quiet: true
                implicitWidth: 76
                implicitHeight: 32
                onClicked: root.close()
            }
        }
    }

    component ExportPane: Item {
        id: pane
        property string title: ""
        property string description: ""
        property string detail: ""
        property string iconName: "download"
        property string actionText: ""
        property int paneHeight: 154
        property string secondaryActionText: ""
        property bool actionEnabled: true
        signal actionRequested()
        signal secondaryActionRequested()

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: pane.paneHeight
            color: Qt.rgba(1, 1, 1, 0.025)
            radius: Theme.radiusSmall
            border.color: Qt.rgba(1, 1, 1, 0.08)

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingLarge
                spacing: Theme.paddingLarge

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)
                    LineIcon {
                        anchors.centerIn: parent
                        name: pane.iconName
                        color: Theme.accentLight
                        width: 23
                        height: 23
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text {
                        Layout.fillWidth: true
                        text: pane.title
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: pane.description
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        text: pane.detail
                        color: Theme.accentLight
                        font.pixelSize: Theme.fontSmall
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 150
                    spacing: Theme.paddingSmall
                    PrimaryButton {
                        Layout.fillWidth: true
                        text: pane.actionText
                        iconName: "download"
                        enabled: pane.actionEnabled
                        onClicked: pane.actionRequested()
                    }
                    PrimaryButton {
                        Layout.fillWidth: true
                        visible: pane.secondaryActionText !== ""
                        text: pane.secondaryActionText
                        iconName: "download"
                        quiet: true
                        enabled: pane.actionEnabled
                        onClicked: pane.secondaryActionRequested()
                    }
                }
            }
        }
    }
}
