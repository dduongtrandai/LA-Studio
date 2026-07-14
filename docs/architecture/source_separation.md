# Source Separation Architecture

This document describes the design, threading model, and implementation details of the Source Separation capability in LA Studio.

## 1. Component Diagram

```text
VoiceIsolatorController / DubbingJobRunner
                     │
         [SeparationRequest struct]
                     ▼
          SourceSeparationService
                     │
          [owns & runs via QThread]
                     ▼
             SeparationWorker
                     │
                     ├─► SeparationCache (lookup / save manifest)
                     │
                     ├─► SeparationAudioIO (decode via QAudioDecoder/FFmpeg)
                     │
                     └─► SeparationBackendFactory
                               │
                       [create backend]
                               ▼
                   SeparationBackend (Interface)
                               ▲
                               │
                  SherpaOnnxSeparationBackend
                               │
                       [owns instance]
                               ▼
                      SherpaOnnxRuntime (RAII DLL wrapper)
```

---

## 2. Ownership & Threading Model

- **`SourceSeparationService`**: Lives on the main/UI thread. It owns a dedicated `QThread` and an instance of `SeparationWorker`.
- **`SeparationWorker`**: Moved to the dedicated worker thread. All heavy computations—hashing, media decoding (via QAudioDecoder or FFmpeg), inference, cache read/write, and WAV encoding—are performed sequentially on this worker thread.
- **Queued Connections**: Thread communication uses Qt's queued signal/slot mechanism. Audio buffers are **never** passed back via signals to the UI thread; instead, filepath references to output stems are returned in the final `SeparationResult`.
- **Destruction Safety**: When the service is destroyed, it sets an atomic cancel flag, requests the thread to quit, and blocks using `m_thread->wait()` to ensure that no DLLs are unloaded while native backend execution is still in progress.

---

## 3. Typed Configuration Contract

Clients interact with the service using typed requests defined in `SeparationTypes.h`:

- **`SeparationConfiguration`**: Contains runtime paths, target backend ID, pipeline profile (e.g. `uvr-2stems`, `spleeter-2stems`), and mapping of role names (e.g., `model`, `vocals-model`) to resolved model paths.
- **`SeparationRequest`**: Combines the input media path, output root directory, configuration, and execution parameters (such as `numThreads`).
- **`SeparationResult`**: Reports execution status (`success`), cache status (`cacheHit`), target output stems (path, sample rate, channels), and structured error codes.

---

## 4. Backend Registration & Extension

The system defines the `SeparationBackend` interface. Concrete backends register their creator functions with the `SeparationBackendFactory`:

```cpp
class SeparationBackend {
public:
    virtual ~SeparationBackend() = default;
    virtual QString id() const = 0;
    virtual BackendResult separate(...) = 0;
};
```

### Adding a New Model Profile (UVR/Spleeter on sherpa-onnx)
- Add catalog entry under `catalog-src/`. Specify the correct `backend` and `pipelineProfile`.
- No code changes are required in `SourceSeparationService` or `SeparationWorker`.

### Adding a New Runtime/Inference Engine (e.g., Demucs, ONNX Runtime)
- Implement `SeparationBackend` for the new engine.
- Register the creator function in `SeparationBackendFactory::registerBuiltins()`.
- Add the corresponding runtime ID and model capabilities in the catalog.

---

## 5. Cancellation Mechanism & Limitations

- **Cancellation Token**: Passed down from the service to the worker and backends.
- **FFmpeg/Decoding Checkpoints**: The decoding stage checks the token periodically. If cancelled, FFmpeg is terminated immediately and partial staging files are removed.
- **Sherpa-Onnx Inference Block**: Native `sherpa-onnx` inference is executed as a single C API function call (`SherpaOnnxOfflineSourceSeparationProcess`), which cannot be interrupted midway. Therefore, cancellation requested during inference waits for the native call to return, then immediately discards the output instead of writing it to cache or notifying the user of success. The UI displays "cancellation requested" during this waiting period.

---

## 6. Cache Invalidation Contract

Separation results are cached under:
`<outputRoot>/<sourceHash>/<configurationHash>/`

### `configurationHash` components:
- Cache schema version (`v2`).
- `backendId` and `pipelineProfile`.
- Runtime ID, version, and runtime file fingerprint (name, size, modification time).
- Specific configuration signature and sorted list of model roles.
- Model file fingerprints (name, size, modification time).
- Expected list of output stem IDs.

### Invalidation criteria:
- If a model file is modified, its fingerprint changes, resulting in a cache miss.
- If a different profile is selected (e.g., UVR vs. Spleeter), the pipeline profile string changes, leading to a different configuration hash.
- Manifest validation: Cache hit is only accepted if `manifest.json` exists, matches the expected schema/key, and all declared stem files exist on disk with non-zero sizes.
