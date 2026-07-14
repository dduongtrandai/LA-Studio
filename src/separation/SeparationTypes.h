#pragma once

#include <QString>
#include <QMap>
#include <QVariantMap>
#include <QList>
#include <QMetaType>
#include <functional>

namespace LAStudio {

struct SeparationConfiguration {
    QString backendId;
    QString pipelineProfile;
    QString runtimeId;
    QString runtimeVersion;
    QString runtimePath;
    QString familyId;
    QString configurationSignature;
    QMap<QString, QString> modelFilesByRole;
    QVariantMap backendOptions;
};

struct SeparationRequest {
    QString sourcePath;
    QString outputRoot;
    SeparationConfiguration configuration;
    int numThreads = 1;
};

struct SeparationStem {
    QString id;
    QString path;
    int sampleRate = 0;
    int channels = 0;
};

enum class SeparationErrorCode {
    None,
    InvalidRequest,
    Busy,
    Cancelled,
    DecodeFailed,
    UnsupportedBackend,
    RuntimeLoadFailed,
    InferenceFailed,
    OutputFailed
};

struct SeparationResult {
    bool success = false;
    bool cacheHit = false;
    QString sourceHash;
    QString cacheKey;
    QList<SeparationStem> stems;
    SeparationErrorCode errorCode = SeparationErrorCode::None;
    QString error;
};

struct CancellationToken {
    std::function<bool()> isCancelled = []() { return false; };
};

} // namespace LAStudio

Q_DECLARE_METATYPE(LAStudio::SeparationConfiguration)
Q_DECLARE_METATYPE(LAStudio::SeparationRequest)
Q_DECLARE_METATYPE(LAStudio::SeparationStem)
Q_DECLARE_METATYPE(LAStudio::SeparationResult)
Q_DECLARE_METATYPE(LAStudio::SeparationErrorCode)
