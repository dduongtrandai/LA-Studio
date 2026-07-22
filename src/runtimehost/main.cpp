#include "RuntimeHostServer.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>

using namespace LAStudio;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LAStudioRuntimeHost"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption socketOption(QStringLiteral("socket"), QStringLiteral("Local socket name."), QStringLiteral("name"));
    QCommandLineOption tokenOption(QStringLiteral("token"), QStringLiteral("One-time authentication token."), QStringLiteral("token"));
    parser.addOption(socketOption);
    parser.addOption(tokenOption);
    parser.process(app);

    RuntimeHostServer server(parser.value(socketOption), parser.value(tokenOption));
    QString error;
    if (!server.start(&error)) {
        qCritical().noquote() << error;
        return 2;
    }
    return app.exec();
}
