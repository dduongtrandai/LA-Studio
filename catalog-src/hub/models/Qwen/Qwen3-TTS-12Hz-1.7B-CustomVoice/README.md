# Qwen3-TTS CustomVoice 1.7B

Qwen3-TTS CustomVoice 1.7B is a text-to-speech model from the Alibaba Qwen team specialized for preset speakers and style instructions, published under the `Qwen` organization on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice
- Upstream GitHub: https://github.com/QwenLM/Qwen2.5-TTS

## Model Facts

- **Task:** Text-to-Speech
- **Parameters:** 1.7B
- **License:** Apache-2.0
- **Architecture:** Qwen2.5-TTS GGUF

## LA Studio Notes

Qwen3-TTS CustomVoice 1.7B in LA Studio is a preset speaker TTS workflow. It downloads GGUF models from conversion repository `cstr/qwen3-tts-1.7b-customvoice-GGUF`. It needs:
- Backbone: `qwen3-tts-12hz-1.7b-customvoice-q8_0.gguf`
- Tokenizer: `qwen3-tts-tokenizer-12hz.gguf`

It comes with 9 default preset speakers: aiden, dylan, eric, ono_anna, ryan, serena, sohee, uncle_fu, and vivian. You can also supply text instructions to control the style of the generated speech.
