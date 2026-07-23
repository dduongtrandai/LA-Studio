#pragma once

#include <QString>

namespace vietnorm::detail {

class VietnameseTextProcessor final {
public:
    QString process(const QString &input) const;

private:
    QString numberToWords(const QString &digits) const;
    QString processGroup(int value, bool hundreds) const;
};

} // namespace vietnorm::detail
