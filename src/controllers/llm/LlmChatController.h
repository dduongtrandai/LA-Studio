#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>

namespace LAStudio {
class LlmChatEngine;
class LlmChatModelSession;

class LlmChatController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LlmChatController is managed by AppController")
    Q_PROPERTY(QVariantList conversations READ conversations NOTIFY conversationsChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QString activeConversationId READ activeConversationId NOTIFY activeConversationChanged)
    Q_PROPERTY(bool generating READ generating NOTIFY generatingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString systemPrompt READ systemPrompt WRITE setSystemPrompt NOTIFY settingsChanged)
    Q_PROPERTY(int contextTokens READ contextTokens WRITE setContextTokens NOTIFY settingsChanged)
    Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY settingsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY settingsChanged)
    Q_PROPERTY(double topP READ topP WRITE setTopP NOTIFY settingsChanged)
    Q_PROPERTY(int topK READ topK WRITE setTopK NOTIFY settingsChanged)
    Q_PROPERTY(double repeatPenalty READ repeatPenalty WRITE setRepeatPenalty NOTIFY settingsChanged)
public:
    explicit LlmChatController(LlmChatEngine *engine, LlmChatModelSession *session, QObject *parent = nullptr);
    QVariantList conversations() const { return m_conversations; }
    QVariantList messages() const { return m_messages; }
    QString activeConversationId() const { return m_activeId; }
    bool generating() const { return m_generating; }
    QString errorText() const { return m_error; }
    QString systemPrompt() const { return m_systemPrompt; }
    int contextTokens() const { return m_contextTokens; }
    int maxTokens() const { return m_maxTokens; }
    double temperature() const { return m_temperature; }
    double topP() const { return m_topP; }
    int topK() const { return m_topK; }
    double repeatPenalty() const { return m_repeatPenalty; }
    void setSystemPrompt(const QString &value);
    void setContextTokens(int value);
    void setMaxTokens(int value);
    void setTemperature(double value);
    void setTopP(double value);
    void setTopK(int value);
    void setRepeatPenalty(double value);

    Q_INVOKABLE void newConversation();
    Q_INVOKABLE void selectConversation(const QString &id);
    Q_INVOKABLE void renameConversation(const QString &id, const QString &title);
    Q_INVOKABLE void deleteConversation(const QString &id);
    Q_INVOKABLE void clearConversation();
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void stopGeneration();
    Q_INVOKABLE void regenerateLastResponse();
    Q_INVOKABLE void copyMessage(const QString &text);

signals:
    void conversationsChanged();
    void messagesChanged();
    void activeConversationChanged();
    void generatingChanged();
    void errorTextChanged();
    void settingsChanged();
    void statusChanged();

private slots:
    void onToken(const QString &requestId, const QString &token);
    void onFinished(const QString &requestId, const QString &text);
    void onCancelled(const QString &requestId, const QString &text);
    void onEngineError(const QString &message);

private:
    void persist();
    void load();
    void setError(const QString &message);
    void setGenerating(bool value);
    QString newId() const;
    void ensureActive();

    LlmChatEngine *m_engine = nullptr;
    LlmChatModelSession *m_session = nullptr;
    QVariantList m_conversations;
    QVariantList m_messages;
    QString m_activeId;
    QString m_requestId;
    int m_streamingMessageIndex = -1;
    bool m_generating = false;
    QString m_error;
    QString m_systemPrompt;
    int m_contextTokens = 4096;
    int m_maxTokens = 1024;
    double m_temperature = 0.7;
    double m_topP = 0.8;
    int m_topK = 20;
    double m_repeatPenalty = 1.05;
};
} // namespace LAStudio
