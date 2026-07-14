#include "AudioRecorder.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include "audio/WavIO.h"
#include <cmath>
#include <cstdint>
#include <QDir>
#include <algorithm>
#include <QPointer>
#include <QThreadPool>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <QThread>
#include <atomic>

namespace LAStudio {

class WasapiLoopbackThread : public QThread {
    Q_OBJECT
public:
    WasapiLoopbackThread(QObject *parent = nullptr) : QThread(parent) {}
    ~WasapiLoopbackThread() override {
        stop();
        wait();
    }

    void stop() {
        m_running = false;
    }

    QVector<float> monoSamples() const { return m_monoSamples; }
    int sampleRate() const { return m_sampleRate; }

signals:
    void levelUpdated(float level);

protected:
    void run() override {
        m_monoSamples.clear();
        m_running = true;

        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (FAILED(hr)) {
            CoUninitialize();
            return;
        }

        IMMDevice *pDevice = NULL;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        pEnumerator->Release();
        if (FAILED(hr)) {
            CoUninitialize();
            return;
        }

        IAudioClient *pAudioClient = NULL;
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
        pDevice->Release();
        if (FAILED(hr)) {
            CoUninitialize();
            return;
        }

        WAVEFORMATEX *pwfx = NULL;
        hr = pAudioClient->GetMixFormat(&pwfx);
        if (FAILED(hr)) {
            pAudioClient->Release();
            CoUninitialize();
            return;
        }

        m_sampleRate = pwfx->nSamplesPerSec;
        int channels = pwfx->nChannels;
        int bps = pwfx->wBitsPerSample;
        int formatTag = pwfx->wFormatTag;

        // 00000003-0000-0010-8000-00aa00389b71
        static const GUID My_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
            0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
        };

        bool isFloat = false;
        if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE *pEx = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(pwfx);
            if (IsEqualGUID(pEx->SubFormat, My_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                isFloat = true;
            }
        } else if (formatTag == WAVE_FORMAT_IEEE_FLOAT) {
            isFloat = true;
        }

        REFERENCE_TIME hnsRequestedDuration = 10000000; // 1 second
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pwfx, NULL);
        if (FAILED(hr)) {
            CoTaskMemFree(pwfx);
            pAudioClient->Release();
            CoUninitialize();
            return;
        }

        IAudioCaptureClient *pCaptureClient = NULL;
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
        if (FAILED(hr)) {
            CoTaskMemFree(pwfx);
            pAudioClient->Release();
            CoUninitialize();
            return;
        }

        hr = pAudioClient->Start();
        if (FAILED(hr)) {
            pCaptureClient->Release();
            CoTaskMemFree(pwfx);
            pAudioClient->Release();
            CoUninitialize();
            return;
        }

        while (m_running) {
            UINT32 packetLength = 0;
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (SUCCEEDED(hr) && packetLength > 0) {
                BYTE *pData;
                UINT32 numFramesRead;
                DWORD flags;
                hr = pCaptureClient->GetBuffer(&pData, &numFramesRead, &flags, NULL, NULL);
                if (SUCCEEDED(hr)) {
                    QVector<float> chunkMono;
                    chunkMono.reserve(numFramesRead);
                    double sumSq = 0;

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        for (UINT32 i = 0; i < numFramesRead; ++i) {
                            chunkMono.append(0.0f);
                        }
                    } else {
                        if (isFloat && bps == 32) {
                            const float *src = reinterpret_cast<const float *>(pData);
                            for (UINT32 i = 0; i < numFramesRead; ++i) {
                                float sum = 0;
                                for (int c = 0; c < channels; ++c) {
                                    sum += src[i * channels + c];
                                }
                                float mono = sum / channels;
                                chunkMono.append(mono);
                                sumSq += mono * mono;
                            }
                        } else if (bps == 16) {
                            const int16_t *src = reinterpret_cast<const int16_t *>(pData);
                            for (UINT32 i = 0; i < numFramesRead; ++i) {
                                float sum = 0;
                                for (int c = 0; c < channels; ++c) {
                                    sum += src[i * channels + c] / 32768.0f;
                                }
                                float mono = sum / channels;
                                chunkMono.append(mono);
                                sumSq += mono * mono;
                            }
                        } else {
                            for (UINT32 i = 0; i < numFramesRead; ++i) {
                                chunkMono.append(0.0f);
                            }
                        }
                    }

                    m_monoSamples.append(chunkMono);

                    if (numFramesRead > 0) {
                        float level = static_cast<float>(std::sqrt(sumSq / numFramesRead));
                        emit levelUpdated(level);
                    }

                    pCaptureClient->ReleaseBuffer(numFramesRead);
                }
            }
            QThread::msleep(10);
        }

        pAudioClient->Stop();
        pCaptureClient->Release();
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        CoUninitialize();
    }

private:
    std::atomic<bool> m_running{false};
    QVector<float> m_monoSamples;
    int m_sampleRate = 48000;
};

} // namespace LAStudio

#include "AudioRecorder.moc"
#endif

namespace LAStudio {

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
{
}

AudioRecorder::~AudioRecorder() = default;

QAudioFormat AudioRecorder::makeFormat() const
{
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    return fmt;
}

void AudioRecorder::start()
{
    if (m_recording) return;

    m_buffer.clear();
    m_bufferSampleRate = 48000;

#ifdef Q_OS_WIN
    if (m_recordSystemAudio) {
        m_loopbackThread.reset(new WasapiLoopbackThread(this));
        connect(m_loopbackThread.data(), &WasapiLoopbackThread::levelUpdated, this, [this](float lvl){
            m_level = lvl;
            emit levelChanged();
        });
        m_loopbackThread->start();
        m_recording = true;
        emit recordingChanged();
        return;
    }
#endif

    QAudioFormat fmt = makeFormat();
    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!dev.isFormatSupported(fmt)) {
        Logger::warning("AudioRecorder", "Default device doesn't support 48kHz mono Int16");
    }
    m_bufferSampleRate = fmt.sampleRate();

    m_source.reset(new QAudioSource(dev, fmt));
    m_ioDevice = m_source->start();
    if (!m_ioDevice) {
        Logger::error("AudioRecorder", "Failed to start audio source");
        return;
    }

    connect(m_ioDevice, &QIODevice::readyRead, this, &AudioRecorder::onReadyRead);

    m_recording = true;
    emit recordingChanged();
}

void AudioRecorder::stop()
{
    if (!m_recording) return;

#ifdef Q_OS_WIN
    if (m_recordSystemAudio && m_loopbackThread) {
        m_loopbackThread->stop();
        m_loopbackThread->wait();

        QVector<float> floatSamples = m_loopbackThread->monoSamples();
        int srcRate = m_loopbackThread->sampleRate();
        m_loopbackThread.reset();

        m_bufferSampleRate = srcRate > 0 ? srcRate : 48000;
        m_buffer.resize(floatSamples.size() * 2);
        int16_t *dest = reinterpret_cast<int16_t *>(m_buffer.data());
        for (int i = 0; i < floatSamples.size(); ++i) {
            float s = std::clamp(floatSamples[i], -1.0f, 1.0f);
            dest[i] = static_cast<int16_t>(s * 32767.0f);
        }

        m_recording = false;
        m_level = 0.0f;
        emit recordingChanged();
        emit levelChanged();
        emit finished(m_buffer);
        return;
    }
#endif

    m_source->stop();
    m_source.reset();
    m_ioDevice = nullptr;

    m_recording = false;
    m_level = 0.0f;
    emit recordingChanged();
    emit levelChanged();
    emit finished(m_buffer);
}

void AudioRecorder::onReadyRead()
{
    QByteArray chunk = m_ioDevice->readAll();
    m_buffer.append(chunk);

    // Compute RMS for level meter
    const auto *samples = reinterpret_cast<const int16_t *>(chunk.constData());
    int numSamples = static_cast<int>(chunk.size()) / 2;
    if (numSamples > 0) {
        double sum = 0;
        for (int i = 0; i < numSamples; ++i) {
            double s = samples[i] / 32768.0;
            sum += s * s;
        }
        m_level = static_cast<float>(std::sqrt(sum / numSamples));
        emit levelChanged();
    }
}

QString AudioRecorder::saveLastRecordingToCache(int targetSampleRate)
{
    if (m_buffer.isEmpty()) return "";

    const int16_t *srcSamples = reinterpret_cast<const int16_t *>(m_buffer.constData());
    int srcCount = m_buffer.size() / 2;

    QVector<int16_t> destSamples;
    const int sourceSampleRate = m_bufferSampleRate > 0 ? m_bufferSampleRate : targetSampleRate;
    if (targetSampleRate != sourceSampleRate) {
        double ratio = static_cast<double>(targetSampleRate) / sourceSampleRate;
        int destCount = static_cast<int>(srcCount * ratio);
        destSamples.resize(destCount);
        for (int i = 0; i < destCount; ++i) {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, srcCount - 1);
            double frac = srcIdx - idx0;
            destSamples[i] = static_cast<int16_t>(srcSamples[idx0] * (1.0 - frac) + srcSamples[idx1] * frac);
        }
    } else {
        destSamples.resize(srcCount);
        std::memcpy(destSamples.data(), srcSamples, srcCount * 2);
    }

    QString cacheDir = PathUtils::cacheDir();
    QDir().mkpath(cacheDir);

    QString filePath = cacheDir + "/recorded_ref.wav";
    bool ok = WavIO::savePcm16(filePath, destSamples.constData(), destSamples.size(), targetSampleRate, 1);
    if (!ok) {
        Logger::error("AudioRecorder", "Failed to save recorded audio to: " + filePath);
        return "";
    }

    return "file:///" + filePath;
}

void AudioRecorder::saveLastRecordingToCacheAsync(int targetSampleRate)
{
    if (m_saving)
        return;

    if (m_buffer.isEmpty()) {
        emit recordingSaved(QString());
        return;
    }

    m_saving = true;
    emit savingChanged();

    const QByteArray bufferCopy = m_buffer;
    const int sourceSampleRate = m_bufferSampleRate > 0 ? m_bufferSampleRate : targetSampleRate;
    QPointer<AudioRecorder> self(this);

    QThreadPool::globalInstance()->start([self, bufferCopy, sourceSampleRate, targetSampleRate]() {
        if (!self)
            return;

        const int16_t *srcSamples = reinterpret_cast<const int16_t *>(bufferCopy.constData());
        const int srcCount = bufferCopy.size() / 2;

        QVector<int16_t> destSamples;
        if (targetSampleRate != sourceSampleRate) {
            const double ratio = static_cast<double>(targetSampleRate) / sourceSampleRate;
            const int destCount = static_cast<int>(srcCount * ratio);
            destSamples.resize(destCount);
            for (int i = 0; i < destCount; ++i) {
                const double srcIdx = i / ratio;
                const int idx0 = static_cast<int>(srcIdx);
                const int idx1 = std::min(idx0 + 1, srcCount - 1);
                const double frac = srcIdx - idx0;
                destSamples[i] = static_cast<int16_t>(srcSamples[idx0] * (1.0 - frac) + srcSamples[idx1] * frac);
            }
        } else {
            destSamples.resize(srcCount);
            std::memcpy(destSamples.data(), srcSamples, srcCount * 2);
        }

        const QString cacheDir = PathUtils::cacheDir();
        QDir().mkpath(cacheDir);

        const QString filePath = cacheDir + "/recorded_ref.wav";
        const bool ok = WavIO::savePcm16(filePath, destSamples.constData(), destSamples.size(), targetSampleRate, 1);
        const QString savedPath = ok ? ("file:///" + filePath) : QString();

        QMetaObject::invokeMethod(self, [self, savedPath]() {
            if (!self)
                return;

            if (self->m_saving) {
                self->m_saving = false;
                emit self->savingChanged();
            }

            if (savedPath.isEmpty()) {
                Logger::error("AudioRecorder", "Failed to save recorded audio to cache");
            }

            emit self->recordingSaved(savedPath);
        }, Qt::QueuedConnection);
    });
}

} // namespace LAStudio


