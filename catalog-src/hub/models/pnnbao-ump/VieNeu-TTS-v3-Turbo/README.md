# VieNeu-TTS v3 Turbo

VieNeu-TTS v3 Turbo is a 48 kHz Vietnamese-English text-to-speech model from Pham Nguyen Ngoc Bao. LA Studio uses the native `VieNeu-TTS.cpp` runtime packages with the `vieneu-v3-native` pipeline for local synthesis and runtime-reported inference progress.

## LA Studio Notes

LA Studio targets `VieNeu-TTS.cpp` v0.1.3 with `pipelineProfile: vieneu-v3-native`. The runtime exposes a C ABI-compatible progress callback, so LA Studio can show exact inference progress while generating audio.

Required components are packaged in `lastudio-community/VieNeu-TTS-v3-Turbo-CPP` for LA Studio:

- VieNeu v3 config/tokenizer
- Native assets: `backbone.gguf`, `vieneu_v3_heads.npz`, and `acoustic/vieneu_acoustic_weights.npz`
- Preset voices: `voices_v3_turbo.json`
- MOSS Audio Tokenizer Nano ONNX codec files under `codec/`
- Runtime archives from https://github.com/dduongtrandai/VieNeu-TTS.cpp/releases

The native C++ runtime consumes local files only. It does not download model files at runtime.
