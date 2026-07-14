# OmniVoice

OmniVoice is a massively multilingual zero-shot text-to-speech, voice cloning, and voice design model family developed by `k2-fsa` on Hugging Face.

## Links

- Upstream model card: https://huggingface.co/k2-fsa/OmniVoice
- Upstream GitHub: https://github.com/k2-fsa/sherpa-onnx

## Model Facts

- **Task:** Text-to-Speech, Zero-Shot Voice Cloning & Voice Design
- **Parameters:** ~1B
- **License:** Apache-2.0
- **Architecture:** OmniVoice (GGUF)

## LA Studio Notes

OmniVoice in LA Studio is a high-fidelity speech synthesis workflow. It downloads GGUF models from conversion repository `Serveurperso/OmniVoice-GGUF`. It needs:
- Backbone: `omnivoice-base-Q8_0.gguf`
- Tokenizer: `omnivoice-tokenizer-F32.gguf`

It supports over 600 languages, zero-shot voice cloning, and description-based voice design. It also supports non-verbal tags like `[laughter]`, `[sigh]`, etc.
