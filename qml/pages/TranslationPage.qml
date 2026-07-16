import QtQuick
import "../components/shared"
import "../components/translation"
import LAStudio

StudioPageFrame {
    id: translationPageFrame
    capabilityId: "translation"

    contentView: Component {
        TranslationStudioView {
            studioController: translationPageFrame.studioController
            onBackToGallery: translationPageFrame.openConfiguration(studioController ? studioController.selectedFamilyId : "")
        }
    }
}
