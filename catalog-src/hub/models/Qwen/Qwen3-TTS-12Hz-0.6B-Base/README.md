# Qwen3-TTS Base 0.6B

Qwen3-TTS Base 0.6B is a lightweight zero-shot voice cloning text-to-speech model from the Alibaba Qwen team, published under the `Qwen` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/Qwen/Qwen3-TTS-12Hz-0.6B-Base
- Upstream GitHub: https://github.com/QwenLM/Qwen2.5-TTS

## Model Facts

- **Task:** Text-to-Speech (Zero-Shot Voice Cloning)
- **Parameters:** 600M (0.6B)
- **License:** Apache-2.0
- **Architecture:** Qwen2.5-TTS GGUF

## LA Studio Notes

Qwen3-TTS Base 0.6B in LA Studio is a zero-shot voice cloning workflow. It downloads GGUF models from conversion repository `cstr/qwen3-tts-0.6b-base-GGUF`. It needs:
- Backbone: `qwen3-tts-12hz-0.6b-base-q4_k.gguf`, `qwen3-tts-12hz-0.6b-base-q8_0.gguf`, or `qwen3-tts-12hz-0.6b-base.gguf`
- Tokenizer: `qwen3-tts-tokenizer-12hz.gguf`

It requires a 24kHz reference WAV file and a reference transcript to clone a voice.
