import QtQuick

QtObject {
    id: root

    property string capability: ""
    property string familyId: ""
    property string runtimeId: ""
    property string runtimeVersion: ""

    function setSelection(nextFamilyId, nextRuntimeId, nextRuntimeVersion) {
        familyId = nextFamilyId || ""
        runtimeId = nextRuntimeId || ""
        runtimeVersion = nextRuntimeVersion || ""
    }
}
