import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

ColumnLayout {
    id: root

    property var tags: []
    property Item targetEditor: null
    property string title: qsTr("Non-verbal tags")
    property string helperText: qsTr("Click a tag to insert it at the cursor.")
    property bool locked: false

    signal tagInserted(string tag)

    Layout.fillWidth: true
    spacing: 6

    function normalizedTags() {
        var result = []
        for (var i = 0; i < root.tags.length; ++i) {
            var item = root.tags[i]
            if (typeof item === "string") {
                result.push({ text: item, value: item, description: "" })
            } else if (item) {
                result.push({
                    text: item.text || item.label || item.value || item.tag || "",
                    value: item.value || item.tag || item.text || item.label || "",
                    description: item.description || ""
                })
            }
        }
        return result
    }

    function insertTag(tagText) {
        if (!root.targetEditor || tagText === "")
            return

        var editor = root.targetEditor
        var selectionStart = editor.selectionStart !== undefined ? editor.selectionStart : editor.cursorPosition
        var selectionEnd = editor.selectionEnd !== undefined ? editor.selectionEnd : editor.cursorPosition
        if (selectionStart > selectionEnd) {
            var tmp = selectionStart
            selectionStart = selectionEnd
            selectionEnd = tmp
        }

        var currentText = editor.text || ""
        if (selectionStart !== selectionEnd) {
            editor.text = currentText.slice(0, selectionStart) + tagText + currentText.slice(selectionEnd)
            editor.cursorPosition = selectionStart + tagText.length
        } else if (typeof editor.insert === "function") {
            var insertPos = editor.cursorPosition || 0
            editor.insert(insertPos, tagText)
            editor.cursorPosition = insertPos + tagText.length
        } else {
            var pos = editor.cursorPosition || 0
            editor.text = currentText.slice(0, pos) + tagText + currentText.slice(pos)
            editor.cursorPosition = pos + tagText.length
        }

        if (typeof editor.forceActiveFocus === "function")
            editor.forceActiveFocus()

        root.tagInserted(tagText)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        LineIcon {
            name: "spark"
            color: Theme.accent
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
        }

        Text {
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSmall
            font.bold: true
            Layout.fillWidth: true
        }
    }

    Text {
        text: root.helperText
        color: Theme.textSecondary
        font.pixelSize: 10
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Flow {
        id: tagFlow
        Layout.fillWidth: true
        Layout.preferredHeight: childrenRect.height
        spacing: 6

        Repeater {
            model: root.normalizedTags()

            delegate: Button {
                id: tagButton
                property var item: modelData

                implicitHeight: 28
                implicitWidth: tagText.implicitWidth + 18
                padding: 0
                flat: true
                enabled: !root.locked

                ToolTip.text: item.description || item.value
                ToolTip.visible: hovered && ToolTip.text !== ""

                contentItem: Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: item.text || item.value
                    color: tagButton.hovered ? Theme.accentLight : Theme.textPrimary
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 7
                    color: tagButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.16) : Qt.rgba(1, 1, 1, 0.035)
                    border.color: tagButton.hovered ? Qt.rgba(0.49, 0.30, 1.0, 0.52) : Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                }

                onClicked: root.insertTag(item.value || item.text || "")
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }
    }
}
