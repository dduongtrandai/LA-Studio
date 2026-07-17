#include "TranslationBackendFactory.h"

#include "CrispTranslationBackend.h"
#include "LlamaTranslationBackend.h"

namespace LAStudio {

TranslationBackendFactory::TranslationBackendFactory()
{
    registerBackend(QStringLiteral("crispasr"), [] { return std::make_unique<CrispTranslationBackend>(); });
    registerBackend(QStringLiteral("m2m100"), [] { return std::make_unique<CrispTranslationBackend>(); });
    registerBackend(QStringLiteral("madlad"), [] { return std::make_unique<CrispTranslationBackend>(); });
    registerBackend(QStringLiteral("llama"), [] { return std::make_unique<LlamaTranslationBackend>(); });
}

void TranslationBackendFactory::registerBackend(const QString &backendId, Creator creator)
{
    m_creators.insert(backendId.trimmed().toLower(), std::move(creator));
}

std::unique_ptr<TranslationBackend> TranslationBackendFactory::create(const QString &backendId) const
{
    const auto it = m_creators.constFind(backendId.trimmed().toLower());
    return it == m_creators.cend() || !it.value() ? nullptr : it.value()();
}

} // namespace LAStudio
