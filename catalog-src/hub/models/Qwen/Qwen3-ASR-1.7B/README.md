# Qwen3-ASR 1.7B

Qwen3-ASR 1.7B is a medium speech-to-text and audio Q&A model from the Alibaba Qwen team, published under the `Qwen` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/Qwen/Qwen3-ASR-1.7B
- Upstream GitHub: https://github.com/QwenLM/Qwen2.5-ASR

## Model Facts

- **Task:** Speech-to-Text (Transcription and Audio Q&A)
- **Parameters:** 1.7B
- **License:** Apache-2.0
- **Architecture:** Qwen3-ASR GGUF

## LA Studio Notes

Qwen3-ASR 1.7B in LA Studio runs offline using the CrispASR runtime. It downloads GGUF models from conversion repository `cstr/qwen3-asr-1.7b-GGUF`. It needs one of the following model files:
- `qwen3-asr-1.7b-q4_k.gguf` (Recommended, 1.3 GB)
- `qwen3-asr-1.7b-q8_0.gguf` (2.5 GB)
- `qwen3-asr-1.7b-f16.gguf` (4.7 GB)

It supports standard multilingual speech transcription as well as free-form audio Q&A instructions via the `Audio Q&A Prompt` configuration setting.
