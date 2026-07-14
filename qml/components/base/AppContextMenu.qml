import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

Menu {
    id: root

    // Public API: an array of objects { text, subText, iconName, enabled, action, type }
    property var items: []

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1,1,1,0.04)
        implicitWidth: 280

        // Simple shadow fallback
        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            color: "#00000014"
            radius: Theme.radiusMedium + 6
            z: -1
        }
    }

    Component {
        id: menuItemComp
        MenuItem {
            id: controlItem
            // Extra properties to receive data
            property string subText: ""
            property string iconName: ""
            property var itemAction: null

            background: Rectangle {
                implicitWidth: 260
                implicitHeight: 54
                color: controlItem.hovered || controlItem.highlighted || controlItem.pressed ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                radius: Theme.radiusSmall
                border.color: controlItem.hovered || controlItem.highlighted || controlItem.pressed ? Qt.rgba(255, 255, 255, 0.04) : "transparent"
                border.width: 1

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }
                Behavior on border.color {
                    ColorAnimation { duration: 120 }
                }
            }

            contentItem: RowLayout {
                spacing: Theme.paddingMedium
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium

                LineIcon { 
                    name: controlItem.iconName || ""; 
                    color: (controlItem.iconName && controlItem.iconName !== "") ? (controlItem.hovered || controlItem.highlighted ? "#ffffff" : Theme.accentLight) : Theme.textSecondary; 
                    Layout.preferredWidth: 18; 
                    Layout.preferredHeight: 18 
                    visible: !!name
                }

                ColumnLayout {
                    spacing: 2
                    Text { 
                        text: controlItem.text 
                        color: controlItem.hovered || controlItem.highlighted ? "#ffffff" : Theme.textPrimary 
                        font.pixelSize: Theme.fontMedium 
                        Layout.fillWidth: true 
                    }
                    Text { 
                        text: controlItem.subText 
                        color: controlItem.hovered || controlItem.highlighted ? Theme.textPrimary : Theme.textSecondary 
                        font.pixelSize: Theme.fontSmall 
                        visible: controlItem.subText !== "" 
                        Layout.fillWidth: true 
                    }
                }

                Item { Layout.fillWidth: true }
                LineIcon { name: "chevron-right"; color: Theme.textSecondary; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
            }

            onTriggered: {
                if (controlItem.itemAction) controlItem.itemAction()
            }
        }
    }

    Component {
        id: sepComp
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.surfaceAlt
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.paddingSmall
            }
        }
    }

    function rebuild() {
        // Clear all existing items
        while (root.count > 0) {
            var item = root.takeItem(0)
            if (item) item.destroy()
        }

        if (!items) return;

        for (var j = 0; j < items.length; ++j) {
            var it = items[j]
            if (!it) continue
            
            var obj
            if (it.type === "separator") {
                obj = sepComp.createObject(root.contentItem)
            } else {
                obj = menuItemComp.createObject(root.contentItem, { 
                    text: it.text || "", 
                    subText: it.subText || "", 
                    iconName: it.iconName || "", 
                    itemAction: it.action || null 
                })
                
                if (obj) {
                    // compute enabled, allow boolean or function
                    var en = true
                    if (typeof it.enabled === 'function') en = it.enabled()
                    else if (it.enabled !== undefined) en = !!it.enabled
                    obj.enabled = en
                }
            }
            
            if (obj) {
                root.addItem(obj)
            }
        }
    }

    Component.onCompleted: rebuild()
    onItemsChanged: rebuild()
}
