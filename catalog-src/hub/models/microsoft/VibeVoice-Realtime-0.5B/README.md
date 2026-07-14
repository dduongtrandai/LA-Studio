# VibeVoice

VibeVoice Realtime 0.5B is Microsoft's low-latency text-to-speech model. LA Studio installs the CrispASR GGUF build for local synthesis through the shared CrispASR runtime.

## Links

- Upstream model card: https://huggingface.co/microsoft/VibeVoice-Realtime-0.5B
- CrispASR GGUF artifacts: https://huggingface.co/cstr/vibevoice-realtime-0.5b-GGUF
- Runtime: https://github.com/CrispStrobe/CrispASR/releases/tag/v0.8.6

## Model Facts

- **Task:** Text-to-Speech
- **Parameters:** 500M (0.5B)
- **License:** MIT
- **Architecture:** VibeVoice Realtime GGUF
- **CrispASR backend:** `vibevoice-tts`

## LA Studio Notes

LA Studio downloads the recommended Q4_K talker and a required voice prompt GGUF from `cstr/vibevoice-realtime-0.5b-GGUF`:

- Talker: `vibevoice-realtime-0.5b-q4_k.gguf`
- Default voice: `vibevoice-voice-emma.gguf`
- Optional voices: `vibevoice-voice-en-Carter_man.gguf`, `vibevoice-voice-en-Emma_woman.gguf`, and other language voice packs

The realtime 0.5B model uses preset GGUF voice packs. It does not support dynamic WAV reference cloning in CrispASR; use a larger VibeVoice variant when runtime WAV cloning is required.
