cmake_minimum_required(VERSION 3.21)

# This is the single source of truth for the application version. Release
# automation may override it with -DLASTUDIO_VERSION=<major.minor.patch>.
set(LASTUDIO_VERSION "0.1.11" CACHE STRING "LA Studio application version")
if(NOT LASTUDIO_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "LASTUDIO_VERSION must use MAJOR.MINOR.PATCH format; got '${LASTUDIO_VERSION}'")
endif()

project(LAStudio VERSION ${LASTUDIO_VERSION} LANGUAGES CXX)

set(LLAMA_CPP_SOURCE_DIR "" CACHE PATH
    "llama.cpp checkout used only for public C API headers at build time")
if(NOT LLAMA_CPP_SOURCE_DIR)
    set(_llama_cpp_candidates
        "$ENV{LLAMA_CPP_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/.deps/llama.cpp"
        "${CMAKE_SOURCE_DIR}/../llama.cpp"
    )
    foreach(_candidate IN LISTS _llama_cpp_candidates)
        if(EXISTS "${_candidate}/include/llama.h" AND EXISTS "${_candidate}/ggml/include/ggml.h")
            set(LLAMA_CPP_SOURCE_DIR "${_candidate}" CACHE PATH
                "llama.cpp checkout used only for public C API headers at build time" FORCE)
            break()
        endif()
    endforeach()
    unset(_llama_cpp_candidates)
    unset(_candidate)
endif()
if(NOT EXISTS "${LLAMA_CPP_SOURCE_DIR}/include/llama.h" OR
   NOT EXISTS "${LLAMA_CPP_SOURCE_DIR}/ggml/include/ggml.h")
    message(FATAL_ERROR
        "LLAMA_CPP_SOURCE_DIR must point to a llama.cpp b10036 checkout compatible with the packaged runtime. "
        "Pass -DLLAMA_CPP_SOURCE_DIR=<path> or set the LLAMA_CPP_SOURCE_DIR environment variable.")
endif()

set(LASTUDIO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GLOB_RECURSE LASTUDIO_EXAMPLE_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "examples/*.json"
    "examples/*.md"
)
file(GLOB LASTUDIO_LANGUAGE_SET_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "data/language-sets/*.json"
)
file(MAKE_DIRECTORY "${LASTUDIO_GENERATED_DIR}/lastudio")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/AppVersion.h.in"
    "${LASTUDIO_GENERATED_DIR}/lastudio/AppVersion.h"
    @ONLY
)

include(CTest)
include(cmake/CompilerOptions.cmake)
message(STATUS "MSVC value: ${MSVC}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")

option(ENABLE_SHERPA_ONNX "Enable Sherpa-ONNX support" OFF)
set(SHERPA_ONNX_ROOT "" CACHE PATH "Optional sherpa-onnx checkout or install root used for source separation development")
if(NOT SHERPA_ONNX_ROOT AND DEFINED ENV{SHERPA_ONNX_ROOT})
    set(SHERPA_ONNX_ROOT "$ENV{SHERPA_ONNX_ROOT}" CACHE PATH "Optional sherpa-onnx checkout or install root used for source separation development" FORCE)
endif()
if(SHERPA_ONNX_ROOT)
    add_compile_definitions(LASTUDIO_SHERPA_ONNX_ROOT=\"${SHERPA_ONNX_ROOT}\")
    message(STATUS "Using sherpa-onnx development root: ${SHERPA_ONNX_ROOT}")
endif()
if(ENABLE_SHERPA_ONNX)
    add_compile_definitions(LA_ENABLE_SHERPA_ONNX)
endif()

option(ENABLE_OMNIVOICE "Enable Omnivoice support" ON)
if(ENABLE_OMNIVOICE)
    add_compile_definitions(LA_ENABLE_OMNIVOICE)
endif()

include(cmake/Dependencies.cmake)

# Require per-directory qmldir handling for Qt QML module sources.
qt_policy(SET QTP0004 NEW)

set(LASTUDIO_SOURCES
    src/main.cpp
    src/core/Logger.cpp
    src/core/PathUtils.cpp
    src/core/VoiceCloningUtils.cpp
    src/core/InferenceThreadPolicy.cpp
    src/inference/WorkerThreadHost.cpp
    src/translation/TranslationEngine.cpp
    src/translation/TranslationEngineInstance.cpp
    src/translation/TranslationWorker.cpp
    src/translation/backends/TranslationBackendFactory.cpp
    src/translation/backends/CrispTranslationBackend.cpp
    src/translation/backends/LlamaTranslationBackend.cpp
    src/core/Settings.cpp
    src/api/ApiServerService.cpp
    src/core/HFHubClient.cpp
    src/core/DownloadManager.cpp
    src/core/ModelManager.cpp
    src/core/CatalogManager.cpp
    src/core/RegistryManager.cpp
    src/core/RuntimeManager.cpp
    src/core/HardwareManager.cpp
    src/core/QmlLogger.cpp
    src/core/StudioCapabilityRegistry.cpp
    src/core/StudioSelectionRepository.cpp
    src/core/CapabilityFamilyModel.cpp
    src/core/LogViewService.cpp

    src/audio/WavIO.cpp
    src/audio/AudioRecorder.cpp
    src/audio/AudioPlayer.cpp
    src/audio/WaveformProvider.cpp
    src/stt/SttEngine.cpp
    src/stt/SttEngineInstance.cpp
    src/stt/SttWorker.cpp
    src/stt/backends/SttBackendFactory.cpp
    src/stt/backends/WhisperSttBackend.cpp
    src/stt/backends/QwenSttBackend.cpp
    src/stt/backends/NemotronSttBackend.cpp
    src/tts/TtsEngine.cpp
    src/tts/TtsEngineInstance.cpp
    src/tts/TtsWorker.cpp
    src/tts/TtsRequestValidator.cpp
    src/tts/backends/TtsBackendFactory.cpp
    src/tts/backends/KokoroBackend.cpp
    src/tts/backends/KokoroVietnameseBackend.cpp
    src/tts/backends/Qwen3Backend.cpp
    src/tts/backends/VoxCpm2Backend.cpp
    src/tts/backends/VibevoiceBackend.cpp
    src/tts/backends/VieneuTtsBackend.cpp
    src/tts/backends/OmnivoiceBackend.cpp
    src/controllers/app/AppController.cpp
    src/controllers/shared/AudioPreviewService.cpp
    src/controllers/shared/HistoryService.cpp
    src/controllers/shared/HistoryRepository.cpp
    src/controllers/shared/VoiceClonePresetService.cpp
    src/controllers/shared/VoiceDesignPresetService.cpp
    src/controllers/models/DownloadInstallService.cpp
    src/controllers/alignment/AlignmentExecutionService.cpp
    src/dubbing/AlignmentRefinementService.cpp
    src/dubbing/DubbingSegmentNormalizer.cpp
    src/controllers/alignment/AlignmentTranscriptMatcher.cpp
    src/alignment/CrispAlignmentInterface.cpp
    src/translation/CrispTranslationInterface.cpp
    src/translation/LlamaTranslationInterface.cpp
    src/controllers/models/ModelsPathMigrator.cpp
    src/controllers/models/ModelsPathMigrationService.cpp
    src/controllers/shared/FileAccessService.cpp
    src/controllers/shared/AppUpdateService.cpp
    src/controllers/shared/ExampleManager.cpp
    src/controllers/alignment/AlignmentModelSession.cpp
    src/controllers/translation/TranslationModelSession.cpp
    src/translation/TranslationProject.cpp
    src/translation/TranslationService.cpp
    src/controllers/translation/TranslationController.cpp
    src/controllers/separation/VoiceIsolatorModelSession.cpp
    src/controllers/app/StudioActions.cpp
    src/controllers/models/ModelLifecycleController.cpp
    src/controllers/stt/SttModelSession.cpp
    src/controllers/tts/TtsSharedModelSession.cpp
    src/controllers/models/ModelSessionRegistry.cpp
    src/controllers/app/StudioSessionViewModel.cpp
    src/controllers/models/StudioConfigurationResolver.cpp
    src/controllers/models/CapabilitySettingsSchema.cpp
    src/controllers/stt/SttSessionController.cpp
    src/controllers/tts/SubtitleVoiceController.cpp
    src/controllers/stt/SttAudioDecoder.cpp
    src/dubbing/DubbingProject.cpp
    src/dubbing/DubbingDuration.cpp
    src/dubbing/DubbingTimingProfile.cpp
    src/dubbing/DubbingTimingService.cpp
    src/controllers/dubbing/DubbingController.cpp
    src/dubbing/AudioTimelineMixer.cpp
    src/subtitles/SrtTimelineParser.cpp
    src/tts/SubtitleSmartFitPlanner.cpp
    src/tts/TimedSpeechPipeline.cpp
    src/audio/AudioTimelineRenderer.cpp
    src/controllers/dubbing/DubbingJobRunner.cpp
    src/separation/SeparationAudioIO.cpp
    src/separation/SourceSeparationService.cpp
    src/separation/SeparationWorker.cpp
    src/separation/backends/SeparationBackendFactory.cpp
    src/separation/backends/sherpa/SherpaOnnxRuntime.cpp
    src/separation/backends/sherpa/SherpaOnnxSeparationBackend.cpp
    src/controllers/separation/VoiceIsolatorController.cpp
    src/dubbing/DubbingTranslationService.cpp
    src/dubbing/media/MediaToolService.cpp
    src/dubbing/media/AtomicMediaCommit.cpp
    src/dubbing/media/MediaIngestService.cpp
    src/controllers/app/WorkflowActivityManager.cpp
    src/workflows/WorkflowTypes.cpp
    src/workflows/WorkflowSession.cpp
    src/workflows/WorkflowGraph.cpp
    src/dubbing/workflow/DubbingWorkflowDefinition.cpp
    src/workflows/WorkflowArtifact.cpp
    src/workflows/WorkflowRun.cpp
    src/workflows/WorkflowRunJournal.cpp
    src/workflows/WorkflowTranscript.cpp
    src/dubbing/workflow/DubbingWorkflowNodes.cpp
    src/dubbing/workflow/DubbingWorkflowAdapter.cpp
    src/workflows/NodeRegistry.cpp
    src/workflows/WorkflowGraphRunner.cpp
    src/alignment/AlignmentWorkflowResolver.cpp
    src/core/LocalizationManager.cpp
)

set(LASTUDIO_HEADERS
    src/core/Logger.h
    src/core/PathUtils.h
    src/core/VoiceCloningUtils.h
    src/core/InferenceThreadPolicy.h
    src/inference/InferenceTypes.h
    src/inference/InstanceRegistry.h
    src/inference/InferenceCancellation.h
    src/inference/WorkerThreadHost.h
    src/core/Settings.h
    src/core/HFHubClient.h
    src/core/DownloadManager.h
    src/core/ModelManager.h
    src/core/CatalogManager.h
    src/core/RegistryManager.h
    include/runtimes/CrispKokoroInterface.h
    include/runtimes/KokoroVietnameseInterface.h
    include/runtimes/CrispVoxCpm2Interface.h
    include/runtimes/CrispVibeVoiceInterface.h
    include/runtimes/VieneuTtsInterface.h
    include/runtimes/CrispQwen3SttInterface.h
    include/runtimes/CrispNemotronSttInterface.h
    src/core/RuntimeManager.h
    src/core/HardwareManager.h
    src/core/QmlLogger.h
    src/core/StudioCapabilityDescriptor.h
    src/core/StudioCapabilityRegistry.h
    src/core/StudioSelectionRepository.h
    src/core/CapabilityFamilyModel.h
    src/core/LogViewService.h
    src/api/ApiServerService.h

    src/audio/WavIO.h
    src/audio/AudioRecorder.h
    src/audio/AudioPlayer.h
    src/audio/WaveformProvider.h
    src/stt/SttEngine.h
    src/stt/SttEngineInstance.h
    src/stt/SttWorker.h
    src/stt/backends/SttBackend.h
    src/stt/backends/SttBackendFactory.h
    src/stt/backends/WhisperSttBackend.h
    src/stt/backends/QwenSttBackend.h
    src/stt/backends/NemotronSttBackend.h
    src/tts/TtsEngine.h
    src/tts/TtsEngineInstance.h
    src/tts/TtsWorker.h
    src/tts/TtsRequestValidator.h
    src/tts/backends/TtsBackend.h
    src/tts/backends/TtsBackendFactory.h
    src/tts/backends/KokoroBackend.h
    src/tts/backends/KokoroVietnameseBackend.h
    src/tts/backends/Qwen3Backend.h
    src/tts/backends/VoxCpm2Backend.h
    src/tts/backends/VibevoiceBackend.h
    src/tts/backends/VieneuTtsBackend.h
    src/tts/backends/OmnivoiceBackend.h
    src/controllers/app/AppController.h
    src/controllers/shared/AudioPreviewService.h
    src/controllers/shared/HistoryService.h
    src/controllers/shared/HistoryRepository.h
    src/controllers/shared/VoiceClonePresetService.h
    src/controllers/shared/VoiceDesignPresetService.h
    src/controllers/models/DownloadInstallService.h
    src/controllers/alignment/AlignmentExecutionService.h
    src/dubbing/AlignmentRefinementService.h
    src/dubbing/DubbingSegmentNormalizer.h
    src/controllers/alignment/AlignmentTranscriptMatcher.h
    include/runtimes/CrispAlignmentInterface.h
    include/runtimes/CrispTranslationInterface.h
    include/runtimes/LlamaTranslationInterface.h
    src/controllers/models/ModelsPathMigrator.h
    src/controllers/models/ModelsPathMigrationService.h
    src/controllers/shared/FileAccessService.h
    src/controllers/shared/AppUpdateService.h
    src/controllers/shared/ExampleManager.h
    src/controllers/alignment/AlignmentModelSession.h
    src/controllers/translation/TranslationModelSession.h
    src/translation/TranslationProject.h
    src/translation/TranslationEngine.h
    src/translation/TranslationEngineInstance.h
    src/translation/TranslationWorker.h
    src/translation/backends/TranslationBackend.h
    src/translation/backends/TranslationBackendFactory.h
    src/translation/backends/CrispTranslationBackend.h
    src/translation/backends/LlamaTranslationBackend.h
    src/translation/TranslationService.h
    src/controllers/translation/TranslationController.h
    src/controllers/separation/VoiceIsolatorModelSession.h
    src/controllers/app/IStudioAction.h
    src/controllers/app/StudioActions.h
    src/controllers/models/IModelSession.h
    src/controllers/models/ModelLifecycleController.h
    src/controllers/stt/SttModelSession.h
    src/controllers/tts/TtsSharedModelSession.h
    src/controllers/models/ModelSessionRegistry.h
    src/controllers/app/StudioSessionViewModel.h
    src/controllers/models/StudioConfigurationResolver.h
    src/controllers/models/CapabilitySettingsSchema.h
    src/controllers/stt/SttSessionController.h
    src/controllers/tts/SubtitleVoiceController.h
    src/controllers/stt/SttAudioDecoder.h
    src/dubbing/DubbingProject.h
    src/dubbing/DubbingTimingService.h
    src/controllers/dubbing/DubbingController.h
    src/dubbing/AudioTimelineMixer.h
    src/subtitles/TimedTextCue.h
    src/subtitles/SrtTimelineParser.h
    src/tts/SubtitleSmartFitPlanner.h
    src/tts/TimedSpeechPipeline.h
    src/audio/AudioTimelineRenderer.h
    src/controllers/dubbing/DubbingJobRunner.h
    src/separation/SeparationTypes.h
    src/separation/SeparationAudioIO.h
    src/separation/SourceSeparationService.h
    src/separation/SeparationWorker.h
    src/separation/backends/SeparationBackend.h
    src/separation/backends/SeparationBackendFactory.h
    src/separation/backends/sherpa/SherpaOnnxRuntime.h
    src/separation/backends/sherpa/SherpaOnnxSeparationBackend.h
    src/controllers/separation/VoiceIsolatorController.h
    src/dubbing/DubbingTranslationService.h
    src/dubbing/media/MediaToolService.h
    src/dubbing/media/AtomicMediaCommit.h
    src/dubbing/media/MediaIngestService.h
    src/controllers/app/WorkflowActivityManager.h
    src/workflows/WorkflowTypes.h
    src/workflows/IWorkflowResolver.h
    src/workflows/WorkflowSession.h
    src/workflows/WorkflowGraph.h
    src/dubbing/workflow/DubbingWorkflowDefinition.h
    src/workflows/WorkflowArtifact.h
    src/workflows/WorkflowRun.h
    src/workflows/WorkflowRunJournal.h
    src/workflows/WorkflowTranscript.h
    src/dubbing/workflow/DubbingWorkflowNodes.h
    src/workflows/WorkflowExecutionAdapter.h
    src/dubbing/workflow/DubbingWorkflowAdapter.h
    src/workflows/NodeRegistry.h
    src/workflows/WorkflowGraphRunner.h
    src/alignment/AlignmentWorkflowResolver.h
    src/core/LocalizationManager.h
)

qt_add_executable(LAStudio ${LASTUDIO_SOURCES} ${LASTUDIO_HEADERS})

qt_add_translations(LAStudio
    TS_FILES
        i18n/lastudio_en.ts
        i18n/lastudio_vi.ts
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/shared/StudioRouteRegistry.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(LAStudio
    URI LAStudio
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/pages/HFExplorerPage.qml
        qml/pages/DeveloperPage.qml
        qml/pages/ModelsPage.qml
        qml/pages/MyModelsPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/AlignmentPage.qml
        qml/pages/TranslationPage.qml
        qml/pages/SttPage.qml
        qml/pages/TtsPage.qml
        qml/pages/VoiceCloningPage.qml
        qml/pages/VoiceDesignPage.qml
        qml/pages/VoiceIsolatorPage.qml
        qml/pages/DubbingPage.qml
        qml/pages/WelcomePage.qml
        qml/pages/settings/GeneralSettingsTab.qml
        qml/pages/settings/RuntimeSettingsTab.qml
        qml/pages/settings/HardwareSettingsTab.qml

        qml/components/Sidebar.qml
        qml/components/AppTitleBar.qml
        qml/components/AudioDropArea.qml
        qml/components/WaveformView.qml
        qml/components/RuntimeVersionManagerDialog.qml
        qml/components/base/AppComboBox.qml
        qml/components/base/AppContextMenu.qml
        qml/components/base/ConfirmationDialog.qml
        qml/components/base/ModelDependenciesDialog.qml
        qml/components/base/AppTabButton.qml
        qml/components/base/AppTextArea.qml
        qml/components/base/AppToolTip.qml
        qml/components/base/LineIcon.qml
        qml/components/PrimaryButton.qml
        qml/components/SearchBar.qml
        qml/components/ModelCard.qml
        qml/components/ModelListItem.qml
        qml/components/ModelDetailsView.qml
        qml/components/DownloadProgressBar.qml
        qml/components/DownloadsPopup.qml
        qml/components/WorkflowPopup.qml
        qml/components/CommunityDialog.qml
        qml/components/BottomLogPanel.qml
        qml/components/alignment/AlignmentStudioView.qml
        qml/components/translation/TranslationStudioView.qml
        qml/components/alignment/AlignmentSectionHeader.qml
        qml/components/alignment/AlignmentOptionField.qml
        qml/components/alignment/AlignmentStatusStrip.qml
        qml/components/alignment/AlignmentStatusRow.qml
        qml/components/alignment/AlignmentSetupPanel.qml
        qml/components/alignment/AlignmentViewModeButton.qml
        qml/components/alignment/AlignmentPlayerButton.qml
        qml/components/alignment/AlignmentResultMetric.qml
        qml/components/dubbing/DubbingExportDialog.qml
        qml/components/dubbing/DubbingHistoryPanel.qml
        qml/components/dubbing/DubbingProjectStatusPanel.qml
        qml/components/dubbing/DubbingNodeSettingsPanel.qml
        qml/components/dubbing/DubbingNodeInspector.qml
        qml/components/dubbing/DubbingWorkflowStep.qml
        qml/components/dubbing/DubbingWorkflowHeader.qml
        qml/components/dubbing/DubbingSourceMediaPanel.qml
        qml/components/dubbing/DubbingVoiceClipReview.qml
        qml/components/shared/WorkflowPipelineDialog.qml
        qml/components/shared/WorkflowNodeModelDialog.qml
        qml/components/voicecloning/VoiceSettingsPanel.qml
        qml/components/voicecloning/VoiceCloningStudioView.qml
        qml/components/voicecloning/ReferenceInputBox.qml
        qml/components/voicecloning/InputSourceTabs.qml
        qml/components/voicecloning/AudioDisplayBox.qml
        qml/components/voicecloning/ModelSelectorPopup.qml
        qml/components/voicecloning/VoiceCloningSetupPanel.qml
        qml/components/shared/StudioHeaderBar.qml
        qml/components/shared/GeneratedAudioOutput.qml
        qml/components/shared/VoiceSeparationOutput.qml
        qml/components/shared/LoadedModelDialog.qml
        qml/components/shared/ExamplePickerDialog.qml
        qml/components/shared/CapabilityGallery.qml
        qml/components/shared/StudioContext.qml
        qml/components/shared/StudioShell.qml
        qml/components/shared/StudioPageFrame.qml
        qml/components/shared/WorkflowStudioShell.qml
        qml/components/shared/StudioRouteRegistry.qml
        qml/components/shared/VoiceLibraryDialog.qml
        qml/components/shared/PromptTagPicker.qml
        qml/components/shared/AudioInputSourcePicker.qml
        qml/components/shared/MediaInputSourcePicker.qml
        qml/components/shared/settings/CollapsibleSettingsSection.qml
        qml/components/shared/settings/LanguageSelector.qml
        qml/components/shared/settings/ModelParameterControls.qml
        qml/components/shared/settings/SettingsSection.qml
        qml/components/shared/settings/ParameterSlider.qml
        qml/components/shared/settings/ToggleRow.qml
        qml/components/shared/settings/FieldLabel.qml
        qml/components/shared/settings/SettingsCard.qml
        qml/components/shared/settings/SettingRow.qml
        qml/components/tts/TtsHistoryPanel.qml
        qml/components/tts/TtsSettingsPanel.qml
        qml/components/tts/TtsStudioView.qml
        qml/components/tts/SrtVoiceView.qml
        qml/components/voiceisolator/VoiceIsolatorHistoryPanel.qml
        qml/components/voiceisolator/VoiceIsolatorStudioView.qml
        qml/components/voicedesign/VoiceDesignHistoryPanel.qml
        qml/components/voicedesign/VoiceDesignSettingsPanel.qml
        qml/components/voicedesign/VoiceDesignStudioView.qml
        qml/components/voicedesign/VoicePresetPanel.qml
        qml/components/stt/SttHistoryPanel.qml
        qml/components/stt/SttInputSection.qml
        qml/components/stt/SttTranscriptionView.qml
        qml/components/stt/SttStudioView.qml
        qml/components/stt/SttSettingsPanel.qml
    RESOURCES
        data/catalog.json
        data/registry_schema.sql
        ${LASTUDIO_LANGUAGE_SET_FILES}
        data/vieneu/voices_v3_turbo.json
        ${LASTUDIO_EXAMPLE_FILES}
        icons/app_icon.ico
        icons/app_icon_16.png
        icons/app_icon_32.png
        icons/app_icon_48.png
        icons/app_icon_128.png
        icons/app_icon_256.png
    RESOURCE_PREFIX /
)

target_include_directories(LAStudio PRIVATE
    ${LASTUDIO_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/stt
    ${CMAKE_SOURCE_DIR}/src/tts
    ${CMAKE_SOURCE_DIR}/src/controllers
    ${CMAKE_SOURCE_DIR}/src/controllers/app
    ${CMAKE_SOURCE_DIR}/src/controllers/alignment
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing
    ${CMAKE_SOURCE_DIR}/src/controllers/stt
    ${CMAKE_SOURCE_DIR}/src/controllers/tts
    ${CMAKE_SOURCE_DIR}/src/controllers/translation
    ${CMAKE_SOURCE_DIR}/src/controllers/separation
    ${CMAKE_SOURCE_DIR}/src/controllers/models
    ${CMAKE_SOURCE_DIR}/src/controllers/shared
    ${CMAKE_SOURCE_DIR}/src/dubbing
    ${CMAKE_SOURCE_DIR}/src/dubbing/media
    ${CMAKE_SOURCE_DIR}/src/dubbing/workflow
    ${CMAKE_SOURCE_DIR}/src/alignment
    ${CMAKE_SOURCE_DIR}/src/separation
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/whisper/include
    ${LLAMA_CPP_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/ggml/include
)

target_compile_definitions(LAStudio PRIVATE
    LASTUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(ENABLE_OMNIVOICE)
    target_include_directories(LAStudio PRIVATE
        ${CMAKE_SOURCE_DIR}/omnivoice.cpp/src
    )
endif()

target_link_libraries(LAStudio PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    CURL::libcurl
    ZLIB::ZLIB
)

if(WIN32)
    target_link_libraries(LAStudio PRIVATE shell32)
endif()

# Unit tests build.
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WIN32)
    target_compile_definitions(LAStudio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(LAStudio PRIVATE dxgi)
endif()



# Omnivoice is loaded dynamically at runtime via OmnivoiceInterface

set_target_properties(LAStudio PROPERTIES
    OUTPUT_NAME "LA Studio"
    MACOSX_BUNDLE TRUE
)

if(WIN32)
    # Keep a console in Debug builds for dev logging, but hide it in all non-Debug builds.
    set_target_properties(LAStudio PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Debug>,FALSE,TRUE>
    )

    # If an .rc resource exists (and an .ico) then add it so the built .exe has an embedded icon
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/app_icon.rc")
        message(STATUS "Found resources/app_icon.rc — adding to LAStudio sources for Windows exe icon")
        target_sources(LAStudio PRIVATE "${CMAKE_SOURCE_DIR}/resources/app_icon.rc")
    else()
        message(STATUS "No resources/app_icon.rc found — skipping .exe icon embedding")
    endif()
endif()

# --- Installation Rules ---
include(GNUInstallDirs)

if(WIN32)
    # Keep installer metadata synchronized with the executable at configure time.
    file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" LASTUDIO_INSTALLER_STAGE_DIR)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/out" LASTUDIO_INSTALLER_OUTPUT_DIR)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/icons/app_icon.ico" LASTUDIO_INSTALLER_ICON_FILE)
    configure_file(
        "${CMAKE_SOURCE_DIR}/scripts/installer.iss.in"
        "${CMAKE_BINARY_DIR}/installer.iss"
        @ONLY
    )
endif()

# Install the executable
install(TARGETS LAStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install resources (if any outside of QML resources)
if(EXISTS "${CMAKE_SOURCE_DIR}/resources")
    install(DIRECTORY resources DESTINATION .)
endif()

# Note: Runtime DLLs (Whisper, etc.) and windeployqt will be handled 
# by the CI/CD script or a post-install step.

