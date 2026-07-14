#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>
#include <QByteArray>

namespace LAStudio {

class HistoryRepository {
public:
    static QVariantList loadTtsHistory(const QString &dataDir);
    static bool addTtsHistoryItem(const QString &dataDir,
                                  const QString &text,
                                  const QString &modelName,
                                  const QString &voiceName,
                                  const QVector<float> &samples,
                                  int sampleRate,
                                  QString &errorMsg);
    static bool deleteTtsHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg);
    static bool clearTtsHistory(const QString &dataDir, QString &errorMsg);

    static QVariantList loadSttHistory(const QString &dataDir);
    static bool addSttHistoryItem(const QString &dataDir,
                                  const QString &text,
                                  const QString &modelName,
                                  const QVector<float> &samples,
                                  QString &errorMsg);
    static bool deleteSttHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg);
    static bool clearSttHistory(const QString &dataDir, QString &errorMsg);

    static QVariantList loadVoiceDesignHistory(const QString &dataDir);
    static bool addVoiceDesignHistoryItem(const QString &dataDir,
                                          const QString &text,
                                          const QString &voiceDescription,
                                          const QString &presetName,
                                          const QString &familyId,
                                          const QString &modelName,
                                          const QVector<float> &samples,
                                          int sampleRate,
                                          QString &errorMsg);
    static bool deleteVoiceDesignHistoryItem(const QString &dataDir, const QString &id, QString &errorMsg);
    static bool clearVoiceDesignHistory(const QString &dataDir, QString &errorMsg);
};

} // namespace LAStudio
