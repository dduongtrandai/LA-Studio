import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Rectangle {
    id: panel

    property bool isExpanded: false
    property bool isResizing: false
    property bool scrollAfterNextLoad: false
    readonly property int collapsedHeight: 36
    readonly property int minExpandedHeight: 120
    readonly property int expandedHeight: 220
    readonly property int maxExpandedHeight: Math.max(expandedHeight, Math.floor((parent ? parent.height : 800) * 0.65))
    property int panelHeight: expandedHeight

    color: Theme.surface
    clip: true

    function clampPanelHeight(height) {
        return Math.max(minExpandedHeight, Math.min(maxExpandedHeight, height))
    }

    function toggleExpanded() {
        if (!isExpanded && panelHeight < expandedHeight) {
            panelHeight = expandedHeight
        }
        isExpanded = !isExpanded
    }

    function scrollToLatestLog() {
        Qt.callLater(function() {
            if (!logScrollView.contentItem) {
                return
            }

            var maxY = Math.max(0, logScrollView.contentItem.contentHeight - logScrollView.contentItem.height)
            logScrollView.contentItem.contentY = maxY
        })
    }

    implicitHeight: isExpanded ? panelHeight : collapsedHeight

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.surfaceAlt
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 35
            color: Theme.surface

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: panel.toggleExpanded()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingSmall

                LineIcon {
                    name: panel.isExpanded ? "chevron-down" : "chevron-up"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    strokeWidth: 2.0
                }

                Text {
                    text: qsTr("System Logs")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }

                BusyIndicator {
                    running: AppController.logs.loading
                    visible: running
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    visible: panel.isExpanded
                    spacing: Theme.paddingSmall

                    CheckBox {
                        id: autoScrollCheck
                        text: qsTr("Auto-Scroll")
                        checked: true

                        indicator: Rectangle {
                            implicitWidth: 14
                            implicitHeight: 14
                            radius: 3
                            color: autoScrollCheck.checked ? Theme.accent : "transparent"
                            border.color: autoScrollCheck.checked ? Theme.accent : Qt.rgba(255, 255, 255, 0.2)
                            border.width: 1.2
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                color: "white"
                                font.pixelSize: 9
                                font.bold: true
                                visible: autoScrollCheck.checked
                            }
                        }

                        contentItem: Text {
                            text: autoScrollCheck.text
                            font.pixelSize: Theme.fontSmall
                            color: Theme.textSecondary
                            leftPadding: autoScrollCheck.indicator.width + Theme.paddingSmall
                            verticalAlignment: Text.AlignVCenter
                        }

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Rectangle {
                        width: 1
                        height: 16
                        color: Qt.rgba(255, 255, 255, 0.1)
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                    }

                    Button {
                        id: refreshButton
                        implicitWidth: 28
                        implicitHeight: 28
                        flat: true

                        AppToolTip {
                            text: qsTr("Refresh Logs")
                            visible: refreshButton.hovered
                        }

                        background: Rectangle {
                            color: refreshButton.pressed ? Qt.rgba(1, 1, 1, 0.12)
                                : (refreshButton.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                            radius: 6
                        }

                        contentItem: LineIcon {
                            name: "refresh"
                            color: refreshButton.hovered ? Theme.accentLight : Theme.textSecondary
                            width: 14
                            height: 14
                            anchors.centerIn: parent
                        }

                        onClicked: AppController.logs.requestLoadLogs()
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Button {
                        id: copyButton
                        implicitWidth: 28
                        implicitHeight: 28
                        flat: true

                        AppToolTip {
                            text: qsTr("Copy Logs")
                            visible: copyButton.hovered
                        }

                        background: Rectangle {
                            color: copyButton.pressed ? Qt.rgba(1, 1, 1, 0.12)
                                : (copyButton.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                            radius: 6
                        }

                        contentItem: LineIcon {
                            name: "copy"
                            color: copyButton.hovered ? Theme.accentLight : Theme.textSecondary
                            width: 14
                            height: 14
                            anchors.centerIn: parent
                        }

                        onClicked: AppController.copyToClipboard(AppController.logs.readLogFile())
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    Button {
                        id: clearButton
                        implicitWidth: 28
                        implicitHeight: 28
                        flat: true

                        AppToolTip {
                            text: qsTr("Clear Logs")
                            visible: clearButton.hovered
                        }

                        background: Rectangle {
                            color: clearButton.pressed ? Qt.rgba(1, 0, 0, 0.15)
                                : (clearButton.hovered ? Qt.rgba(1, 0, 0, 0.1) : "transparent")
                            radius: 6
                        }

                        contentItem: LineIcon {
                            name: "trash"
                            color: clearButton.hovered ? Theme.danger : Theme.textSecondary
                            width: 14
                            height: 14
                            anchors.centerIn: parent
                        }

                        onClicked: {
                            AppController.logs.clearLogFile()
                            logTextArea.text = ""
                            AppController.logs.requestLoadLogs()
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(0, 0, 0, 0.25)
            visible: panel.isExpanded

            ScrollView {
                id: logScrollView
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                TextArea {
                    id: logTextArea
                    width: logScrollView.width - Theme.paddingSmall * 2
                    selectByMouse: true
                    readOnly: true
                    textFormat: Text.RichText
                    wrapMode: Text.WrapAnywhere
                    font.family: "Consolas"
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    selectionColor: Theme.accent
                    selectedTextColor: "white"
                    placeholderText: qsTr("No logs captured yet.")
                    placeholderTextColor: Theme.textSecondary

                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }
        }
    }

    Item {
        id: resizeHandle
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 10
        z: 10

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 3
            width: 48
            height: 3
            radius: 1.5
            color: Theme.textSecondary
            opacity: resizeMouseArea.containsMouse || panel.isResizing ? 0.55 : 0

            Behavior on opacity { NumberAnimation { duration: 120 } }
        }

        MouseArea {
            id: resizeMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeVerCursor
            preventStealing: true

            property real pressY: 0
            property int pressHeight: panel.expandedHeight
            property bool dragged: false

            onPressed: function(mouse) {
                pressY = mouse.y
                pressHeight = panel.isExpanded ? panel.panelHeight : panel.collapsedHeight
                dragged = false
                panel.isResizing = true
            }

            onPositionChanged: function(mouse) {
                if (!pressed) {
                    return
                }

                var deltaY = mouse.y - pressY
                var targetHeight = pressHeight - deltaY
                dragged = dragged || Math.abs(deltaY) > 3

                if (targetHeight > panel.collapsedHeight + 12) {
                    panel.isExpanded = true
                    panel.panelHeight = panel.clampPanelHeight(targetHeight)
                } else {
                    panel.panelHeight = panel.minExpandedHeight
                    panel.isExpanded = false
                }
            }

            onReleased: {
                panel.isResizing = false
            }

            onCanceled: panel.isResizing = false
        }
    }

    Connections {
        target: AppController.logs
        function onLogContentChanged() {
            var nextContent = AppController.logs.logContent
            if (logTextArea.text !== nextContent) {
                logTextArea.text = nextContent
            }
            if (autoScrollCheck.checked || panel.scrollAfterNextLoad) {
                panel.scrollAfterNextLoad = false
                panel.scrollToLatestLog()
            }
        }
    }

    Timer {
        interval: 1500
        running: panel.isExpanded
        repeat: true
        onTriggered: AppController.logs.requestLoadLogs()
    }

    onIsExpandedChanged: {
        if (isExpanded) {
            panelHeight = clampPanelHeight(panelHeight)
            scrollAfterNextLoad = true
            AppController.logs.requestLoadLogs()
            scrollToLatestLog()
        }
    }
}
