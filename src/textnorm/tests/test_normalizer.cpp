#include "test_normalizer.h"

#include "vietnorm/Normalizer.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

namespace vietnorm::tests {

void TestNormalizer::normalizesDate()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Hôm nay là 25/12/2023"));
    QVERIFY(result.text.contains(QStringLiteral("hai mươi lăm")));
    QVERIFY(result.text.contains(QStringLiteral("tháng mười hai")));
    QVERIFY(!result.text.contains(QStringLiteral("2023")));
}

void TestNormalizer::normalizesTime()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Cuộc họp lúc 14:30"));
    QVERIFY(result.text.contains(QStringLiteral("mười bốn giờ")));
    QVERIFY(result.text.contains(QStringLiteral("ba mươi phút")));
}

void TestNormalizer::normalizesCurrency()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Giá là 1.500.000 đồng"));
    QVERIFY(result.text.contains(QStringLiteral("đồng")));
    QVERIFY(!result.text.contains(QStringLiteral("1.500.000")));
}

void TestNormalizer::normalizesPercentage()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Tăng 25% so với năm ngoái"));
    QVERIFY(result.text.contains(QStringLiteral("phần trăm")));
    QVERIFY(!result.text.contains(QStringLiteral("25%")));
}

void TestNormalizer::normalizesStandaloneNumber()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Tôi có 123 quyển sách"));
    QVERIFY(result.text.contains(QStringLiteral("một trăm hai mươi ba")));
    QVERIFY(!result.text.contains(QStringLiteral("123")));
}

void TestNormalizer::normalizesPhoneNumber()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Gọi 0912345678"));
    QVERIFY(result.text.contains(QStringLiteral("không chín một hai ba bốn năm sáu bảy tám")));
}

void TestNormalizer::normalizesMeasurement()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Tốc độ 120km/h"));
    QVERIFY(result.text.contains(QStringLiteral("ki-lô-mét trên giờ")));
    QVERIFY(result.text.contains(QStringLiteral("một trăm hai mươi")));
}

void TestNormalizer::removesUrlsAndSpecialCharacters()
{
    auto normalizer = Normalizer::create();
    const auto result = normalizer->normalize(QStringLiteral("Xem https://example.com & gọi @abc"));
    QVERIFY(!result.text.contains(QStringLiteral("https://")));
    QVERIFY(result.text.contains(QStringLiteral("và")));
    QVERIFY(result.text.contains(QStringLiteral("a còng")));
}

void TestNormalizer::preservesVietnameseText()
{
    auto normalizer = Normalizer::create();
    const QString input = QStringLiteral("Xin chào thế giới.");
    const auto result = normalizer->normalize(input);
    QCOMPARE(result.text, QStringLiteral("xin chào thế giới."));
}

void TestNormalizer::supportsCustomDictionaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    {
        QFile acronyms(directory.filePath(QStringLiteral("acronyms.csv")));
        QVERIFY(acronyms.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&acronyms);
        stream.setEncoding(QStringConverter::Utf8);
        stream << "acronym,transliteration\nTEST,tét đặc biệt\n";
    }
    {
        QFile words(directory.filePath(QStringLiteral("non-vietnamese-words.csv")));
        QVERIFY(words.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&words);
        stream.setEncoding(QStringConverter::Utf8);
        stream << "original,transliteration\nwidget,qui-dét\n";
    }
    QString error;
    auto normalizer = Normalizer::fromDataDirectory(directory.path(), &error);
    QVERIFY2(normalizer != nullptr, qPrintable(error));
    const auto result = normalizer->normalize(QStringLiteral("TEST widget"));
    QVERIFY(result.text.contains(QStringLiteral("tét đặc biệt")));
    QVERIFY(result.text.contains(QStringLiteral("qui-dét")));
}

} // namespace vietnorm::tests
