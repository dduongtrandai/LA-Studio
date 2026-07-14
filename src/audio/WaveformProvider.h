#pragma once

#include <QQuickImageProvider>
#include <QVector>
#include <QMutex>

namespace LAStudio {

class WaveformProvider : public QQuickImageProvider {
public:
    WaveformProvider();

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

    void setSamples(const QVector<float> &samples);

private:
    QVector<float> m_samples;
    mutable QMutex m_mutex;
};

} // namespace LAStudio

