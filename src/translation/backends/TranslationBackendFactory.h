#pragma once

#include "TranslationBackend.h"

#include <QHash>
#include <QMutex>
#include <functional>
#include <memory>

namespace LAStudio {

class TranslationBackendFactory final
{
public:
    using Creator = std::function<std::unique_ptr<TranslationBackend>()>;

    TranslationBackendFactory();

    void registerBackend(const QString &backendId, Creator creator);
    std::unique_ptr<TranslationBackend> create(const QString &backendId) const;

private:
    QHash<QString, Creator> m_creators;
};

} // namespace LAStudio
