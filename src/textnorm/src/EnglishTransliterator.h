#pragma once

#include <QString>

namespace vietnorm::detail {

bool isVietnameseWord(const QString &word);
QString transliterateWord(const QString &word);

} // namespace vietnorm::detail
