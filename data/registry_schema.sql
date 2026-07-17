PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS schema_migrations (
  version INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS catalog_sources (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL CHECK (kind IN ('bundled', 'remote', 'user')),
  version TEXT,
  uri TEXT,
  imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS capabilities (
  id TEXT PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS catalog_model_categories (
  id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL REFERENCES catalog_sources(id) ON DELETE CASCADE,
  display_name TEXT NOT NULL,
  title TEXT NOT NULL,
  description TEXT,
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS catalog_model_category_capabilities (
  category_id TEXT NOT NULL REFERENCES catalog_model_categories(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  PRIMARY KEY (category_id, capability_id)
);

CREATE TABLE IF NOT EXISTS model_families (
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

CREATE TABLE IF NOT EXISTS family_capabilities (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  PRIMARY KEY (family_id, capability_id)
);

CREATE TABLE IF NOT EXISTS family_requirements (
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

CREATE TABLE IF NOT EXISTS requirement_candidates (
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

CREATE TABLE IF NOT EXISTS runtime_engines (
  id TEXT PRIMARY KEY,
  source_id TEXT NOT NULL REFERENCES catalog_sources(id) ON DELETE CASCADE,
  engine_family TEXT NOT NULL,
  display_name TEXT NOT NULL,
  backend TEXT,
  type TEXT CHECK (type IN ('tts', 'stt', 'voice-cloning', 'alignment', 'mixed')),
  library_path TEXT,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS runtime_versions (
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

CREATE TABLE IF NOT EXISTS runtime_dependencies (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  runtime_version_id INTEGER NOT NULL REFERENCES runtime_versions(id) ON DELETE CASCADE,
  dependency_id TEXT NOT NULL,
  filename TEXT NOT NULL,
  url TEXT NOT NULL,
  checksum TEXT,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS family_runtimes (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  sort_order INTEGER NOT NULL DEFAULT 0,
  metadata_json TEXT NOT NULL DEFAULT '{}',
  PRIMARY KEY (family_id, runtime_id)
);

CREATE TABLE IF NOT EXISTS installed_model_files (
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

CREATE TABLE IF NOT EXISTS installed_runtime_versions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  version TEXT NOT NULL,
  local_path TEXT NOT NULL UNIQUE,
  asset_name TEXT,
  installed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_seen_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS user_family_selections (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  capability_id TEXT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  runtime_id TEXT REFERENCES runtime_engines(id) ON DELETE SET NULL,
  runtime_version TEXT,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (family_id, capability_id)
);

CREATE TABLE IF NOT EXISTS user_requirement_selections (
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  requirement_id INTEGER NOT NULL REFERENCES family_requirements(id) ON DELETE CASCADE,
  filename TEXT NOT NULL,
  provider_model_id TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (family_id, requirement_id)
);

CREATE TABLE IF NOT EXISTS user_imported_models (
  id TEXT PRIMARY KEY,
  provider TEXT NOT NULL DEFAULT 'huggingface',
  provider_model_id TEXT NOT NULL,
  local_dir TEXT NOT NULL,
  inferred_family_id TEXT REFERENCES model_families(id) ON DELETE SET NULL,
  status TEXT NOT NULL DEFAULT 'detected' CHECK (status IN ('detected', 'mapped', 'ignored')),
  imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  metadata_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS download_jobs (
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

CREATE TABLE IF NOT EXISTS hardware_compatibility_cache (
  runtime_id TEXT NOT NULL REFERENCES runtime_engines(id) ON DELETE CASCADE,
  hardware_hash TEXT NOT NULL,
  compatible INTEGER NOT NULL CHECK (compatible IN (0, 1)),
  title TEXT NOT NULL,
  detail TEXT NOT NULL,
  checked_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (runtime_id, hardware_hash)
);

CREATE INDEX IF NOT EXISTS idx_family_capabilities_capability
  ON family_capabilities(capability_id);

CREATE INDEX IF NOT EXISTS idx_catalog_model_categories_source
  ON catalog_model_categories(source_id, sort_order);

CREATE INDEX IF NOT EXISTS idx_family_runtimes_runtime
  ON family_runtimes(runtime_id);

CREATE INDEX IF NOT EXISTS idx_requirement_candidates_filename
  ON requirement_candidates(filename);

CREATE INDEX IF NOT EXISTS idx_installed_model_files_provider
  ON installed_model_files(provider_model_id, filename);

CREATE INDEX IF NOT EXISTS idx_installed_runtime_versions_runtime
  ON installed_runtime_versions(runtime_id, version);

CREATE TABLE IF NOT EXISTS active_capability_selections (
  capability_id TEXT PRIMARY KEY,
  family_id TEXT NOT NULL REFERENCES model_families(id) ON DELETE CASCADE,
  runtime_id TEXT REFERENCES runtime_engines(id) ON DELETE SET NULL,
  runtime_version TEXT,
  selected_files_json TEXT NOT NULL DEFAULT '{}',
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Deliberately independent from bundled catalog foreign keys. Bundled model rows are
-- replaced during catalog refreshes, while a user's per-family file choices must survive.
CREATE TABLE IF NOT EXISTS model_family_file_selections (
  capability_id TEXT NOT NULL,
  family_id TEXT NOT NULL,
  selected_files_json TEXT NOT NULL DEFAULT '{}',
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (capability_id, family_id)
);
