# Runtime Host Architecture

LA Studio loads ABI-incompatible native runtimes in a dedicated
`LAStudioRuntimeHost` process. The UI process talks to the host through a
versioned framed protocol over a private `QLocalSocket` endpoint.

## Isolation boundary

The host loads exactly one runtime adapter and its dependent DLL directory.
For OmniVoice and Whisper, their native interfaces therefore never enter the
UI process. Llama translation uses the same host adapter and structured result
channel. A model-family host can be restarted independently when
its DLLs fail or when a GPU context becomes unhealthy.

## Data path

Control messages use a compact CBOR payload. Audio is transferred through a
named `QSharedMemory` segment containing little-endian `float32` samples; the
control payload carries only the segment descriptor. This avoids copying large
PCM buffers through the socket while keeping ownership explicit: the receiver
copies the samples before the sender reuses or detaches the segment.

Text and structured inference results (Whisper transcript/segments and Llama
translation patches) remain in the CBOR response payload, so they do not need
an artificial audio encoding.

## Scheduling and lifecycle

The server socket remains on the host's Qt event loop. Adapter work runs on a
worker thread, so ping, cancellation, disconnect, and shutdown remain
observable while inference is executing. The application keeps a host alive
for the loaded model, amortising DLL loading and CUDA context creation across
requests. `LASTUDIO_RUNTIME_HOST=0` is retained as a diagnostic escape hatch
for comparing the legacy in-process path.

`RuntimeHostManager` admits at most two GPU-backed hosts at once. Loading a
third GPU model waits for a slot (up to the configured timeout) instead of
creating another CUDA context and causing avoidable VRAM eviction. Unloading a
model releases the permit immediately; CPU-only hosts are not counted against
the GPU limit.

Runtime catalog entries may declare `metadata.executionMode: hosted` and a
`metadata.hostAdapter` name. This metadata is carried into installed runtime
manifests so the factory can select the matching adapter without inferring it
from a DLL filename.
