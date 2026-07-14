# Nemotron-3.5 ASR Streaming 0.6B

Nemotron-3.5 ASR Streaming 0.6B is NVIDIA's multilingual, cache-aware streaming automatic-speech-recognition model. It uses a FastConformer encoder and RNN-T decoder.

## Links

- Upstream model card: https://huggingface.co/nvidia/nemotron-3.5-asr-streaming-0.6b
- CrispASR backend: https://github.com/CrispStrobe/CrispASR/releases/tag/v0.8.6

## Model Facts

- **Task:** Speech-to-Text
- **Parameters:** 0.6B
- **License:** OpenMDW-1.1
- **Languages:** 35 languages, including Vietnamese
- **Architecture:** Cache-aware FastConformer RNN-T

## LA Studio Notes

LA Studio runs this model locally with the `nemotron` backend in CrispASR v0.8.6 or later. The app downloads GGUF artifacts from `cstr/nemotron-3.5-asr-streaming-0.6b-GGUF`:

- `nemotron-3.5-asr-streaming-0.6b-q4_k.gguf` (recommended, 479 MB)
- `nemotron-3.5-asr-streaming-0.6b-f16.gguf` (1.3 GB)

The model is streaming-native. LA Studio's current STT workflow transcribes selected or recorded audio as a completed job; live incremental transcript delivery requires a future CrispASR session-stream API for Nemotron.
