# LA Studio Examples

This directory contains ready-to-run prompt and settings examples for manual QA,
model validation, and future automated smoke tests.

Examples are organized by task first, then by shared capability, then by
model-specific behavior:

```text
examples/
|-- tts/
|   |-- common/          # Baseline TTS examples that most TTS models should handle
|   |-- capabilities/    # Capability-focused examples, such as mixed language text
|   `-- models/          # Model-specific examples for unique syntax or behavior
`-- voice-cloning/       # Reference-audio cloning examples
```

Each JSON file uses this lightweight shape:

```json
{
  "schemaVersion": "lastudio.example.v1",
  "id": "tts-basic-vi",
  "task": "tts",
  "model": {
    "familyId": "vieneu-tts-v3-turbo",
    "required": false
  },
  "capabilities": ["tts", "vi"],
  "inputs": {
    "text": "..."
  },
  "settings": {
    "temperature": 0.45
  },
  "expected": {
    "manualChecks": ["..."]
  }
}
```

Use `model.required: true` when an example depends on a specific model or syntax.
Use `model.required: false` when the example is a generic smoke test that can be
tried with any compatible model.

For voice-cloning examples, `inputs.referenceAudio` points to a repository-local
sample when one is available. Replace it with a user-provided WAV file for real
voice cloning tests.

