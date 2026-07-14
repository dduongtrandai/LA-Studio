#!/usr/bin/env python3
import base64
import argparse
import json
import os
import sys
import time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

try:
    if os.environ.get("LA_STUDIO_DISABLE_PYYAML") == "1":
        raise ImportError
    import yaml
except ImportError:
    yaml = None


def strip_yaml_comment(line):
    in_single = False
    in_double = False
    escaped = False
    for i, char in enumerate(line):
        if char == "\\" and in_double and not escaped:
            escaped = True
            continue
        if char == "'" and not in_double:
            in_single = not in_single
        elif char == '"' and not in_single and not escaped:
            in_double = not in_double
        elif char == "#" and not in_single and not in_double:
            return line[:i]
        escaped = False
    return line


def split_top_level(value, separator=","):
    parts = []
    current = []
    depth = 0
    quote = None
    escaped = False
    for char in value:
        if quote:
            current.append(char)
            if char == "\\" and quote == '"' and not escaped:
                escaped = True
                continue
            if char == quote and not escaped:
                quote = None
            escaped = False
            continue
        if char in ("'", '"'):
            quote = char
            current.append(char)
        elif char in "[{":
            depth += 1
            current.append(char)
        elif char in "]}":
            depth -= 1
            current.append(char)
        elif char == separator and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    if current or value.endswith(separator):
        parts.append("".join(current).strip())
    return parts


def split_yaml_key_value(text):
    quote = None
    escaped = False
    for i, char in enumerate(text):
        if quote:
            if char == "\\" and quote == '"' and not escaped:
                escaped = True
                continue
            if char == quote and not escaped:
                quote = None
            escaped = False
            continue
        if char in ("'", '"'):
            quote = char
        elif char == ":":
            return text[:i].strip(), text[i + 1:].strip()
    raise ValueError(f"Expected YAML key/value pair: {text}")


def parse_yaml_scalar(value):
    if value == "":
        return None
    if value in ("null", "Null", "NULL", "~"):
        return None
    if value in ("true", "True", "TRUE"):
        return True
    if value in ("false", "False", "FALSE"):
        return False
    if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
        return json.loads(value) if value.startswith('"') else value[1:-1].replace("''", "'")
    if value.startswith("[") and value.endswith("]"):
        inner = value[1:-1].strip()
        if not inner:
            return []
        return [parse_yaml_scalar(part) for part in split_top_level(inner)]
    if value.startswith("{") and value.endswith("}"):
        inner = value[1:-1].strip()
        if not inner:
            return {}
        result = {}
        for part in split_top_level(inner):
            key, item_value = split_yaml_key_value(part)
            result[key] = parse_yaml_scalar(item_value)
        return result
    try:
        if value.startswith("0") and len(value) > 1 and value[1].isdigit():
            return value
        return int(value)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        return value


def yaml_line_indent(line):
    return len(line) - len(line.lstrip(" "))


def parse_yaml_block(lines, index, indent):
    if index >= len(lines):
        return {}, index

    current_indent = yaml_line_indent(lines[index])
    if current_indent < indent:
        return {}, index

    is_list = lines[index].lstrip().startswith("- ")
    if is_list:
        result = []
        while index < len(lines):
            line_indent = yaml_line_indent(lines[index])
            if line_indent < indent:
                break
            if line_indent != indent or not lines[index].lstrip().startswith("- "):
                break

            item_text = lines[index].lstrip()[2:].strip()
            index += 1
            if not item_text:
                item, index = parse_yaml_block(lines, index, indent + 2)
            elif ":" in item_text and not item_text.startswith(("'", '"')):
                key, value = split_yaml_key_value(item_text)
                item = {key: parse_yaml_scalar(value)}
                if value == "" and index < len(lines) and yaml_line_indent(lines[index]) > indent:
                    item[key], index = parse_yaml_block(lines, index, indent + 2)
                if index < len(lines) and yaml_line_indent(lines[index]) > indent:
                    extra, index = parse_yaml_block(lines, index, indent + 2)
                    if isinstance(extra, dict):
                        item.update(extra)
                    else:
                        item[key] = extra
            else:
                item = parse_yaml_scalar(item_text)
            result.append(item)
        return result, index

    result = {}
    while index < len(lines):
        line_indent = yaml_line_indent(lines[index])
        if line_indent < indent:
            break
        if line_indent != indent:
            break
        text = lines[index].strip()
        if text.startswith("- "):
            break

        key, value = split_yaml_key_value(text)
        index += 1
        if value == "":
            if index < len(lines) and yaml_line_indent(lines[index]) > indent:
                result[key], index = parse_yaml_block(lines, index, indent + 2)
            else:
                result[key] = None
        else:
            result[key] = parse_yaml_scalar(value)
    return result, index


def load_yaml_file(path):
    if yaml is not None:
        with open(path, "r", encoding="utf-8") as f:
            return yaml.safe_load(f) or {}

    cleaned_lines = []
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = strip_yaml_comment(raw_line.rstrip("\n\r")).rstrip()
            if line.strip():
                cleaned_lines.append(line)

    parsed, index = parse_yaml_block(cleaned_lines, 0, 0)
    if index != len(cleaned_lines):
        raise ValueError(f"Unsupported YAML structure near line {index + 1} in {path}")
    return parsed


def source_to_model_id(source):
    if source.get("type") == "huggingface" and source.get("user") and source.get("repo"):
        return f"{source['user']}/{source['repo']}"
    return ""


def unique_preserve_order(values):
    out = []
    seen = set()
    for value in values:
        if not value:
            continue
        key = value.lower()
        if key in seen:
            continue
        seen.add(key)
        out.append(value)
    return out


def collect_hf_download_model_ids(family):
    model_ids = []
    family_model_id = family.get("modelId", "")

    for base in family.get("modelYaml", {}).get("base", []):
        for source in base.get("sources", []):
            model_id = source_to_model_id(source)
            if model_id:
                model_ids.append(model_id)

    for req in family.get("requiredFiles", []):
        has_explicit_source = False
        for source in req.get("sources", []):
            model_id = source_to_model_id(source)
            if model_id:
                model_ids.append(model_id)
                has_explicit_source = True
        model_id = req.get("modelId")
        if model_id:
            model_ids.append(model_id)
            has_explicit_source = True
        if not has_explicit_source and family_model_id:
            model_ids.append(family_model_id)

    return unique_preserve_order(model_ids)


def normalize_family_stats(family):
    stats = dict(family.get("stats", {}))
    model_id = family.get("modelId", "")
    upstream_model_id = stats.get("upstreamModelId") or stats.get("likesModelId") or model_id

    stats["upstreamModelId"] = upstream_model_id
    stats["likesModelId"] = upstream_model_id
    stats["downloadModelIds"] = [upstream_model_id] if upstream_model_id else []
    stats["policy"] = {
        "downloads": "upstream-hf-model",
        "likes": "upstream-hf-model"
    }
    family["stats"] = stats


def fetch_hf_model_stat(model_id, timeout=20):
    url = f"https://huggingface.co/api/models/{quote(model_id, safe='/')}"
    request = Request(url, headers={"User-Agent": "LAStudio catalog generator"})
    with urlopen(request, timeout=timeout) as response:
        data = json.load(response)
    return {
        "modelId": model_id,
        "downloads": int(data.get("downloads") or 0),
        "downloadsAllTime": int(data.get("downloadsAllTime") or 0),
        "likes": int(data.get("likes") or 0),
        "lastModified": data.get("lastModified", "")
    }


def enrich_families_with_hf_stats(families):
    requested_ids = []
    for family in families:
        stats = family.get("stats", {})
        requested_ids.append(stats.get("upstreamModelId", ""))
        requested_ids.extend(stats.get("downloadModelIds", []))

    fetched = {}
    for model_id in unique_preserve_order(requested_ids):
        try:
            fetched[model_id] = fetch_hf_model_stat(model_id)
        except (HTTPError, URLError, TimeoutError, ValueError) as e:
            print(f"Warning: Failed to fetch Hugging Face stats for {model_id}: {e}", file=sys.stderr)

    updated_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    for family in families:
        stats = family.setdefault("stats", {})
        upstream_model_id = stats.get("upstreamModelId", "")
        upstream = fetched.get(upstream_model_id, {})
        download_sources = [fetched[mid] for mid in stats.get("downloadModelIds", []) if mid in fetched]

        if upstream:
            stats["upstreamLikes"] = upstream.get("likes", 0)
            stats["upstreamDownloads"] = upstream.get("downloads", 0)
            stats["upstreamDownloadsAllTime"] = upstream.get("downloadsAllTime", 0)
            stats["displayDownloads"] = upstream.get("downloads", 0)
            stats["localDownloads"] = upstream.get("downloads", 0)
            stats["localDownloadsAllTime"] = upstream.get("downloadsAllTime", 0)
        stats["updatedAt"] = updated_at
        stats["sources"] = {
            "upstream": upstream,
            "downloads": download_sources
        }


def normalize_model_yaml_family(raw, source_path):
    if "model" not in raw or "base" not in raw:
        return raw

    studio = raw.get("laStudio", {})
    if not studio:
        raise ValueError(f"{source_path} uses model.yaml fields but is missing laStudio metadata")

    model_id = raw["model"]
    family = dict(studio)
    family.setdefault("id", model_id.split("/")[-1].lower())
    family.setdefault("modelId", model_id)
    family.setdefault("localDir", family["id"])
    family.setdefault("type", raw.get("metadataOverrides", {}).get("domain", "tts"))
    family.setdefault("capabilities", raw.get("metadataOverrides", {}).get("capabilities", []))
    family.setdefault("tags", raw.get("metadataOverrides", {}).get("compatibilityTypes", []))

    required_files = family.get("requiredFiles", [])
    if not required_files:
        for base in raw.get("base", []):
            sources = base.get("sources", [])
            files = []
            for source in sources:
                for file_info in source.get("files", []):
                    if isinstance(file_info, str):
                        files.append({"name": file_info})
                    else:
                        files.append(file_info)

            default_file = base.get("defaultFile")
            if not default_file and files:
                default_file = files[0].get("name") or files[0].get("file")

            if not default_file:
                continue

            first_source_model_id = source_to_model_id(sources[0]) if sources else ""
            requirement = {
                "role": base.get("role", "model"),
                "name": base.get("name", base.get("key", default_file)),
                "purpose": base.get("purpose", "Required model component"),
                "file": default_file,
                "candidates": [f.get("name") or f.get("file") for f in files if f.get("name") or f.get("file")]
            }
            if base.get("key"):
                requirement["modelKey"] = base["key"]
            if first_source_model_id and first_source_model_id != model_id:
                requirement["modelId"] = first_source_model_id

            default_file_info = next((f for f in files if (f.get("name") or f.get("file")) == default_file), None)
            if default_file_info and default_file_info.get("size"):
                requirement["size"] = default_file_info["size"]
            if files:
                requirement["variants"] = files
            if sources:
                requirement["sources"] = sources
            required_files.append(requirement)
    family["requiredFiles"] = required_files

    model_yaml = {
        "model": raw.get("model"),
        "base": raw.get("base", []),
    }
    for key in ("metadataOverrides", "config", "customFields", "suggestions"):
        if key in raw:
            model_yaml[key] = raw[key]
            if key == "metadataOverrides":
                family[key] = raw[key]
    family["modelYaml"] = model_yaml
    normalize_family_stats(family)
    return family


def load_family_file(path):
    if path.suffix == ".json":
        with open(path, "r", encoding="utf-8") as f:
            family = json.load(f)
        attach_hub_sidecar_files(family, path.parent)
        normalize_family_stats(family)
        return family
    if path.suffix in (".yaml", ".yml"):
        raw = load_yaml_file(path)
        family = normalize_model_yaml_family(raw, path)
        attach_hub_sidecar_files(family, path.parent)
        return family
    raise ValueError(f"Unsupported family file extension: {path}")


def attach_hub_sidecar_files(family, model_dir):
    hub_files = {}

    manifest_file = model_dir / "manifest.json"
    if manifest_file.exists():
        with open(manifest_file, "r", encoding="utf-8") as f:
            hub_files["manifest"] = json.load(f)

    readme_file = model_dir / "README.md"
    if readme_file.exists():
        with open(readme_file, "r", encoding="utf-8") as f:
            hub_files["readme"] = {
                "filename": readme_file.name,
                "content": f.read()
            }

    thumbnail_mime_types = {
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".webp": "image/webp",
        ".svg": "image/svg+xml",
    }
    thumbnail_file = next(
        (model_dir / f"thumbnail{ext}" for ext in thumbnail_mime_types if (model_dir / f"thumbnail{ext}").exists()),
        None
    )
    if thumbnail_file:
        with open(thumbnail_file, "rb") as f:
            hub_files["thumbnail"] = {
                "filename": thumbnail_file.name,
                "mimeType": thumbnail_mime_types.get(thumbnail_file.suffix.lower(), "application/octet-stream"),
                "base64": base64.b64encode(f.read()).decode("ascii")
            }

    if hub_files:
        family["hubFiles"] = hub_files


def enrich_category_items_with_family_hub_files(model_categories, families):
    family_by_model_id = {}
    for family in families:
        model_id = family.get("modelId")
        if model_id:
            family_by_model_id[model_id] = family

    for category in model_categories:
        for item in category.get("items", []):
            wants_detail_metadata = any(key in item for key in ("modelId", "source", "modelCardUrl", "readme"))
            if not wants_detail_metadata:
                continue

            model_id = item.get("modelId") or item.get("realId")
            family = family_by_model_id.get(model_id)
            if not family:
                continue

            item.setdefault("modelId", model_id)
            if family.get("hubFiles"):
                item.setdefault("hubFiles", family["hubFiles"])

            hub_readme = family.get("hubFiles", {}).get("readme", {})
            if hub_readme:
                readme = item.setdefault("readme", {})
                readme.setdefault("filename", hub_readme.get("filename"))
                readme.setdefault("content", hub_readme.get("content"))


def generate_catalog(fetch_hf_stats=False):
    src_dir = Path("catalog-src")
    data_dir = Path("data")
    
    categories_file = src_dir / "categories.json"
    hub_models_dir = src_dir / "hub" / "models"
    lang_sets_dir = src_dir / "language-sets"
    picks_dir = src_dir / "picks"
    
    output_file = data_dir / "catalog.json"
    
    # Load categories
    if not categories_file.exists():
        print(f"Error: {categories_file} not found")
        sys.exit(1)
        
    with open(categories_file, "r", encoding="utf-8") as f:
        model_categories = json.load(f)

    model_picks = []
    if picks_dir.exists():
        for picks_file in sorted(picks_dir.glob("*.json")):
            with open(picks_file, "r", encoding="utf-8") as f:
                model_picks.append(json.load(f))
        
    # Load families
    tts_families = []
    stt_families = []
    family_ids = set()
    
    if not hub_models_dir.exists():
        print(f"Error: {hub_models_dir} not found")
        sys.exit(1)

    family_files = []
    if hub_models_dir.exists():
        family_files.extend(sorted(hub_models_dir.glob("*/*/model.json")))
        family_files.extend(sorted(hub_models_dir.glob("*/*/model.yaml")))
        family_files.extend(sorted(hub_models_dir.glob("*/*/model.yml")))
        family_files.extend(sorted(hub_models_dir.glob("*/*/family.json")))

    if not family_files:
        print(f"Error: No model definitions found in {hub_models_dir}")
        sys.exit(1)

    for family_file in family_files:
        try:
            family = load_family_file(family_file)
        except Exception as e:
            print(f"Error: Failed to load family file {family_file}: {e}")
            sys.exit(1)
            
        family["modelPath"] = family_file.parent.relative_to(hub_models_dir).as_posix()
        fid = family.get("id")
        if not fid:
            print(f"Error: Family file {family_file} missing 'id'")
            sys.exit(1)
            
        if fid in family_ids:
            print(f"Error: Duplicate family id '{fid}' found in {family_file}")
            sys.exit(1)
            
        family_ids.add(fid)
        
        # Validate language sets
        lang_set_id = family.get("supportedLanguageSetId")
        if lang_set_id:
            lang_set_file = lang_sets_dir / f"{lang_set_id}.json"
            if not lang_set_file.exists():
                print(f"Error: Family '{fid}' references non-existent language set '{lang_set_id}'")
                sys.exit(1)
            
            # Validate language count if specified
            try:
                with open(lang_set_file, "r", encoding="utf-8") as lf:
                    lang_set_data = json.load(lf)
                expected_count = family.get("supportedLanguageCount")
                actual_count = len(lang_set_data.get("languages", []))
                if expected_count is not None and expected_count != actual_count:
                    print(f"Error: Family '{fid}' expects {expected_count} languages, but language set '{lang_set_id}' has {actual_count}")
                    sys.exit(1)
            except Exception as e:
                print(f"Error reading/validating language set '{lang_set_id}': {e}")
                sys.exit(1)
                
        # Sort into TTS/STT
        ftype = family.pop("type", "tts")
        if ftype == "tts":
            tts_families.append(family)
        elif ftype == "stt":
            stt_families.append(family)
        else:
            print(f"Error: Unknown family type '{ftype}' in {family_file}")
            sys.exit(1)

    all_families = tts_families + stt_families
    if fetch_hf_stats:
        enrich_families_with_hf_stats(all_families)

    enrich_category_items_with_family_hub_files(model_categories, all_families)
            
    # Final catalog structure
    catalog = {
        "schemaVersion": "1.0.0",
        "version": "0.1.9",
        "modelPicks": model_picks,
        "modelCategories": model_categories,
        "ttsFamilies": tts_families,
        "sttFamilies": stt_families
    }
    
    # Write output
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=2, ensure_ascii=False)
        f.write("\n") # Ensure newline at end of file
        
    print(f"Successfully generated {output_file}")

    # Copy language sets to data folder
    dst_lang_sets = data_dir / "language-sets"
    try:
        dst_lang_sets.mkdir(parents=True, exist_ok=True)
        import shutil
        copied_count = 0
        for lang_set_file in lang_sets_dir.glob("*.json"):
            shutil.copy2(lang_set_file, dst_lang_sets / lang_set_file.name)
            copied_count += 1
        print(f"Successfully copied {copied_count} language set(s) to {dst_lang_sets}")
    except Exception as e:
        print(f"Error copying language sets: {e}")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate the bundled LA Studio model catalog.")
    parser.add_argument(
        "--fetch-hf-stats",
        action="store_true",
        default=os.environ.get("LA_STUDIO_FETCH_HF_STATS") == "1",
        help="Fetch Hugging Face repo metrics and embed normalized catalog stats."
    )
    args = parser.parse_args()
    generate_catalog(fetch_hf_stats=args.fetch_hf_stats)
