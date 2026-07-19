find_package(Qt6 REQUIRED COMPONENTS Core Test Concurrent Sql Network Multimedia Quick)
find_package(CURL CONFIG REQUIRED)

set(TEST_SOURCES
    main.cpp
    test_FileAccessService.cpp
    test_ModelsPathMigration.cpp
    test_History.cpp
    test_DownloadInstallService.cpp
    test_AudioPreviewService.cpp
    test_ModelsAndRuntimes.cpp
    test_SttSession.cpp
    test_SubtitleVoice.cpp
    test_StudioCapabilities.cpp
    test_AlignmentTranscriptMatcher.cpp
    test_AlignmentWorkflow.cpp
    test_DubbingProject.cpp
    test_TranslationProject.cpp
    test_WorkflowGraph.cpp
    test_SourceSeparation.cpp
    mocks/SttEngine.cpp
    mocks/SttEngineInstance.cpp
    mocks/TtsEngine.cpp
    mocks/TtsEngineInstance.cpp
    mocks/AudioRecorder.cpp
    mocks/AudioPlayer.cpp
    mocks/HFHubClient.cpp
    mocks/DownloadManager.cpp
    mocks/AppController.cpp
    test_SubtitleVoice.h

    ../src/api/ApiServerService.cpp

    ../src/controllers/shared/FileAccessService.cpp
    ../src/controllers/shared/AppUpdateService.cpp
    ../src/controllers/shared/ExampleManager.cpp
    ../src/controllers/models/DownloadInstallService.cpp
    ../src/controllers/alignment/AlignmentExecutionService.cpp
    ../src/dubbing/AlignmentRefinementService.cpp
    ../src/dubbing/DubbingSegmentNormalizer.cpp
    ../src/controllers/alignment/AlignmentTranscriptMatcher.cpp
    ../src/alignment/CrispAlignmentInterface.cpp
    ../src/translation/CrispTranslationInterface.cpp
    ../src/translation/LlamaTranslationInterface.cpp
    ../src/controllers/models/ModelsPathMigrator.cpp
    ../src/controllers/models/ModelsPathMigrationService.cpp
    ../src/controllers/shared/HistoryRepository.cpp
    ../src/controllers/shared/HistoryService.cpp
    ../src/controllers/shared/AudioPreviewService.cpp
    ../src/controllers/app/StudioActions.cpp
    ../src/controllers/shared/VoiceClonePresetService.cpp
    ../src/controllers/shared/VoiceDesignPresetService.cpp
    ../src/controllers/alignment/AlignmentModelSession.cpp
    ../src/controllers/translation/TranslationModelSession.cpp
    ../src/translation/TranslationProject.cpp
    ../src/translation/TranslationEngine.cpp
    ../src/translation/TranslationEngineInstance.cpp
    ../src/translation/TranslationWorker.cpp
    ../src/translation/backends/TranslationBackendFactory.cpp
    ../src/translation/backends/CrispTranslationBackend.cpp
    ../src/translation/backends/LlamaTranslationBackend.cpp
    ../src/translation/TranslationService.cpp
    ../src/controllers/translation/TranslationController.cpp
    ../src/controllers/separation/VoiceIsolatorModelSession.cpp
    ../src/controllers/models/ModelLifecycleController.cpp
    ../src/controllers/stt/SttModelSession.cpp
    ../src/controllers/tts/TtsSharedModelSession.cpp
    ../src/controllers/models/ModelSessionRegistry.cpp
    ../src/controllers/models/StudioConfigurationResolver.cpp
    ../src/controllers/models/CapabilitySettingsSchema.cpp
    ../src/controllers/stt/SttSessionController.cpp
    ../src/controllers/tts/SubtitleVoiceController.cpp
    ../src/subtitles/SrtTimelineParser.cpp
    ../src/tts/SubtitleSmartFitPlanner.cpp
    ../src/tts/TimedSpeechPipeline.cpp
    ../src/audio/AudioTimelineRenderer.cpp
    ../src/controllers/stt/SttAudioDecoder.cpp
    ../src/dubbing/DubbingProject.cpp
    ../src/dubbing/DubbingDuration.cpp
    ../src/dubbing/DubbingTimingProfile.cpp
    ../src/controllers/dubbing/DubbingController.cpp
    ../src/dubbing/AudioTimelineMixer.cpp
    ../src/controllers/dubbing/DubbingJobRunner.cpp
    ../src/dubbing/DubbingTranslationService.cpp
    ../src/dubbing/media/MediaToolService.cpp
    ../src/dubbing/media/MediaIngestService.cpp
    ../src/separation/SeparationAudioIO.cpp
    ../src/separation/SourceSeparationService.cpp
    ../src/separation/SeparationWorker.cpp
    ../src/separation/backends/SeparationBackendFactory.cpp
    ../src/separation/backends/sherpa/SherpaOnnxRuntime.cpp
    ../src/separation/backends/sherpa/SherpaOnnxSeparationBackend.cpp
    ../src/controllers/separation/VoiceIsolatorController.cpp
    ../src/controllers/app/WorkflowActivityManager.cpp
    ../src/workflows/WorkflowTypes.cpp
    ../src/workflows/WorkflowSession.cpp
    ../src/workflows/WorkflowGraph.cpp
    ../src/dubbing/workflow/DubbingWorkflowDefinition.cpp
    ../src/workflows/WorkflowArtifact.cpp
    ../src/workflows/WorkflowRun.cpp
    ../src/workflows/WorkflowRunJournal.cpp
    ../src/workflows/WorkflowTranscript.cpp
    ../src/dubbing/workflow/DubbingWorkflowNodes.cpp
    ../src/dubbing/workflow/DubbingWorkflowAdapter.cpp
    ../src/workflows/NodeRegistry.cpp
    ../src/workflows/WorkflowGraphRunner.cpp
    ../src/alignment/AlignmentWorkflowResolver.cpp
    ../src/core/LogViewService.cpp
    ../src/core/PathUtils.cpp
    ../src/core/Logger.cpp
    ../src/core/InferenceThreadPolicy.cpp
    ../src/inference/WorkerThreadHost.cpp
    ../src/core/CapabilityFamilyModel.cpp
    ../src/core/HardwareManager.cpp
    ../src/core/StudioCapabilityRegistry.cpp
    ../src/core/StudioSelectionRepository.cpp
    ../src/core/Settings.cpp
    ../src/core/LocalizationManager.cpp
    ../src/core/ModelManager.cpp
    ../src/core/CatalogManager.cpp
    ../src/core/RegistryManager.cpp
    ../src/core/RuntimeManager.cpp
    ../src/core/QmlLogger.cpp
    ../src/audio/WavIO.cpp
    ../src/audio/WaveformProvider.cpp

    ../src/core/DownloadManager.h
    ../src/api/ApiServerService.h
    ../src/stt/SttEngine.h
    ../src/stt/SttEngineInstance.h
    ../src/tts/TtsEngine.h
    ../src/tts/TtsEngineInstance.h
    ../src/audio/AudioRecorder.h
    ../src/audio/AudioPlayer.h
    ../src/core/HFHubClient.h
    ../src/controllers/app/AppController.h
    ../src/controllers/shared/AppUpdateService.h
    ../src/controllers/shared/ExampleManager.h
    ../src/controllers/app/IStudioAction.h
    ../src/controllers/app/StudioActions.h
    ../src/controllers/models/IModelSession.h
    ../src/inference/WorkerThreadHost.h
    ../src/controllers/models/ModelLifecycleController.h
    ../src/controllers/stt/SttModelSession.h
    ../src/controllers/tts/TtsSharedModelSession.h
    ../src/controllers/alignment/AlignmentModelSession.h
    ../src/controllers/translation/TranslationModelSession.h
    ../src/translation/TranslationProject.h
    ../src/translation/TranslationEngine.h
    ../src/translation/TranslationEngineInstance.h
    ../src/translation/TranslationWorker.h
    ../src/translation/backends/TranslationBackend.h
    ../src/translation/backends/TranslationBackendFactory.h
    ../src/translation/backends/CrispTranslationBackend.h
    ../src/translation/backends/LlamaTranslationBackend.h
    ../src/translation/TranslationService.h
    ../src/controllers/translation/TranslationController.h
    ../src/controllers/separation/VoiceIsolatorModelSession.h
    ../src/controllers/models/ModelSessionRegistry.h
    ../src/controllers/app/WorkflowActivityManager.h
    ../src/workflows/WorkflowTypes.h
    ../src/workflows/IWorkflowResolver.h
    ../src/workflows/WorkflowSession.h
    ../src/workflows/WorkflowGraph.h
    ../src/dubbing/workflow/DubbingWorkflowDefinition.h
    ../src/workflows/WorkflowArtifact.h
    ../src/workflows/WorkflowRun.h
    ../src/workflows/WorkflowRunJournal.h
    ../src/workflows/WorkflowTranscript.h
    ../src/dubbing/workflow/DubbingWorkflowNodes.h
    ../src/workflows/WorkflowExecutionAdapter.h
    ../src/dubbing/workflow/DubbingWorkflowAdapter.h
    ../src/workflows/NodeRegistry.h
    ../src/workflows/WorkflowGraphRunner.h
    ../src/alignment/AlignmentWorkflowResolver.h
    ../include/runtimes/CrispTranslationInterface.h
    ../src/separation/SeparationTypes.h
    ../src/separation/SeparationAudioIO.h
    ../src/separation/SourceSeparationService.h
    ../src/separation/SeparationWorker.h
    ../src/separation/backends/SeparationBackend.h
    ../src/separation/backends/SeparationBackendFactory.h
    ../src/separation/backends/sherpa/SherpaOnnxRuntime.h
    ../src/separation/backends/sherpa/SherpaOnnxSeparationBackend.h
    test_SourceSeparation.h
    test_TranslationProject.h
    ../src/controllers/separation/VoiceIsolatorController.h
)

add_executable(LAStudioUnitTests ${TEST_SOURCES})

target_compile_definitions(LAStudioUnitTests PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    LASTUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_include_directories(LAStudioUnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/audio
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
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${LLAMA_CPP_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/ggml/include
)

target_link_libraries(LAStudioUnitTests PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Network
    Qt6::Multimedia
    Qt6::Quick
    CURL::libcurl
    dxgi
)

if(WIN32)
    target_link_libraries(LAStudioUnitTests PRIVATE shell32)
endif()

add_test(NAME LAStudioUnitTests COMMAND LAStudioUnitTests)

get_target_property(QT6_CORE_DLL Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT QT6_CORE_DLL)
    get_target_property(QT6_CORE_DLL Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(QT_BIN_DIR "${QT6_CORE_DLL}" DIRECTORY)
get_filename_component(QT_ROOT_DIR "${QT_BIN_DIR}" DIRECTORY)
set(QT_PLUGIN_DIR "${QT_ROOT_DIR}/plugins")

set_tests_properties(LAStudioUnitTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    ENVIRONMENT "QT_PLUGIN_PATH=${QT_PLUGIN_DIR};QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}"
)
