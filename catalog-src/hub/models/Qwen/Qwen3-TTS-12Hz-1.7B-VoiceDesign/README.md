# Qwen3-TTS 1.7B VoiceDesign

Qwen3-TTS 1.7B VoiceDesign is a text-to-speech model from the Alibaba Qwen team specialized in zero-shot voice design from text instructions, published under the `Qwen` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign
- Upstream GitHub: https://github.com/QwenLM/Qwen2.5-TTS

## Model Facts

- **Task:** Text-to-Speech (Voice Design)
- **Parameters:** 1.7B
- **License:** Apache-2.0
- **Architecture:** Qwen2.5-TTS GGUF

## LA Studio Notes

Qwen3-TTS 1.7B VoiceDesign in LA Studio is a voice design workflow. It downloads GGUF models from conversion repository `cstr/qwen3-tts-1.7b-voicedesign-GGUF`. It needs:
- Backbone: `qwen3-tts-12hz-1.7b-voicedesign-q8_0.gguf`
- Tokenizer: `qwen3-tts-tokenizer-12hz.gguf`

It allows designing unique synthetic voices on the fly by specifying a text style instruction/description alongside the text prompt to synthesize.
