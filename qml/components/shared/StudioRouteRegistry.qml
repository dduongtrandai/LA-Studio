pragma Singleton
import QtQuick

QtObject {
    id: root

    // Mapping route IDs to StackLayout indexes
    readonly property var routeMap: {
        "welcome": 0,
        "studio-stt": 1,
        "studio-tts": 2,
        "studio-voice-cloning": 3,
        "studio-voice-design": 4,
        "studio-voice-isolator": 5,
        "studio-alignment": 6,
        "studio-translation": 7,
        "studio-dubbing": 8,
        "models": 9,
        "my-models": 10,
        "developer": 11,
        "settings": 12,
        "tools-alignment": 6
    }

    // List of route descriptors for Sidebar / navigation
    readonly property var routes: [
        { id: "welcome", label: qsTr("Home"), iconName: "home" },
        { id: "studio-stt", label: qsTr("Speech to Text"), iconName: "mic" },
        { id: "studio-tts", label: qsTr("Text to Speech"), iconName: "volume" },
        { id: "studio-voice-cloning", label: qsTr("Voice Cloning"), iconName: "spark" },
        { id: "studio-voice-design", label: qsTr("Voice Design"), iconName: "waves" },
        { id: "studio-voice-isolator", label: qsTr("Voice Isolator"), iconName: "voice-isolator" },
        { id: "studio-alignment", label: qsTr("Alignment"), iconName: "alignment" },
        { id: "studio-translation", label: qsTr("Translation"), iconName: "translate" },
        { id: "studio-dubbing", label: qsTr("Dubbing"), iconName: "dubbing" },
        { id: "models", label: qsTr("Models"), iconName: "gallery" },
        { id: "my-models", label: qsTr("My Models"), iconName: "folder" },
        { id: "developer", label: qsTr("Developer"), iconName: "code" },
        { id: "settings", label: qsTr("Settings"), iconName: "settings" }
    ]

    readonly property var capabilityRouteMap: {
        "stt": "studio-stt",
        "tts": "studio-tts",
        "voice-cloning": "studio-voice-cloning",
        "voice-design": "studio-voice-design",
        "forced-alignment": "studio-alignment",
        "translation": "studio-translation",
        "dubbing": "studio-dubbing",
        "voice-isolation": "studio-voice-isolator"
    }

    function getIndex(routeId) {
        var idx = routeMap[routeId]
        return idx !== undefined ? idx : 0
    }

    function getRouteId(index) {
        for (var key in routeMap) {
            if (routeMap[key] === index) return key
        }
        return "welcome"
    }

    function routeForCapability(capabilityId) {
        var routeId = capabilityRouteMap[capabilityId]
        return routeId !== undefined ? routeId : "welcome"
    }
}
