import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import ".."
import "../base"

Dialog {
    id: root

    required property var dubbing
    property string connectionMessage: ""
    property bool connectionSuccess: false
    property int segmentIndex: -1
    readonly property string providerName: (dubbing.translationFixConfiguration || {}).provider === "api"
                                           ? qsTr("LLM API") : qsTr("LM Studio")

    function repairCount() {
        return segmentIndex >= 0 ? 1 : dubbing.translationFixCandidateCount
    }

    function openForAll() {
        segmentIndex = -1
        open()
    }

    function openForSegment(index) {
        segmentIndex = index
        open()
    }

    width: Math.min(620, parent ? parent.width - Theme.paddingXL * 2 : 620)
    height: Math.min(610, parent ? parent.height - Theme.paddingXL * 2 : 610)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: 0
    title: ""
    closePolicy: dubbing.translationFixing ? Popup.NoAutoClose
                                            : Popup.CloseOnEscape

    function currentConfiguration() {
        return {
            provider: (dubbing.translationFixConfiguration || {}).provider || "lmstudio",
            configured: true,
            serverUrl: serverUrlField.text.trim(),
            model: modelField.text.trim(),
            apiKey: apiKeyField.text.trim(),
            maxAttempts: attemptsSpin.value,
            temperature: Number(temperatureField.text)
        }
    }

    function loadConfiguration() {
        var config = dubbing.translationFixConfiguration || {}
        serverUrlField.text = config.serverUrl || "http://127.0.0.1:1234"
        modelField.text = config.model || "qwen3.5-2b"
        apiKeyField.text = config.apiKey || ""
        attemptsSpin.value = config.maxAttempts || 4
        temperatureField.text = String(config.temperature !== undefined
                                       ? config.temperature : 0.35)
        connectionMessage = ""
        connectionSuccess = false
    }

    onOpened: loadConfiguration()

    Connections {
        target: root.dubbing
        function onTranslationFixConnectionTested(success, message) {
            root.connectionSuccess = success
            root.connectionMessage = message
        }
    }

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
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingSmall

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                               Theme.accent.b, 0.14)
                LineIcon {
                    anchors.centerIn: parent
                    name: "spark"
                    color: Theme.accentLight
                    width: 18
                    height: 18
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("Fix translation length")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    text: root.segmentIndex >= 0
                          ? qsTr("Rewrite only segment %1 with the configured %2 model.")
                                .arg(root.segmentIndex + 1).arg(root.providerName)
                          : qsTr("Rewrite over-budget segments with the configured %1 model.")
                                .arg(root.providerName)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Button {
                implicitWidth: 32
                implicitHeight: 32
                enabled: !root.dubbing.translationFixing
                onClicked: root.close()
                contentItem: LineIcon {
                    anchors.centerIn: parent
                    name: "close"
                    color: parent.enabled ? Theme.textSecondary
                                          : Qt.rgba(Theme.textSecondary.r,
                                                    Theme.textSecondary.g,
                                                    Theme.textSecondary.b, 0.4)
                    width: 14
                    height: 14
                }
                background: Rectangle {
                    radius: 6
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06)
                                          : "transparent"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge
                    Layout.rightMargin: Theme.paddingLarge
                    Layout.topMargin: Theme.paddingLarge
                    implicitHeight: summaryLayout.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.warning.r, Theme.warning.g,
                                   Theme.warning.b, 0.08)
                    border.color: Qt.rgba(Theme.warning.r, Theme.warning.g,
                                          Theme.warning.b, 0.28)
                    RowLayout {
                        id: summaryLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        spacing: Theme.paddingMedium
                        LineIcon {
                            name: "activity"
                            color: Theme.warning
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: root.segmentIndex >= 0
                                      ? qsTr("Segment %1 exceeds the phoneme budget")
                                            .arg(root.segmentIndex + 1)
                                      : qsTr("%1 segment(s) exceed the phoneme budget")
                                            .arg(root.repairCount())
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Only a rewrite verified inside each segment's eSpeak NG budget will be accepted.")
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge
                    Layout.rightMargin: Theme.paddingLarge
                    spacing: Theme.paddingSmall

                    FieldLabel { text: qsTr("%1 server URL").arg(root.providerName) }
                    FixField {
                        id: serverUrlField
                        Layout.fillWidth: true
                        placeholderText: "http://127.0.0.1:1234"
                        enabled: !root.dubbing.translationFixing
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.providerName === qsTr("LM Studio")
                              ? qsTr("Uses LM Studio /api/v1/chat with reasoning disabled for concise rewrites.")
                              : qsTr("Uses an OpenAI-compatible /v1/chat/completions endpoint.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    FieldLabel {
                        Layout.topMargin: Theme.paddingSmall
                        text: qsTr("API model identifier")
                    }
                    FixField {
                        id: modelField
                        Layout.fillWidth: true
                        placeholderText: "qwen3.5-2b"
                        enabled: !root.dubbing.translationFixing
                    }

                    FieldLabel {
                        Layout.topMargin: Theme.paddingSmall
                        text: qsTr("API token (optional)")
                    }
                    FixField {
                        id: apiKeyField
                        Layout.fillWidth: true
                        placeholderText: qsTr("No token for the default local server")
                        echoMode: TextInput.Password
                        enabled: !root.dubbing.translationFixing
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.paddingSmall
                        spacing: Theme.paddingMedium
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            FieldLabel { text: qsTr("Maximum attempts") }
                            SpinBox {
                                id: attemptsSpin
                                Layout.fillWidth: true
                                from: 1
                                to: 8
                                value: 4
                                editable: true
                                enabled: !root.dubbing.translationFixing
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            FieldLabel { text: qsTr("Temperature") }
                            FixField {
                                id: temperatureField
                                Layout.fillWidth: true
                                text: "0.35"
                                enabled: !root.dubbing.translationFixing
                                validator: DoubleValidator {
                                    bottom: 0
                                    top: 1.5
                                    decimals: 2
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.paddingLarge
                    Layout.rightMargin: Theme.paddingLarge
                    implicitHeight: statusLayout.implicitHeight + Theme.paddingMedium * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(1, 1, 1, 0.025)
                    border.color: root.connectionMessage !== ""
                                  ? (root.connectionSuccess
                                     ? Qt.rgba(Theme.success.r, Theme.success.g,
                                               Theme.success.b, 0.35)
                                     : Qt.rgba(Theme.danger.r, Theme.danger.g,
                                               Theme.danger.b, 0.35))
                                  : Qt.rgba(1, 1, 1, 0.08)
                    ColumnLayout {
                        id: statusLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingMedium
                        spacing: Theme.paddingSmall
                        Text {
                            Layout.fillWidth: true
                            text: root.dubbing.translationFixing
                                  ? root.dubbing.translationFixStatus
                                  : (root.connectionMessage !== ""
                                     ? root.connectionMessage
                                     : (root.dubbing.translationFixStatus !== ""
                                        ? root.dubbing.translationFixStatus
                                        : qsTr("Test the server before starting the repair.")))
                            color: root.dubbing.translationFixing
                                   ? Theme.accentLight
                                   : (root.connectionMessage === ""
                                      ? Theme.textSecondary
                                      : (root.connectionSuccess
                                         ? Theme.success : Theme.danger))
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            visible: root.dubbing.translationFixing
                            from: 0
                            to: 100
                            value: root.dubbing.translationFixProgress
                        }
                    }
                }

                Item { Layout.preferredHeight: Theme.paddingSmall }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            PrimaryButton {
                text: qsTr("Test connection")
                iconName: "activity"
                quiet: true
                enabled: !root.dubbing.processing
                         && serverUrlField.text.trim() !== ""
                         && modelField.text.trim() !== ""
                onClicked: {
                    root.connectionMessage = qsTr("Checking %1...").arg(root.providerName)
                    root.connectionSuccess = false
                    root.dubbing.testTranslationFixConnection(
                                root.currentConfiguration())
                }
            }
            Item { Layout.fillWidth: true }
            PrimaryButton {
                text: root.dubbing.translationFixing ? qsTr("Cancel")
                                                     : qsTr("Close")
                quiet: true
                implicitWidth: 90
                onClicked: {
                    if (root.dubbing.translationFixing)
                        root.dubbing.cancelTranslationFix()
                    else
                        root.close()
                }
            }
            PrimaryButton {
                text: root.segmentIndex >= 0
                      ? qsTr("Fix segment %1").arg(root.segmentIndex + 1)
                      : qsTr("Fix %1 segment(s)").arg(root.repairCount())
                iconName: "spark"
                loading: root.dubbing.translationFixing
                enabled: !root.dubbing.processing
                         && root.repairCount() > 0
                         && serverUrlField.text.trim() !== ""
                         && modelField.text.trim() !== ""
                onClicked: {
                    root.connectionMessage = ""
                    if (root.segmentIndex >= 0)
                        root.dubbing.fixTranslationSegment(
                                    root.segmentIndex,
                                    root.currentConfiguration())
                    else
                        root.dubbing.fixTranslations(root.currentConfiguration())
                }
            }
        }
    }

    component FixField: TextField {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        leftPadding: Theme.paddingMedium
        rightPadding: Theme.paddingMedium
        implicitHeight: 34
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.16)
            border.color: parent.activeFocus ? Theme.accent
                                              : Qt.rgba(1, 1, 1, 0.09)
            border.width: parent.activeFocus ? 2 : 1
        }
    }
}
