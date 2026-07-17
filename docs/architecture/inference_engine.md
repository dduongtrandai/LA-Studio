# Inference engine architecture

Audio studios use one lifecycle vocabulary while keeping backend-specific code isolated:

```text
Engine
  └─ Instance (one model/runtime configuration)
       └─ Worker (QObject on a dedicated thread)
            └─ Backend (runtime adapter)
```

`src/inference/` contains the shared building blocks: session/lifecycle types, instance
registries, cancellation tokens, and worker-thread ownership. STT, TTS, and Translation keep
their public controller/QML APIs, but use these primitives internally.

Translation is the reference implementation of the full layering:

- `TranslationEngine` owns instances and active-instance routing.
- `TranslationEngineInstance` owns one configuration and its worker thread.
- `TranslationWorker` serializes load/unload/translate operations.
- `translation/backends/` contains runtime adapters and the backend factory.

Controllers prepare a `SessionConfiguration` and submit an inference request. They do not load
runtime libraries or call backend ABI functions directly. This lets Translation Studio and
Dubbing share the same loaded instance and cancellation semantics.

When adding a future studio, start with the shared inference types and registry, then implement
the four layers. Keep catalog resolution and UI-facing workflow coordination in controllers;
keep model/runtime ABI details in a backend.
