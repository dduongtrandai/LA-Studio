# Text-to-Speech Examples

Use these examples to test TTS behavior from the LA Studio UI or from a future
example runner.

Recommended manual order:

1. Start with `common/basic-vi.json` or `common/basic-en.json`.
2. Test language coverage with `capabilities/mixed-language-vi-en.json`.
3. Test model-specific syntax with `models/vieneu-tts-v3-turbo.json`.
4. Test non-verbal cue handling with `capabilities/non-verbal-tags-vieneu-v3.json`.
5. For reference-audio cloning, use `../voice-cloning/vieneu-tts-v3-turbo.json`.

Capability examples should stay model-neutral when possible. If a capability uses
model-specific syntax, include the model name in the filename and set
`model.required` to `true`.
