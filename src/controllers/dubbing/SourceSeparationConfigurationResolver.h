#pragma once

#include "separation/SeparationTypes.h"

#include <QString>
#include <QVariantMap>

namespace LAStudio {

class ModelManager;
class RuntimeManager;

struct SourceSeparationConfigurationResult {
    SeparationConfiguration configuration;
    bool available = false;
    QString warning;
    QString error;
};

class SourceSeparationConfigurationResolver final
{
public:
    SourceSeparationConfigurationResolver(ModelManager *models, RuntimeManager *runtimes)
        : m_models(models), m_runtimes(runtimes) {}

    SourceSeparationConfigurationResult resolve(const QVariantMap &selection) const;

private:
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
};

} // namespace LAStudio
