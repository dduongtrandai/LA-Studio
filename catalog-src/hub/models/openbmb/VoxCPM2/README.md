# VoxCPM2

VoxCPM2 is an open-weight multilingual text-to-speech model from OpenBMB. The upstream model card describes it as a tokenizer-free, diffusion-autoregressive 2B parameter TTS model with 48 kHz audio output, natural-language voice design, and reference-audio voice cloning.

LA Studio installs VoxCPM2 through the CrispASR GGUF conversion repository:

- Upstream model: https://huggingface.co/openbmb/VoxCPM2
- GGUF artifacts: https://huggingface.co/cstr/voxcpm2-GGUF
- Runtime: https://github.com/CrispStrobe/CrispASR/releases/tag/v0.8.6

## Supported Languages

Arabic, Burmese, Chinese, Danish, Dutch, English, Finnish, French, German, Greek, Hebrew, Hindi, Indonesian, Italian, Japanese, Khmer, Korean, Lao, Malay, Norwegian, Polish, Portuguese, Russian, Spanish, Swahili, Swedish, Tagalog, Thai, Turkish, Vietnamese.

## LA Studio Notes

VoxCPM2 requires a CrispASR runtime with the `voxcpm2-tts` backend. LA Studio uses CrispASR v0.8.6 or newer for this model.

Voice design is applied by prepending the voice description to the synthesis text in the upstream VoxCPM2 parenthesized prompt style, for example `(A warm, clear voice)Hello.`. CrispASR's current session API does not expose a separate VoxCPM2 `set_instruct` call; LA Studio encodes the instruction in the text passed to `voxcpm2_synthesize`. Voice cloning uses a reference WAV through CrispASR's unified `crispasr_session_set_voice` API.
