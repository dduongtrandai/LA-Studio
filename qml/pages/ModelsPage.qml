import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../components/shared"

Rectangle {
    id: root
    color: Theme.background

    signal openStudioRequested(string capability, string familyId)

    CapabilityGallery {
        anchors.fill: parent
        displayMode: "page"
        managementMode: true
        capability: "all"
        
        onOpenStudio: function(capability, familyId) {
            root.openStudioRequested(capability, familyId)
        }
    }
}
