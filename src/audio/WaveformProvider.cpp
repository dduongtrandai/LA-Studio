#include "WaveformProvider.h"
#include <QPainter>
#include <QColor>
#include <cmath>
#include <algorithm>

namespace LAStudio {

WaveformProvider::WaveformProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

void WaveformProvider::setSamples(const QVector<float> &samples)
{
    QMutexLocker lock(&m_mutex);
    m_samples = samples;
}

QImage WaveformProvider::requestImage(const QString &id, QSize *size,
                                       const QSize &requestedSize)
{
    Q_UNUSED(id);

    int w = requestedSize.width() > 0 ? requestedSize.width() : 800;
    int h = requestedSize.height() > 0 ? requestedSize.height() : 120;

    if (size) *size = QSize(w, h);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0x2a, 0x2a, 0x3e));

    QMutexLocker lock(&m_mutex);
    if (m_samples.isEmpty()) return img;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    QColor waveColor(0x7c, 0x4d, 0xff);
    p.setPen(QPen(waveColor, 1.0));

    float midY = h / 2.0f;
    int numSamples = m_samples.size();
    float samplesPerPixel = static_cast<float>(numSamples) / w;

    for (int x = 0; x < w; ++x) {
        int startSample = static_cast<int>(x * samplesPerPixel);
        int endSample = static_cast<int>((x + 1) * samplesPerPixel);
        endSample = std::min(endSample, numSamples);

        float minVal = 0, maxVal = 0;
        for (int s = startSample; s < endSample; ++s) {
            minVal = std::min(minVal, m_samples[s]);
            maxVal = std::max(maxVal, m_samples[s]);
        }

        int y1 = static_cast<int>(midY + minVal * midY);
        int y2 = static_cast<int>(midY - maxVal * midY);
        y1 = std::clamp(y1, 0, h - 1);
        y2 = std::clamp(y2, 0, h - 1);

        p.drawLine(x, y2, x, y1);
    }

    // Center line
    p.setPen(QPen(QColor(255, 255, 255, 60), 1.0));
    p.drawLine(0, static_cast<int>(midY), w, static_cast<int>(midY));

    return img;
}

} // namespace LAStudio

