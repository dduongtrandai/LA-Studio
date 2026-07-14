import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Dialog {
    id: root

    width: Math.min(460, parent ? parent.width - 32 : 460)
    height: contentLayout.implicitHeight + Theme.paddingLarge * 2
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    padding: Theme.paddingLarge
    title: ""

    readonly property var links: [
        {
            title: qsTr("Facebook Group"),
            description: qsTr("Community announcements and public discussion."),
            iconName: "users",
            url: "https://www.facebook.com/groups/lastudio.community.vn",
            highlighted: true
        },
        {
            title: qsTr("Discussion & Feedback"),
            description: qsTr("Join the Facebook chat for questions, feedback, and suggestions."),
            iconName: "users",
            url: "https://m.me/ch/AbZaLPyeBxvQ_kLb/",
            highlighted: true
        }
    ]

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Theme.surfaceAlt
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            color: "#00000025"
            radius: Theme.radiusMedium + 8
            z: -1
        }
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 8
                color: Qt.rgba(0.49, 0.30, 1.0, 0.14)
                border.color: Qt.rgba(0.49, 0.30, 1.0, 0.30)
                border.width: 1

                LineIcon {
                    anchors.centerIn: parent
                    name: "users"
                    color: Theme.accentLight
                    width: 18
                    height: 18
                    strokeWidth: 1.9
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Community & Feedback")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Facebook links for discussion, feedback, and suggestions.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
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
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"
                }

                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.surfaceAlt
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            Repeater {
                model: root.links

                delegate: Button {
                    id: linkButton

                    required property var modelData
                    readonly property bool isFeaturedLink: !!modelData.highlighted

                    Layout.fillWidth: true
                    implicitHeight: 64
                    onClicked: Qt.openUrlExternally(modelData.url)

                    contentItem: RowLayout {
                        spacing: Theme.paddingMedium

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 8
                            color: linkButton.isFeaturedLink ? Qt.rgba(0.49, 0.30, 1.0, 0.16) : Qt.rgba(255, 255, 255, 0.035)
                            border.color: linkButton.isFeaturedLink ? Qt.rgba(0.64, 0.49, 1.0, 0.36) : Qt.rgba(255, 255, 255, 0.06)
                            border.width: 1

                            LineIcon {
                                anchors.centerIn: parent
                                name: linkButton.modelData.iconName
                                color: (linkButton.hovered || linkButton.isFeaturedLink) ? Theme.accentLight : Theme.textSecondary
                                width: 18
                                height: 18
                                strokeWidth: 1.8
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                Layout.fillWidth: true
                                text: linkButton.modelData.title
                                color: linkButton.isFeaturedLink ? "#ffffff" : Theme.textPrimary
                                font.pixelSize: Theme.fontMedium
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: linkButton.modelData.description
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                elide: Text.ElideRight
                            }
                        }

                        LineIcon {
                            name: "external-link"
                            color: linkButton.hovered ? Theme.accentLight : Theme.textSecondary
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            strokeWidth: 1.8
                        }
                    }

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: {
                            if (linkButton.pressed) return linkButton.isFeaturedLink ? Qt.rgba(0.49, 0.30, 1.0, 0.24) : Qt.rgba(1, 1, 1, 0.08)
                            if (linkButton.isFeaturedLink && linkButton.hovered) return Qt.rgba(0.49, 0.30, 1.0, 0.20)
                            if (linkButton.isFeaturedLink) return Qt.rgba(0.49, 0.30, 1.0, 0.12)
                            if (linkButton.hovered) return Qt.rgba(1, 1, 1, 0.045)
                            return Qt.rgba(255, 255, 255, 0.018)
                        }
                        border.color: (linkButton.hovered || linkButton.isFeaturedLink) ? Qt.rgba(0.64, 0.49, 1.0, 0.34) : Qt.rgba(255, 255, 255, 0.055)
                        border.width: 1
                    }

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }
    }
}
