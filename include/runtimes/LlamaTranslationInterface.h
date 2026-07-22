#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <atomic>
#include <memory>
#include <functional>

namespace LAStudio {

// Dynamically loads the public llama.cpp C ABI from the official release.
// llama.cpp headers are a build-time SDK dependency only; runtime binaries are
// downloaded and managed by LA Studio.
class LlamaTranslationInterface final
{
public:
    LlamaTranslationInterface();
    ~LlamaTranslationInterface();

    bool load(const QString &libraryPath,
              const QString &modelPath,
              QString *error = nullptr,
              bool useGpu = false);
    void unload();
    void cancel();
    bool isLoaded() const;

    static bool parseContextTranslation(const QString &output, int expectedCount,
                                        QStringList *translations);

    QStringList translateBatch(const QStringList &texts,
                               const QString &sourceLanguage,
                               const QString &targetLanguage,
                               int maxTokens,
                               const std::shared_ptr<std::atomic_bool> &cancelToken,
                               QString *error = nullptr,
                               const QString &task = QStringLiteral("translate"),
                               const QVariantList &segments = QVariantList());

    using ChatTokenCallback = std::function<void(const QString &)>;
    bool generateChat(const QList<QVariantMap> &messages,
                      int contextTokens,
                      int maxTokens,
                      float temperature,
                      float topP,
                      int topK,
                      float repeatPenalty,
                      const std::shared_ptr<std::atomic_bool> &cancelToken,
                      const ChatTokenCallback &onToken,
                      QString *fullText = nullptr,
                      QString *error = nullptr);

private:
    struct Api;
    void setError(const QString &message, QString *error);

    std::unique_ptr<Api> m_api;
    QString m_modelPath;
    QString m_error;
    std::atomic_bool m_cancelled{false};
};

} // namespace LAStudio
