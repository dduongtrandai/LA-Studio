#pragma once

#include <QString>

namespace LAStudio {

// Small, optional native adapter around libespeak-ng.  The DLL is loaded at
// runtime so LA Studio still builds and runs when the dependency is absent.
class EspeakNgPhonemizer
{
public:
    static bool phonemize(const QString &text, const QString &language,
                          QString &phonemes, QString *error = nullptr);
    static int count(const QString &text, const QString &language,
                     bool *usedLibrary = nullptr, QString *error = nullptr);
};

} // namespace LAStudio
