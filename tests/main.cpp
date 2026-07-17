#include <QCoreApplication>
#include <QtTest>
#include <iostream>

#include "test_FileAccessService.h"
#include "test_ModelsPathMigration.h"
#include "test_History.h"
#include "test_DownloadInstallService.h"
#include "test_AudioPreviewService.h"
#include "test_ModelsAndRuntimes.h"
#include "test_SttSession.h"
#include "test_StudioCapabilities.h"
#include "test_AlignmentTranscriptMatcher.h"
#include "test_AlignmentWorkflow.h"
#include "test_DubbingProject.h"
#include "test_TranslationProject.h"
#include "test_WorkflowGraph.h"
#include "test_SourceSeparation.h"
#include "test_SubtitleVoice.h"

#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    // Ensure that application info is available for tests that use Settings/PathUtils
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LAStudioUnitTests"));
    app.setOrganizationName(QStringLiteral("LAStudio"));

    int status = 0;

    auto runSuite = [&status](QObject* testObj, const char* name) {
        std::cout << "\n==================================================\n";
        std::cout << "Running suite: " << name << "\n";
        std::cout << "==================================================\n";

        QString filename = QString("%1_results.txt").arg(name);
        QByteArray filenameBytes = filename.toLocal8Bit();
        char* fileArg = filenameBytes.data();

        char* localArgv[] = {
            const_cast<char*>("LAStudioUnitTests"),
            const_cast<char*>("-o"),
            fileArg,
            nullptr
        };
        int localArgc = 3;

        int suiteStatus = QTest::qExec(testObj, localArgc, localArgv);
        status |= suiteStatus;

        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                std::cout << in.readLine().toStdString() << "\n";
            }
            file.close();
            file.remove();
        } else {
            std::cout << "Failed to read test results file: " << name << "_results.txt\n";
        }
    };

    {
        LAStudio::TestFileAccessService suite;
        runSuite(&suite, "TestFileAccessService");
    }

    {
        LAStudio::TestModelsPathMigration suite;
        runSuite(&suite, "TestModelsPathMigration");
    }

    {
        LAStudio::TestHistory suite;
        runSuite(&suite, "TestHistory");
    }

    {
        LAStudio::TestDownloadInstallService suite;
        runSuite(&suite, "TestDownloadInstallService");
    }

    {
        LAStudio::TestAudioPreviewService suite;
        runSuite(&suite, "TestAudioPreviewService");
    }

    {
        LAStudio::TestModelsAndRuntimes suite;
        runSuite(&suite, "TestModelsAndRuntimes");
    }

    {
        LAStudio::TestSttSession suite;
        runSuite(&suite, "TestSttSession");
    }

    {
        LAStudio::TestStudioCapabilities suite;
        runSuite(&suite, "TestStudioCapabilities");
    }

    {
        LAStudio::TestAlignmentTranscriptMatcher suite;
        runSuite(&suite, "TestAlignmentTranscriptMatcher");
    }

    {
        LAStudio::TestAlignmentWorkflow suite;
        runSuite(&suite, "TestAlignmentWorkflow");
    }

    {
        LAStudio::TestDubbingProject suite;
        runSuite(&suite, "TestDubbingProject");
    }

    {
        LAStudio::TestTranslationProject suite;
        runSuite(&suite, "TestTranslationProject");
    }

    {
        LAStudio::TestWorkflowGraph suite;
        runSuite(&suite, "TestWorkflowGraph");
    }

    {
        LAStudio::TestSourceSeparation suite;
        runSuite(&suite, "TestSourceSeparation");
    }

    {
        LAStudio::TestSubtitleVoice suite;
        runSuite(&suite, "TestSubtitleVoice");
    }

    std::cout << "\n==================================================\n";
    std::cout << "All test suites completed with status code: " << status << "\n";
    std::cout << "==================================================\n";

    return status;
}
