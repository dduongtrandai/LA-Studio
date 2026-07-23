#include <QCoreApplication>
#include <QtTest>

#include "test_normalizer.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    vietnorm::tests::TestNormalizer suite;
    return QTest::qExec(&suite, argc, argv);
}
