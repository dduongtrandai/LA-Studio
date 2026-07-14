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
    Layout.preferredHeight: 30
    color: "#3156e8"

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
        window.showMinimized()
    }

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

    RowLayout {
        anchors.centerIn: parent
        spacing: 6
        z: 1

        Image {
            source: "qrc:/LAStudio/icons/app_icon_32.png"
            Layout.preferredWidth: 15
            Layout.preferredHeight: 15
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: root.appName + " - " + root.appVersion
            color: "white"
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

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
        }

        WindowButton {
            iconName: root.maximized ? "restore" : "maximize"
            hoverColor: Qt.rgba(1, 1, 1, 0.12)
            onClicked: root.toggleMaximized()
        }

        WindowButton {
            iconName: "close"
            hoverColor: "#e81123"
            onClicked: if (root.window) root.window.close()
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
            color: "white"
            strokeWidth: 1.55
            anchors.centerIn: parent
            width: 13
            height: 13
        }

        background: Rectangle {
            color: button.hovered ? button.hoverColor : "transparent"
        }
    }
}
