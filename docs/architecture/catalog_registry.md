# Catalog and Local Registry Architecture

## Problem

LA Studio catalog data is not a flat model list. A usable studio configuration is a graph:

- a model family such as OmniVoice, Kokoro, VibeVoice, or Whisper;
- one or more required model files for roles such as `model`, `tokenizer`, `reference`, or `voice`;
- one or more compatible runtime engines;
- one or more installable runtime versions/assets per engine;
- hardware compatibility for CPU, CUDA, Vulkan, and future backends;
- local user state such as installed files, selected variants, imported Hugging Face models, and benchmarks.

The bundled catalog should stay easy to review in Git. SQLite should manage local state, indexing, and dependency resolution.

## Boundaries

Use split catalog source files for authored product metadata:

- `catalog-src/hub/models/<owner>/<model>/model.yaml`: Preferred model family definitions. This mirrors LM Studio's virtual model layout. `model` names the virtual model, `base` lists concrete downloadable components and variants, `metadataOverrides` describes model capabilities, and `config` / `customFields` can bake in defaults.
- `catalog-src/hub/models/<owner>/<model>/model.json`: Legacy LA Studio family definitions kept under the same hub layout while each model is migrated to `model.yaml`.
- `catalog-src/picks/*.json`: Curated LA Studio pick collections. Use names such as `lastudio-picks`; do not use LM Studio's `staff picks` label for project-owned curation.
- `catalog-src/language-sets/*.json`: Shared or large language lists referenced by families.
- `catalog-src/categories.json`: Model categories and UI groupings.

These source files are the **source of truth for developers**. The consolidated `data/catalog.json` is a **generated runtime bundle** and should not be edited directly. To update the catalog, modify model definitions under `catalog-src/hub/models/` and run the generator script:

```bash
python scripts/generate_catalog.py
```

### LA Studio Local Hub Layout

LA Studio uses the same two-folder idea as LM Studio, but it does **not** write to LM Studio's own cache path by default. LM Studio staff-pick metadata lives under a path such as `C:\Users\<user>\.lmstudio\hub\models`. LA Studio writes virtual model metadata to its own app home, `C:\Users\<user>\.lastudio\hub\models`, and writes concrete weights to the configured `modelsPath`.

```text
C:\Users\<user>\.lastudio/
  hub/
    models/
      <owner>/
        <virtual-model>/
          model.yaml
          manifest.json
          README.md
          thumbnail.<ext>

<modelsPath>/
  models/
    <publisher>/
      <concrete-repo>/
        *.gguf
        .la-info.json
```

Example:

```text
C:\Users\<user>\.lastudio\hub\models\hexgrad\Kokoro-82M\model.yaml
C:\Users\<user>\.lastudio\hub\models\hexgrad\Kokoro-82M\manifest.json
C:\Users\<user>\.lastudio\hub\models\hexgrad\Kokoro-82M\README.md
C:\Users\<user>\.lastudio\hub\models\hexgrad\Kokoro-82M\thumbnail.png
<modelsPath>/models/cstr/kokoro-82m-GGUF/kokoro-82m-f16.gguf
<modelsPath>/models/cstr/kokoro-voices-GGUF/kokoro-voice-af_heart.gguf
```

The app still scans the older flat LA Studio layout for backward compatibility, but new downloads should target `models/<publisher>/<repo>`. When a catalog download includes `modelYaml` and `hubFiles`, LA Studio writes the corresponding virtual `hub/models/.../model.yaml`, `manifest.json`, `README.md`, and `thumbnail.<ext>` under `.lastudio`, not under LM Studio's `.lmstudio` directory.

### Family YAML Shape

LA Studio family YAML intentionally separates portable model identity from app-specific UI/runtime metadata:

```yaml
model: hexgrad/Kokoro-82M
base:
  - key: cstr/kokoro-82m-gguf
    role: model
    name: Kokoro Backbone
    purpose: Core model component
    defaultFile: kokoro-82m-f16.gguf
    sources:
      - type: huggingface
        user: cstr
        repo: kokoro-82m-GGUF
        files:
          - name: kokoro-82m-f16.gguf
            size: 330 MB
metadataOverrides:
  domain: tts
  architectures:
    - styletts2
  compatibilityTypes:
    - gguf
  paramsStrings:
    - 82M
  capabilities:
    - tts
laStudio:
  id: kokoro
  type: tts
  localDir: kokoro
  title: Kokoro
  capabilities:
    - tts
  runtimes: []
  studio:
    tts:
      layout: tts
      action: synthesize
```

The generator normalizes this into the existing runtime catalog shape:

- `model` becomes `modelId` unless `laStudio.modelId` overrides it.
- `base[*]` becomes `requiredFiles`, with `role`, `defaultFile`, `sources`, and file variants preserved.
- `metadataOverrides`, `config`, `customFields`, and `suggestions` are stored under `modelYaml` for future loaders and richer UI.
- Sidecar files next to the authored `model.yaml` (`manifest.json`, `README.md`, `thumbnail.<ext>`) are stored under `hubFiles` so installs can recreate an LM Studio-like virtual model folder in `.lastudio`.
- `laStudio` remains the home for product-specific fields such as runtimes, studio layouts, display names, supported languages, and app capability flags.

### LA Studio Picks

Curated recommendations are authored separately from model families:

```json
{
  "id": "lastudio-picks",
  "title": "LA Studio Picks",
  "items": []
}
```

For now, every model family used by the project is part of `lastudio-picks`. These picks are LA Studio catalog metadata; they should not be confused with LM Studio's `staff picks`.

Publisher thumbnails for model picks are stored as `thumbnail.<ext>` sidecar files next to each family definition. To pull the Hugging Face publisher avatar for one model:

```bash
python scripts/sync_hf_thumbnails.py https://huggingface.co/hexgrad/Kokoro-82M
```

To fill missing thumbnails for all curated picks:

```bash
python scripts/sync_hf_thumbnails.py --all-picks
```

The thumbnail sync skips existing files by default so hand-curated thumbnails are preserved. Use `--force` only when intentionally refreshing them.

Use SQLite for local app state and queryable indexes:

- installed model files and checksums;
- installed runtime versions;
- user-imported Hugging Face models;
- selected file variant per family role;
- selected runtime per capability/family;
- download history and pending jobs;
- hardware compatibility cache;
- optional search index.

On startup the app can upsert the generated catalog into the registry.

## Startup Flow

```text
1. Load bundled catalog.json.
2. Load remote catalog.json if available and accepted by version policy.
3. Upsert catalog entities into SQLite.
4. Scan configured model/runtime folders.
5. Detect unknown Hugging Face or local model folders.
6. Resolve readiness:
     required files installed?
     compatible runtime installed?
     user selected variant/runtime still valid?
7. Expose query results to QML through RegistryManager/CatalogManager.
```

## Recommended SQLite Schema

The executable baseline schema lives in `data/registry_schema.sql`. The SQL below documents the same model and the reasoning behind each table group.

Enable foreign keys for every connection:

```sql
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
```

Core metadata:

```sql
CREATE TABLE schema_migrations (
  version INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE catalog_sources (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL CHECK (kind IN ('bundled', 'remote', 'user')),
  version TEXT,
  uri TEXT,
  imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE model_families (
  id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL REFERENCES catalog_sources(id) ON DELETE CASCADE,
  local_dir TEXT NOT NULL,
  provider_model_id TEXT NOT NULL,
  title TEXT NOT NULL,
  subtitle TEXT,
  description TEXT,
  accent TEXT,
  studio_json TEXT NOT NULL DEFAULT '{}',
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE capabilities (
  id TEXT PRIMARY KEY
);

CREATE TABLE catalog_model_categories (
  id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL REFERENCES catalog_sources(id) ON DELETE CASCADE,
  display_name TEXT NOT NULL,
  title TEXT NOT NULL,
  description TEXT,
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE catalog_model_category_capabilities (
  category_id TEXT NOT NULL REFERENCES catalog_model_categories(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  PRIMARY KEY (category_id, capability_id)
);

CREATE TABLE family_capabilities (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  PRIMARY KEY (family_id, capability_id)
);
```

Required files and variants:

```sql
CREATE TABLE family_requirements (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  role TEXT NOT NULL,
  display_name TEXT NOT NULL,
  purpose TEXT NOT NULL,
  default_filename TEXT NOT NULL,
  required INTEGER NOT NULL DEFAULT 1 CHECK (required IN (0, 1)),
  min_count INTEGER NOT NULL DEFAULT 1,
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}',
  UNIQUE (family_id, role)
);

CREATE TABLE requirement_candidates (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  requirement_id INTEGER NOT NULL REFERENCES family_requirements(id) ON DELETE CASCADE,
  provider_model_id TEXT NOT NULL,
  filename TEXT NOT NULL,
  label TEXT,
  size_bytes INTEGER,
  size_label TEXT,
  quantization TEXT,
  language TEXT,
  is_default INTEGER NOT NULL DEFAULT 0 CHECK (is_default IN (0, 1)),
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}',
  UNIQUE (requirement_id, provider_model_id, filename)
);

CREATE INDEX idx_requirement_candidates_filename
  ON requirement_candidates(filename);
```

Runtime engines and assets:

```sql
CREATE TABLE runtime_engines (
  id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL REFERENCES catalog_sources(id) ON DELETE CASCADE,
  engine_family TEXT NOT NULL,
  display_name TEXT NOT NULL,
  backend TEXT,
  type TEXT CHECK (type IN ('tts', 'stt', 'voice-cloning', 'mixed')),
  library_path TEXT,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE runtime_versions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  version TEXT NOT NULL,
  platform TEXT NOT NULL DEFAULT 'win',
  arch TEXT NOT NULL DEFAULT 'x86_64',
  accelerator TEXT NOT NULL DEFAULT 'cpu',
  asset_name TEXT NOT NULL,
  source_base_url TEXT NOT NULL,
  checksum TEXT,
  metadata_json TEXT NOT NULL DEFAULT '{}',
  UNIQUE (runtime_id, version, platform, arch, accelerator)
);

CREATE TABLE runtime_dependencies (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  runtime_version_id INTEGER NOT NULL REFERENCES runtime_versions(id) ON DELETE CASCADE,
  dependency_id TEXT NOT NULL,
  filename TEXT NOT NULL,
  url TEXT NOT NULL,
  checksum TEXT,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE family_runtimes (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}',
  PRIMARY KEY (family_id, runtime_id)
);
```

Local installation state:

```sql
CREATE TABLE installed_model_files (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  provider_model_id TEXT NOT NULL,
  local_path TEXT NOT NULL UNIQUE,
  filename TEXT NOT NULL,
  size_bytes INTEGER,
  checksum TEXT,
  detected_family_id TEXT REFERENCES model_families(id) ON DELETE SET NULL,
  detected_requirement_id INTEGER REFERENCES family_requirements(id) ON DELETE SET NULL,
  source_kind TEXT NOT NULL DEFAULT 'catalog' CHECK (source_kind IN ('catalog', 'huggingface', 'manual')),
  installed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_seen_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE installed_runtime_versions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  version TEXT NOT NULL,
  local_path TEXT NOT NULL UNIQUE,
  asset_name TEXT,
  installed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_seen_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);
```

User choices and imported models:

```sql
CREATE TABLE user_family_selections (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  runtime_id TEXT REFERENCES runtime_engines(id) ON DELETE SET NULL,
  runtime_version TEXT,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (family_id, capability_id)
);

CREATE TABLE user_requirement_selections (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  requirement_id INTEGER NOT NULL REFERENCES family_requirements(id) ON DELETE CASCADE,
  filename TEXT NOT NULL,
  provider_model_id TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (family_id, requirement_id)
);

CREATE TABLE user_imported_models (
  id TEXT PRIMARY KEY,
  provider TEXT NOT NULL DEFAULT 'huggingface',
  provider_model_id TEXT NOT NULL,
  local_dir TEXT NOT NULL,
  inferred_family_id TEXT REFERENCES model_families(id) ON DELETE SET NULL,
  status TEXT NOT NULL DEFAULT 'detected' CHECK (status IN ('detected', 'mapped', 'ignored')),
  imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);
```

Downloads and compatibility:

```sql
CREATE TABLE download_jobs (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL CHECK (kind IN ('model-file', 'runtime-asset', 'dependency')),
  source_url TEXT NOT NULL,
  destination_path TEXT NOT NULL,
  status TEXT NOT NULL,
  bytes_received INTEGER NOT NULL DEFAULT 0,
  bytes_total INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE hardware_compatibility_cache (
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  hardware_hash TEXT NOT NULL,
  compatible INTEGER NOT NULL CHECK (compatible IN (0, 1)),
  title TEXT NOT NULL,
  detail TEXT NOT NULL,
  checked_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (runtime_id, hardware_hash)
);
```

Useful query indexes:

```sql
CREATE INDEX idx_family_capabilities_capability
  ON family_capabilities(capability_id);

CREATE INDEX idx_family_runtimes_runtime
  ON family_runtimes(runtime_id);

CREATE INDEX idx_installed_model_files_provider
  ON installed_model_files(provider_model_id, filename);

CREATE INDEX idx_installed_runtime_versions_runtime
  ON installed_runtime_versions(runtime_id, version);
```

Optional FTS for hub-scale search:

```sql
CREATE VIRTUAL TABLE model_family_search USING fts5(
  family_id UNINDEXED,
  title,
  subtitle,
  description,
  tags
);
```

## Catalog Naming Rules

- `modelCategories` are UI groupings only. They should use stable IDs and human labels such as `Voice Cloning` or `Speech to Text`.
- Runtime or engine names such as `whisper.cpp`, `omnivoice.cpp`, or `CrispASR` belong in runtime metadata, not category names.
- Model family names such as `OmniVoice` and `Kokoro` belong in `ttsFamilies`/`sttFamilies`, not category labels.
- Required file names should describe roles: `Base Voice Model`, `Tokenizer / Audio Codec`, `Reference Voice`, `Whisper Model`.
- Runtime IDs should encode engine, platform, architecture, and accelerator: `omnivoice.cpp-win-x86_64-cuda-12`.

## Migration Path

1. Keep `CatalogManager` loading `data/catalog.json`.
2. Add `RegistryManager` backed by SQLite and Qt Sql.
3. On startup, import the current catalog into SQLite with upsert logic.
4. Move installed file/runtime scans from direct list matching to registry tables.
5. Switch QML gallery queries from raw catalog arrays to registry-backed view models.
6. Later split `catalog-src/` into small source files and generate `data/catalog.json`.
