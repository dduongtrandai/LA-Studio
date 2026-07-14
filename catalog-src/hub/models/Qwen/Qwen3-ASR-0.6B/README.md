# Qwen3-ASR 0.6B

Qwen3-ASR 0.6B is a lightweight speech-to-text and audio Q&A model from the Alibaba Qwen team, published under the `Qwen` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/Qwen/Qwen3-ASR-0.6B
- Upstream GitHub: https://github.com/QwenLM/Qwen2.5-ASR

## Model Facts

- **Task:** Speech-to-Text (Transcription and Audio Q&A)
- **Parameters:** 600M (0.6B)
- **License:** Apache-2.0
- **Architecture:** Qwen3-ASR GGUF

## LA Studio Notes

Qwen3-ASR 0.6B in LA Studio runs offline using the CrispASR runtime. It downloads GGUF models from conversion repository `cstr/qwen3-asr-0.6b-GGUF`. It needs one of the following model files:
- `qwen3-asr-0.6b-q4_k.gguf` (Recommended, 631 MB)
- `qwen3-asr-0.6b-q4_k-imatrix.gguf` (631 MB)
- `qwen3-asr-0.6b-q3_k-imatrix.gguf` (531 MB)
- `qwen3-asr-0.6b-q8_0.gguf` (1.0 GB)
- `qwen3-asr-0.6b.gguf` (1.9 GB)

It supports standard multilingual speech transcription as well as free-form audio Q&A instructions via the `Audio Q&A Prompt` configuration setting.
