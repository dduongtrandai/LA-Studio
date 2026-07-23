#pragma once

#include <QString>

namespace vietnorm {

inline constexpr const char *kVersion = "0.1.0-internal";
inline constexpr const char *kDataVersion = "vietnormalizer-0.2.3";

inline QString version() { return QString::fromLatin1(kVersion); }
inline QString dataVersion() { return QString::fromLatin1(kDataVersion); }

} // namespace vietnorm
