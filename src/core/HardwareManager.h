#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QtQml/qqml.h>

namespace LAStudio {

class HardwareManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString cpuName READ cpuName NOTIFY hardwareInfoChanged)
    Q_PROPERTY(QString cpuArchitecture READ cpuArchitecture NOTIFY hardwareInfoChanged)
    Q_PROPERTY(QString cpuFlags READ cpuFlags NOTIFY hardwareInfoChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY resourceUsageChanged)
    
    Q_PROPERTY(double ramTotal READ ramTotal NOTIFY hardwareInfoChanged)
    Q_PROPERTY(double ramUsed READ ramUsed NOTIFY resourceUsageChanged)
    
    Q_PROPERTY(QVariantList gpus READ gpus NOTIFY hardwareInfoChanged)
    Q_PROPERTY(double vramTotal READ vramTotal NOTIFY hardwareInfoChanged)
    Q_PROPERTY(double vramUsed READ vramUsed NOTIFY resourceUsageChanged)

public:
    explicit HardwareManager(QObject *parent = nullptr);
    static HardwareManager* instance();
    static HardwareManager* create(QQmlEngine *, QJSEngine *);

    QString cpuName() const { return m_cpuName; }
    QString cpuArchitecture() const { return m_cpuArchitecture; }
    QString cpuFlags() const { return m_cpuFlags; }
    double cpuUsage() const { return m_cpuUsage; }

    double ramTotal() const { return m_ramTotal; }
    double ramUsed() const { return m_ramUsed; }

    QVariantList gpus() const { return m_gpus; }
    double vramTotal() const { return m_vramTotal; }
    double vramUsed() const { return m_vramUsed; }

    Q_INVOKABLE QVariantMap runtimeCompatibility(const QVariantMap &runtime) const;

signals:
    void hardwareInfoChanged();
    void resourceUsageChanged();

private slots:
    void updateResourceUsage();

private:
    void detectHardware();
    void updateCpuUsage();
    void updateRamUsage();
    void updateVramUsage();

    QString m_cpuName;
    QString m_cpuArchitecture;
    QString m_cpuFlags;
    double m_cpuUsage = 0;

    double m_ramTotal = 0;
    double m_ramUsed = 0;

    QVariantList m_gpus;
    double m_vramTotal = 0;
    double m_vramUsed = 0;

    QTimer* m_usageTimer;
    
    // For CPU usage calculation
    unsigned long long m_lastIdleTime = 0;
    unsigned long long m_lastKernelTime = 0;
    unsigned long long m_lastUserTime = 0;
};

} // namespace LAStudio
