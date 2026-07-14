import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../../components"
import "../../components/base"
import "../../components/shared/settings"

ScrollView {
    id: root

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    property bool wideLayout: availableWidth >= 980
    property bool openDownloadsPaneOnStart: false
    readonly property int translationRevision: AppController.localization.revision
    readonly property int contentMaxWidth: 1120
    readonly property int rowLabelWidth: 210

    function updateChannelOptions() {
        root.translationRevision
        return [qsTr("Stable"), qsTr("Beta")]
    }

    function selectedUpdateChannel() {
        return channelCombo.currentIndex === 1 ? "beta" : "stable"
    }

    function updateActionText() {
        if (AppController.updates.checking) return qsTr("Checking...")
        if (AppController.updates.downloading) return qsTr("Downloading...")
        if (AppController.updates.downloaded) return qsTr("Install update")
        if (AppController.updates.updateAvailable) return qsTr("Download update")
        return qsTr("Check for updates")
    }

    function runUpdateAction() {
        if (AppController.updates.downloaded) {
            installUpdateDialog.open()
        } else if (AppController.updates.updateAvailable) {
            AppController.updates.downloadUpdate()
        } else {
            AppController.updates.checkForUpdates(root.selectedUpdateChannel())
        }
    }

    function languageIndexFor(language) {
        const languages = AppController.localization.availableLanguages
        for (var i = 0; i < languages.length; i++) {
            if (languages[i].value === language) {
                return i
            }
        }
        return 0
    }

    ColumnLayout {
        width: Math.min(root.contentMaxWidth, Math.max(0, root.availableWidth - Theme.paddingMedium * 2))
        anchors.left: parent.left
        anchors.leftMargin: Theme.paddingMedium
        spacing: Theme.paddingLarge

        Item { Layout.preferredHeight: 2 }

        GridLayout {
            Layout.fillWidth: true
            columns: root.wideLayout ? 2 : 1
            columnSpacing: Theme.paddingLarge
            rowSpacing: Theme.paddingLarge

            SectionPanel {
                title: qsTr("App Update")
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop

                SettingsRow {
                    label: qsTr("Installed")
                    valueText: qsTr("%1 v%2").arg(Qt.application.name).arg(Qt.application.version)
                }

                ThinDivider {}

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    Text {
                        text: qsTr("Updates channel")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        Layout.preferredWidth: root.rowLabelWidth
                        Layout.maximumWidth: root.rowLabelWidth
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingMedium

                        PrimaryButton {
                            text: root.updateActionText()
                            buttonColor: Theme.surfaceAlt
                            implicitWidth: 142
                            implicitHeight: 32
                            quiet: true
                            enabled: !AppController.updates.checking && !AppController.updates.downloading
                            onClicked: root.runUpdateAction()
                        }

                        AppComboBox {
                            id: channelCombo
                            model: root.updateChannelOptions()
                            currentIndex: 0
                            implicitWidth: 110
                            implicitHeight: 32
                            enabled: !AppController.updates.checking && !AppController.updates.downloading
                        }

                        Item { Layout.fillWidth: true }
                    }
                }

                ProgressBar {
                    visible: AppController.updates.downloading
                    from: 0
                    to: 1
                    value: AppController.updates.downloadProgress
                    Layout.fillWidth: true
                }

                Text {
                    visible: AppController.updates.statusMessage !== "" || AppController.updates.errorMessage !== ""
                    text: AppController.updates.errorMessage !== "" ? AppController.updates.errorMessage : AppController.updates.statusMessage
                    color: AppController.updates.errorMessage !== "" ? Theme.danger : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            SectionPanel {
                title: qsTr("User Interface")
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    ColumnLayout {
                        Layout.preferredWidth: root.rowLabelWidth
                        Layout.maximumWidth: root.rowLabelWidth
                        spacing: 3

                        Text {
                            text: qsTr("App Language")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("Still in development")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                    }

                    AppComboBox {
                        id: langCombo
                        model: AppController.localization.availableLanguages
                        textRole: "text"
                        currentIndex: root.languageIndexFor(AppController.localization.currentLanguage)
                        Layout.fillWidth: true
                        Layout.maximumWidth: 260
                        implicitHeight: 32
                        onActivated: (index) => {
                            AppController.localization.currentLanguage = model[index].value
                        }

                        Connections {
                            target: AppController.localization
                            function onCurrentLanguageChanged() {
                                langCombo.currentIndex = root.languageIndexFor(AppController.localization.currentLanguage)
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            SectionPanel {
                title: qsTr("General Settings")
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingMedium

                    Text {
                        text: qsTr("Open downloads pane when starting a new model download")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    ToggleRow {
                        text: ""
                        checked: root.openDownloadsPaneOnStart
                        Layout.fillWidth: false
                        Layout.preferredWidth: 42
                        onToggled: root.openDownloadsPaneOnStart = checked
                    }
                }
            }

            SectionPanel {
                title: qsTr("Models Directory")
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Text {
                        text: qsTr("Model downloads and indexing location")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        PathPill {
                            path: AppController.settings.modelsPath
                            Layout.fillWidth: true
                        }

                        PrimaryButton {
                            iconName: "more-horizontal"
                            buttonColor: Theme.surfaceAlt
                            implicitWidth: 36
                            implicitHeight: 32
                            quiet: true
                            enabled: !AppController.modelsMigration.running
                            onClicked: folderDialog.open()
                        }
                    }

                    Text {
                        visible: AppController.modelsMigration.running || AppController.modelsMigration.message !== ""
                        text: AppController.modelsMigration.message
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: AppController.modelsMigration.error !== ""
                        text: AppController.modelsMigration.error
                        color: Theme.danger
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        SectionPanel {
            title: qsTr("App Info")
            Layout.fillWidth: true

            InfoActionRow {
                label: qsTr("Open app logs")
                actionText: qsTr("Open")
                onActivated: Qt.openUrlExternally("file:///" + AppController.logsDir)
            }

            ThinDivider {}

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingMedium

                Text {
                    text: qsTr("App home directory")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    Layout.preferredWidth: root.rowLabelWidth
                    Layout.maximumWidth: root.rowLabelWidth
                }

                PathPill {
                    path: AppController.dataDir
                    Layout.fillWidth: true
                }

                PrimaryButton {
                    iconName: "copy"
                    buttonColor: Theme.surfaceAlt
                    implicitWidth: 32
                    implicitHeight: 30
                    quiet: true
                    onClicked: AppController.copyToClipboard(AppController.dataDir)
                }
            }

            ThinDivider {}

            InfoActionRow {
                label: qsTr("Report bug or send feedback")
                actionText: qsTr("Open in browser")
                actionIcon: "external-link"
                actionWidth: 146
                onActivated: Qt.openUrlExternally("https://github.com/dduongtrandai/LA-Studio/issues")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.paddingMedium
            Layout.bottomMargin: Theme.paddingLarge
            spacing: Theme.paddingSmall

            Image {
                id: laStudioLogo
                Layout.alignment: Qt.AlignHCenter
                width: 32
                height: 32
                source: "qrc:/LAStudio/icons/app_icon_32.png"
                fillMode: Image.PreserveAspectFit
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Made by Tran Dai Duong")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                opacity: 0.6
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select Models Directory")
        onAccepted: {
            AppController.modelsMigration.changeDirectory(selectedFolder.toString())
        }
    }

    ConfirmationDialog {
        id: installUpdateDialog
        titleText: qsTr("Install update")
        messageText: qsTr("LA Studio will close and start the installer. Your app data and downloaded models will stay in the app home directory.")
        confirmText: qsTr("Install")
        cancelText: qsTr("Cancel")
        onConfirmed: AppController.updates.installDownloadedUpdate()
    }

    component SectionPanel: Rectangle {
        property string title: ""
        default property alias content: panelContent.data

        implicitHeight: panelLayout.implicitHeight + Theme.paddingLarge * 2
        radius: Theme.radiusMedium
        color: Qt.rgba(255, 255, 255, 0.018)
        border.color: Qt.rgba(255, 255, 255, 0.055)
        border.width: 1

        ColumnLayout {
            id: panelLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            Text {
                text: title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                Layout.fillWidth: true
            }

            ColumnLayout {
                id: panelContent
                Layout.fillWidth: true
                spacing: Theme.paddingMedium
            }
        }
    }

    component SettingsRow: RowLayout {
        property string label: ""
        property string valueText: ""

        Layout.fillWidth: true
        spacing: Theme.paddingMedium

        Text {
            text: label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            font.bold: true
            Layout.preferredWidth: root.rowLabelWidth
            Layout.maximumWidth: root.rowLabelWidth
        }

        Text {
            text: valueText
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    component InfoActionRow: RowLayout {
        id: actionRow

        property string label: ""
        property string actionText: ""
        property string actionIcon: ""
        property int actionWidth: 70
        signal activated()

        Layout.fillWidth: true
        spacing: Theme.paddingMedium

        Text {
            text: label
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            Layout.fillWidth: true
        }

        PrimaryButton {
            text: actionText
            iconName: actionIcon
            buttonColor: Theme.surfaceAlt
            implicitWidth: actionWidth
            implicitHeight: 30
            quiet: true
            onClicked: actionRow.activated()
        }
    }

    component PathPill: Rectangle {
        property string path: ""

        implicitHeight: 30
        radius: Theme.radiusSmall
        color: Qt.rgba(255, 255, 255, 0.04)
        border.color: Qt.rgba(255, 255, 255, 0.08)
        border.width: 1

        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.paddingMedium
            anchors.rightMargin: Theme.paddingMedium
            text: path
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
