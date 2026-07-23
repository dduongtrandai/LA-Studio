import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Rectangle {
    id: root

    property var window
    property string appName: "LA Studio"
    property string appVersion: Qt.application.version
    readonly property bool maximized: window && window.visibility === Window.Maximized

    Layout.fillWidth: true
    Layout.preferredHeight: 34
    color: Theme.background

    // Bottom subtle border line to separate title bar from app content
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    function toggleMaximized() {
        if (!window) return
        if (maximized) {
            window.showNormal()
        } else {
            window.showMaximized()
        }
    }

    function minimizeWindow() {
        if (!window) return
        if (window.showMinimized) {
            window.showMinimized()
        }
    }

    // Window drag and double click area
    MouseArea {
        id: dragArea
        anchors.fill: parent
        anchors.rightMargin: windowControls.width
        acceptedButtons: Qt.LeftButton
        z: 0
        onPressed: {
            if (root.window && root.window.startSystemMove) {
                root.window.startSystemMove()
            }
        }
        onDoubleClicked: root.toggleMaximized()
    }

    // Centered Title & Version (iOS minimalist style)
    RowLayout {
        anchors.centerIn: parent
        spacing: 8
        z: 1

        Image {
            source: "qrc:/LAStudio/icons/app_icon_32.png"
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            fillMode: Image.PreserveAspectFit
            opacity: 0.9
        }

        Text {
            text: root.appName
            color: Theme.textPrimary
            font.pixelSize: 13
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            Layout.preferredWidth: 4
            Layout.preferredHeight: 4
            radius: 2
            color: Theme.textSecondary
            opacity: 0.4
        }

        Text {
            text: "v" + root.appVersion
            color: Theme.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Windows control buttons on the right
    RowLayout {
        id: windowControls
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0
        z: 2

        WindowButton {
            iconName: "minus"
            hoverColor: Qt.rgba(1, 1, 1, 0.12)
            onClicked: root.minimizeWindow()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Minimize")
            ToolTip.delay: 500
        }

        WindowButton {
            iconName: root.maximized ? "restore" : "maximize"
            hoverColor: Qt.rgba(1, 1, 1, 0.12)
            onClicked: root.toggleMaximized()
            ToolTip.visible: hovered
            ToolTip.text: root.maximized ? qsTr("Restore") : qsTr("Maximize")
            ToolTip.delay: 500
        }

        WindowButton {
            iconName: "close"
            hoverColor: "#e81123"
            onClicked: if (root.window) root.window.close()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Close")
            ToolTip.delay: 500
        }
    }

    component WindowButton: Button {
        id: button

        property string iconName: ""
        property color hoverColor: Qt.rgba(1, 1, 1, 0.12)

        Layout.preferredWidth: 46
        Layout.fillHeight: true
        padding: 0
        flat: true

        contentItem: LineIcon {
            name: button.iconName
            color: button.hovered && button.iconName === "close" ? "white" : Theme.textPrimary
            strokeWidth: 1.55
            anchors.centerIn: parent
            width: 12
            height: 12
        }

        background: Rectangle {
            color: button.hovered ? button.hoverColor : "transparent"
        }
    }
}

