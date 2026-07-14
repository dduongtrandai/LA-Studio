# VieNeu-TTS v2 Turbo

VieNeu-TTS v2 Turbo is an advanced, lightweight bilingual Vietnamese-English text-to-speech and voice cloning model, published by `pnnbao-ump` on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/pnnbao-ump/VieNeu-TTS-v2
- Runtime GitHub: https://github.com/dduongtrandai/VieNeu-TTS.cpp

## Model Facts

- **Task:** Text-to-Speech & Voice Cloning
- **Parameters:** ~300M
- **License:** Apache-2.0
- **Architecture:** VieNeu-TTS (GGUF)

## LA Studio Notes

VieNeu-TTS v2 Turbo in LA Studio is optimized for fast CPU execution and instant 3-5 seconds zero-shot voice cloning. It downloads files from conversion repositories:
- `pnnbao-ump/VieNeu-TTS-v2-Turbo-GGUF` for the GGUF backbone and preset voices configuration.
- `pnnbao-ump/VieNeu-Codec` for the speaker encoder and neural decoder ONNX models.
