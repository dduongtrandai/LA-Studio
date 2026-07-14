import QtQuick
import QtQuick.Controls
import "../components/shared"
import "../components/voicecloning"

StudioPageFrame {
    id: cloningPageFrame
    capabilityId: "voice-cloning"

    contentView: Component {
        VoiceCloningStudioView {
            studioController: cloningPageFrame.studioController
            family: {
                var fams = studioController.families
                for (var i = 0; i < fams.length; i++) {
                    if (fams[i].id === studioController.selectedFamilyId) return fams[i]
                }
                return null
            }
            families: studioController.families
            selectedFamilyId: studioController.selectedFamilyId
            studioReady: studioController.studioReady
            studioTitle: studioController.studioHeaderTitle
            modalSelectionTitle: studioController.modalSelectionTitle
            modalSelectionValue: studioController.modalSelectionValue
            modalSelectionDetail: studioController.modalSelectionDetail
            onBackToGallery: cloningPageFrame.openConfiguration(selectedFamilyId)
            onReloadRequested: studioController.reload()
            onEjectRequested: studioController.unload()
            onModelSwitchRequested: function(nextFamilyId) {
                studioController.selectFamily(nextFamilyId)
                studioController.commitSelection()
            }
            onRuntimeSwitchRequested: function(nextRuntimeId) {
                studioController.selectRuntime(nextRuntimeId, "")
                studioController.commitSelection()
            }
        }
    }
}
