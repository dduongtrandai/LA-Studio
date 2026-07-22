#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace LAStudio {

class DubbingTranslationFixService final : public QObject
{
    Q_OBJECT
public:
    explicit DubbingTranslationFixService(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    int progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }
    QString lastError() const { return m_lastError; }
    QVariantMap configuration() const { return m_configuration; }

    bool start(const QString &sourceLanguage, const QString &targetLanguage,
               const QVariantList &segments, const QVariantMap &configuration,
               int segmentIndex = -1);
    void testConnection(const QVariantMap &configuration);
    void cancel();
    void clearError();

    static int eligibleSegmentCount(const QVariantList &segments,
                                    const QString &targetLanguage);
    static QVariantMap normalizedConfiguration(const QVariantMap &configuration);
    static QUrl chatUrl(const QString &serverUrl);
    static QUrl modelsUrl(const QString &serverUrl);
    static QString cleanAssistantText(const QString &content);
    static bool isCloserToBudget(int currentPhonemes, int candidatePhonemes,
                                 int minimum, int maximum);

signals:
    void stateChanged();
    void completed(const QVariantList &segments, int fixedCount, int unresolvedCount);
    void failed(const QString &message);
    void connectionTested(bool success, const QString &message);

private:
    void saveConfiguration();
    void setBusy(bool busy);
    void setProgress(int progress);
    void setStatus(const QString &status);
    void setError(const QString &message);
    void beginSegment();
    void requestAttempt();
    void handleAttemptResponse(QNetworkReply *reply);
    void finishSegment(bool fixed, bool improved = false);
    void finishRun();
    QString buildPrompt(const QVariantMap &segment) const;
    QStringList protectedTokens(const QString &text) const;
    bool preservesProtectedTokens(const QString &candidate,
                                  const QStringList &tokens) const;
    void applyCandidate(QVariantMap &segment, const QString &candidate,
                        int phonemes, bool withinBudget) const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QVariantMap m_configuration;
    QVariantList m_segments;
    QList<int> m_eligibleIndices;
    QString m_sourceLanguage;
    QString m_targetLanguage;
    QString m_originalTranslation;
    QString m_promptTranslation;
    QString m_lastCandidate;
    QString m_bestCandidate;
    QSet<QString> m_seenCandidates;
    int m_lastCandidatePhonemes = 0;
    int m_bestCandidatePhonemes = 0;
    int m_promptPhonemes = 0;
    int m_segmentPosition = 0;
    int m_attempt = 0;
    int m_maxAttempts = 4;
    int m_fixedCount = 0;
    int m_improvedCount = 0;
    int m_unresolvedCount = 0;
    bool m_busy = false;
    bool m_testing = false;
    int m_progress = 0;
    QString m_statusText;
    QString m_lastError;
};

} // namespace LAStudio
