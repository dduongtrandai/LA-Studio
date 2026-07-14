# Kokoro Vietnamese

Native LA Studio integration for Vietnamese Kokoro TTS.

- Model source: https://huggingface.co/contextboxai/Kokoro-Vietnamese
- Upstream code: https://github.com/iamdinhthuan/Kokoro-Vietnamese
- Runtime: https://github.com/dduongtrandai/Kokoro-Vietnamese.cpp/releases/tag/v0.1.0

The selected runtime package includes `kokoro-vietnamese.dll`, `kokoro_vi_g2p.exe`,
ONNX Runtime, `kokoro_vi.onnx`, `config.json`, and converted `.bin` preset
voicepacks. LA Studio should pass text directly to the runtime; Vietnamese G2P
is handled inside the runtime package.
