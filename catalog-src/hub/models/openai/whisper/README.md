# Whisper

Whisper is a state-of-the-art automatic speech recognition (ASR) and speech translation model family developed by OpenAI, published under the `openai` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/openai/whisper-large-v3
- Upstream GitHub: https://github.com/openai/whisper

## Model Facts

- **Task:** Speech-to-Text (ASR) & Speech Translation
- **License:** MIT
- **Architecture:** Whisper (GGML)

## LA Studio Notes

Whisper.cpp in LA Studio runs inference locally using CPU and GPU options. It downloads GGML-converted models from conversion repository `ggerganov/whisper.cpp`. It needs:
- Backbone: `ggml-base.bin` (or other supported GGML files).

It supports 100 languages, automatic language detection, translation to English, token timestamps, and speaker diarization workflows.
