#!/usr/bin/env python3
import argparse
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

sys.dont_write_bytecode = True
import generate_catalog


HF_BASE_URL = "https://huggingface.co"
SUPPORTED_IMAGE_TYPES = {
    "image/png": ".png",
    "image/jpeg": ".jpg",
    "image/webp": ".webp",
    "image/svg+xml": ".svg",
}
THUMBNAIL_EXTENSIONS = [".png", ".jpg", ".jpeg", ".webp", ".svg"]


class ThumbnailSyncError(RuntimeError):
    pass


def repo_root_from_script():
    return Path(__file__).resolve().parents[1]


def parse_model_id(value):
    value = value.strip()
    if not value:
        raise ThumbnailSyncError("Empty Hugging Face model id or URL")

    parsed = urllib.parse.urlparse(value)
    if parsed.scheme:
        host = parsed.netloc.lower()
        if host not in ("huggingface.co", "www.huggingface.co", "hf.co", "www.hf.co"):
            raise ThumbnailSyncError(f"Unsupported Hugging Face host: {parsed.netloc}")

        segments = [urllib.parse.unquote(part) for part in parsed.path.split("/") if part]
        if len(segments) >= 4 and segments[0] == "api" and segments[1] == "models":
            segments = segments[2:]
        elif segments and segments[0] in ("models", "datasets", "spaces"):
            segments = segments[1:]

        if len(segments) < 2:
            raise ThumbnailSyncError(f"Cannot extract model id from URL: {value}")

        return f"{segments[0]}/{segments[1]}"

    if value.count("/") != 1:
        raise ThumbnailSyncError(f"Expected 'owner/repo', got: {value}")
    return value


def publisher_from_model_id(model_id):
    return model_id.split("/", 1)[0]


def request_url(url, timeout, token=None):
    headers = {
        "User-Agent": "LA-Studio catalog thumbnail sync",
        "Accept": "*/*",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"

    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read(), response.headers


def request_json(url, timeout, token=None):
    data, _headers = request_url(url, timeout=timeout, token=token)
    return json.loads(data.decode("utf-8"))


def find_avatar_url(publisher, timeout, token=None):
    quoted = urllib.parse.quote(publisher, safe="")
    endpoints = [
        f"{HF_BASE_URL}/api/users/{quoted}/avatar",
        f"{HF_BASE_URL}/api/organizations/{quoted}/avatar",
        f"{HF_BASE_URL}/api/users/{quoted}/overview",
        f"{HF_BASE_URL}/api/organizations/{quoted}/overview",
    ]
    errors = []

    for endpoint in endpoints:
        try:
            payload = request_json(endpoint, timeout=timeout, token=token)
        except urllib.error.HTTPError as exc:
            if exc.code in (401, 403, 404):
                errors.append(f"{endpoint}: HTTP {exc.code}")
                continue
            raise
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            errors.append(f"{endpoint}: invalid JSON ({exc})")
            continue

        avatar_url = payload.get("avatarUrl") or payload.get("avatar_url")
        if avatar_url:
            return urllib.parse.urljoin(HF_BASE_URL, avatar_url)
        errors.append(f"{endpoint}: missing avatarUrl")

    detail = "; ".join(errors) if errors else "no endpoint attempted"
    raise ThumbnailSyncError(f"Cannot find avatar URL for publisher '{publisher}': {detail}")


def content_type_without_parameters(value):
    return value.split(";", 1)[0].strip().lower()


def infer_image_type(data, headers, url):
    content_type = content_type_without_parameters(headers.get("Content-Type", ""))
    if content_type in SUPPORTED_IMAGE_TYPES:
        return content_type, SUPPORTED_IMAGE_TYPES[content_type]

    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "image/png", ".png"
    if data.startswith(b"\xff\xd8\xff"):
        return "image/jpeg", ".jpg"
    if data.startswith(b"RIFF") and data[8:12] == b"WEBP":
        return "image/webp", ".webp"

    stripped = data.lstrip()[:300].lower()
    if stripped.startswith(b"<svg") or b"<svg" in stripped:
        return "image/svg+xml", ".svg"

    suffix = Path(urllib.parse.urlparse(url).path).suffix.lower()
    if suffix == ".jpeg":
        suffix = ".jpg"
    for mime_type, extension in SUPPORTED_IMAGE_TYPES.items():
        if suffix == extension:
            return mime_type, extension

    raise ThumbnailSyncError(
        f"Unsupported avatar image type ({content_type or 'unknown content type'}): {url}"
    )


def infer_extension_from_url(url):
    suffix = Path(urllib.parse.urlparse(url).path).suffix.lower()
    if suffix == ".jpeg":
        return ".jpg"
    return suffix if suffix in THUMBNAIL_EXTENSIONS else ".png"


def download_image(url, timeout, token=None):
    data, headers = request_url(url, timeout=timeout, token=token)
    mime_type, extension = infer_image_type(data, headers, url)
    return data, mime_type, extension


def family_files(repo_root):
    hub_models_dir = repo_root / "catalog-src" / "hub" / "models"
    files = []
    files.extend(sorted(hub_models_dir.glob("*/*/model.json")))
    files.extend(sorted(hub_models_dir.glob("*/*/model.yaml")))
    files.extend(sorted(hub_models_dir.glob("*/*/model.yml")))
    files.extend(sorted(hub_models_dir.glob("*/*/family.json")))
    return files


def load_family_index(repo_root):
    by_model_id = {}
    by_family_id = {}

    for family_file in family_files(repo_root):
        family = generate_catalog.load_family_file(family_file)
        record = {
            "familyId": family.get("id"),
            "modelId": family.get("modelId"),
            "dir": family_file.parent,
            "file": family_file,
        }
        if record["modelId"]:
            by_model_id[record["modelId"]] = record
        if record["familyId"]:
            by_family_id[record["familyId"]] = record

    return by_model_id, by_family_id


def load_pick_targets(repo_root, by_model_id, by_family_id):
    picks_dir = repo_root / "catalog-src" / "picks"
    targets = []
    seen = set()

    for picks_file in sorted(picks_dir.glob("*.json")):
        with open(picks_file, "r", encoding="utf-8") as f:
            collection = json.load(f)

        for item in collection.get("items", []):
            family_id = item.get("familyId")
            model_id = item.get("modelId")
            record = by_family_id.get(family_id) if family_id else None
            if record is None and model_id:
                record = by_model_id.get(model_id)
            if record is None:
                label = family_id or model_id or "<missing id>"
                raise ThumbnailSyncError(f"{picks_file} references unknown pick: {label}")

            key = str(record["dir"])
            if key in seen:
                continue
            seen.add(key)
            targets.append(record)

    return targets


def infer_record_for_model_id(repo_root, model_id, by_model_id):
    record = by_model_id.get(model_id)
    if record:
        return record

    publisher, repo = model_id.split("/", 1)
    inferred_dir = repo_root / "catalog-src" / "hub" / "models" / publisher / repo
    if inferred_dir.exists():
        return {
            "familyId": "",
            "modelId": model_id,
            "dir": inferred_dir,
            "file": None,
        }

    raise ThumbnailSyncError(
        f"Model '{model_id}' is not in catalog-src/hub/models. Add the family first."
    )


def existing_thumbnail_file(model_dir):
    for extension in THUMBNAIL_EXTENSIONS:
        candidate = model_dir / f"thumbnail{extension}"
        if candidate.exists():
            return candidate
    return None


def remove_other_thumbnails(model_dir, keep_file):
    for extension in THUMBNAIL_EXTENSIONS:
        candidate = model_dir / f"thumbnail{extension}"
        if candidate.exists() and candidate != keep_file:
            candidate.unlink()


def sync_thumbnail(record, args):
    model_id = record["modelId"]
    if not model_id:
        raise ThumbnailSyncError(f"Family at {record['dir']} is missing modelId")

    existing = existing_thumbnail_file(record["dir"])
    if existing and not args.force:
        print(f"SKIP {model_id}: {existing} already exists")
        return "skipped"

    publisher = publisher_from_model_id(model_id)
    avatar_url = find_avatar_url(publisher, timeout=args.timeout, token=args.token)
    output_file = record["dir"] / f"thumbnail{infer_extension_from_url(avatar_url)}"

    if args.dry_run:
        print(f"DRY  {model_id}: {avatar_url} -> {output_file}")
        return "dry-run"

    image, mime_type, extension = download_image(avatar_url, timeout=args.timeout, token=args.token)
    output_file = record["dir"] / f"thumbnail{extension}"
    if args.force:
        remove_other_thumbnails(record["dir"], output_file)
    output_file.write_bytes(image)
    print(f"OK   {model_id}: wrote {output_file} ({mime_type})")
    return "written"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Download Hugging Face publisher avatars as LA Studio catalog thumbnails."
    )
    parser.add_argument(
        "models",
        nargs="*",
        help="Hugging Face model URLs or repo ids, for example https://huggingface.co/hexgrad/Kokoro-82M",
    )
    parser.add_argument(
        "--all-picks",
        action="store_true",
        help="Sync thumbnails for every item referenced by catalog-src/picks/*.json.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing thumbnail sidecar files.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the target files and avatar URLs without downloading images.",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=30,
        help="HTTP timeout in seconds. Default: 30.",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("HF_TOKEN"),
        help="Optional Hugging Face token. Defaults to HF_TOKEN when set.",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=repo_root_from_script(),
        help="Repository root. Defaults to the parent of the scripts directory.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    repo_root = args.repo_root.resolve()
    by_model_id, by_family_id = load_family_index(repo_root)

    targets = []
    if args.all_picks:
        targets.extend(load_pick_targets(repo_root, by_model_id, by_family_id))

    for value in args.models:
        model_id = parse_model_id(value)
        targets.append(infer_record_for_model_id(repo_root, model_id, by_model_id))

    if not targets:
        raise ThumbnailSyncError("Provide at least one model URL/repo id or use --all-picks")

    seen = set()
    unique_targets = []
    for target in targets:
        key = str(target["dir"])
        if key not in seen:
            unique_targets.append(target)
            seen.add(key)

    counts = {"written": 0, "skipped": 0, "dry-run": 0}
    for target in unique_targets:
        result = sync_thumbnail(target, args)
        counts[result] = counts.get(result, 0) + 1

    print(
        "Done: "
        + ", ".join(f"{key}={value}" for key, value in sorted(counts.items()) if value)
    )


if __name__ == "__main__":
    try:
        main()
    except ThumbnailSyncError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
