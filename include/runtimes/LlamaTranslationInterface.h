#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <atomic>
#include <memory>

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

    QStringList translateBatch(const QStringList &texts,
                               const QString &sourceLanguage,
                               const QString &targetLanguage,
                               int maxTokens,
                               const std::shared_ptr<std::atomic_bool> &cancelToken,
                               QString *error = nullptr,
                               const QString &task = QStringLiteral("translate"),
                               const QVariantList &segments = QVariantList());

private:
    struct Api;
    void setError(const QString &message, QString *error);

    std::unique_ptr<Api> m_api;
    QString m_modelPath;
    QString m_error;
    std::atomic_bool m_cancelled{false};
};

} // namespace LAStudio
