#pragma once

#include <QObject>

namespace vietnorm::tests {

class TestNormalizer final : public QObject {
    Q_OBJECT

private slots:
    void normalizesDate();
    void normalizesTime();
    void normalizesCurrency();
    void normalizesPercentage();
    void normalizesStandaloneNumber();
    void normalizesPhoneNumber();
    void normalizesMeasurement();
    void removesUrlsAndSpecialCharacters();
    void preservesVietnameseText();
    void supportsCustomDictionaries();
};

} // namespace vietnorm::tests
