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

    ../src/api/ApiServerService.cpp

    ../src/controllers/FileAccessService.cpp
    ../src/controllers/AppUpdateService.cpp
    ../src/controllers/ExampleManager.cpp
    ../src/controllers/DownloadInstallService.cpp
    ../src/controllers/AlignmentExecutionService.cpp
    ../src/controllers/AlignmentRefinementService.cpp
    ../src/controllers/DubbingSegmentNormalizer.cpp
    ../src/controllers/AlignmentTranscriptMatcher.cpp
    ../src/controllers/CrispAlignmentInterface.cpp
    ../src/translation/CrispTranslationInterface.cpp
    ../src/controllers/ModelsPathMigrator.cpp
    ../src/controllers/ModelsPathMigrationService.cpp
    ../src/controllers/HistoryRepository.cpp
    ../src/controllers/HistoryService.cpp
    ../src/controllers/AudioPreviewService.cpp
    ../src/controllers/StudioActions.cpp
    ../src/controllers/VoiceClonePresetService.cpp
    ../src/controllers/VoiceDesignPresetService.cpp
    ../src/controllers/AlignmentModelSession.cpp
    ../src/controllers/TranslationModelSession.cpp
    ../src/translation/TranslationProject.cpp
    ../src/translation/TranslationService.cpp
    ../src/controllers/TranslationController.cpp
    ../src/controllers/VoiceIsolatorModelSession.cpp
    ../src/controllers/ModelLifecycleController.cpp
    ../src/controllers/SttModelSession.cpp
    ../src/controllers/TtsSharedModelSession.cpp
    ../src/controllers/ModelSessionRegistry.cpp
    ../src/controllers/StudioConfigurationResolver.cpp
    ../src/controllers/SttSessionController.cpp
    ../src/controllers/SttAudioDecoder.cpp
    ../src/controllers/DubbingProject.cpp
    ../src/controllers/DubbingController.cpp
    ../src/controllers/AudioTimelineMixer.cpp
    ../src/controllers/DubbingJobRunner.cpp
    ../src/translation/DubbingTranslationService.cpp
    ../src/controllers/MediaToolService.cpp
    ../src/controllers/MediaIngestService.cpp
    ../src/separation/SeparationAudioIO.cpp
    ../src/separation/SourceSeparationService.cpp
    ../src/separation/SeparationWorker.cpp
    ../src/separation/backends/SeparationBackendFactory.cpp
    ../src/separation/backends/sherpa/SherpaOnnxRuntime.cpp
    ../src/separation/backends/sherpa/SherpaOnnxSeparationBackend.cpp
    ../src/controllers/VoiceIsolatorController.cpp
    ../src/controllers/WorkflowActivityManager.cpp
    ../src/workflows/WorkflowTypes.cpp
    ../src/workflows/WorkflowSession.cpp
    ../src/workflows/WorkflowGraph.cpp
    ../src/workflows/DubbingWorkflowDefinition.cpp
    ../src/workflows/WorkflowArtifact.cpp
    ../src/workflows/WorkflowRun.cpp
    ../src/workflows/WorkflowRunJournal.cpp
    ../src/workflows/WorkflowTranscript.cpp
    ../src/workflows/DubbingWorkflowNodes.cpp
    ../src/workflows/DubbingWorkflowAdapter.cpp
    ../src/workflows/NodeRegistry.cpp
    ../src/workflows/WorkflowGraphRunner.cpp
    ../src/workflows/AlignmentWorkflowResolver.cpp
    ../src/core/LogViewService.cpp
    ../src/core/PathUtils.cpp
    ../src/core/Logger.cpp
    ../src/core/InferenceThreadPolicy.cpp
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
    ../src/controllers/AppController.h
    ../src/controllers/AppUpdateService.h
    ../src/controllers/ExampleManager.h
    ../src/controllers/IStudioAction.h
    ../src/controllers/StudioActions.h
    ../src/controllers/IModelSession.h
    ../src/controllers/ModelLifecycleController.h
    ../src/controllers/SttModelSession.h
    ../src/controllers/TtsSharedModelSession.h
    ../src/controllers/AlignmentModelSession.h
    ../src/controllers/TranslationModelSession.h
    ../src/translation/TranslationProject.h
    ../src/translation/TranslationService.h
    ../src/controllers/TranslationController.h
    ../src/controllers/VoiceIsolatorModelSession.h
    ../src/controllers/ModelSessionRegistry.h
    ../src/controllers/WorkflowActivityManager.h
    ../src/workflows/WorkflowTypes.h
    ../src/workflows/IWorkflowResolver.h
    ../src/workflows/WorkflowSession.h
    ../src/workflows/WorkflowGraph.h
    ../src/workflows/DubbingWorkflowDefinition.h
    ../src/workflows/WorkflowArtifact.h
    ../src/workflows/WorkflowRun.h
    ../src/workflows/WorkflowRunJournal.h
    ../src/workflows/WorkflowTranscript.h
    ../src/workflows/DubbingWorkflowNodes.h
    ../src/workflows/WorkflowExecutionAdapter.h
    ../src/workflows/DubbingWorkflowAdapter.h
    ../src/workflows/NodeRegistry.h
    ../src/workflows/WorkflowGraphRunner.h
    ../src/workflows/AlignmentWorkflowResolver.h
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
    ../src/controllers/VoiceIsolatorController.h
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
    ${CMAKE_SOURCE_DIR}/src/separation
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
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
