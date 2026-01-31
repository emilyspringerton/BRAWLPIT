#!/usr/bin/env python3
"""
Minimal Cursed Engine Prototype (Phase 1)
- Parses a spirit stick ZIP
- Loads mega prompt + key files
- Emits placeholder patches and rationale
- Optionally runs a compile check
"""
import argparse
import hashlib
import json
import os
import random
import subprocess
import zipfile

TARGET_FILES = [
    "packages/common/protocol.h",
    "packages/common/physics.h",
    "packages/simulation/local_game.h",
]


def read_text_file(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def load_spirit_stick(zip_path, extract_dir):
    with zipfile.ZipFile(zip_path, "r") as archive:
        archive.extractall(extract_dir)
        return archive.namelist()


def deterministic_id(seed, payload):
    digest = hashlib.sha256(f"{seed}:{payload}".encode("utf-8")).hexdigest()
    return digest[:12]


def generate_placeholder_patch(file_path, seed):
    patch_id = deterministic_id(seed, file_path)
    return (
        f"# PATCH-ID: {patch_id}\n"
        f"# FILE: {file_path}\n"
        "# NOTE: Replace this with real LLM output.\n"
        f"--- a/{file_path}\n"
        f"+++ b/{file_path}\n"
        "@@ -1,1 +1,2 @@\n"
        "-// TODO: Phase 1 conversion\n"
        "+// TODO: Phase 1 conversion (2D movement + camera)\n"
    )


def run_compile_check():
    cmd = ["gcc", "-o", "brawlpit", "apps/lobby/src/main.c", "-lSDL2", "-lGL", "-lGLU", "-lm"]
    return subprocess.run(cmd, check=False, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(description="Minimal Cursed Engine Prototype")
    parser.add_argument("--spirit-stick", required=True, help="Path to SHANKPIT spirit stick zip")
    parser.add_argument("--mega-prompt", required=True, help="Path to BRAWLPIT mega prompt markdown")
    parser.add_argument("--phase", required=True, help="Phase number (e.g., 1)")
    parser.add_argument("--seed", type=int, required=True, help="Seed for deterministic runs")
    parser.add_argument("--output", required=True, help="Output directory for patches")
    parser.add_argument("--compile-check", action="store_true", help="Run gcc compile check")
    args = parser.parse_args()

    random.seed(args.seed)

    os.makedirs(args.output, exist_ok=True)
    spirit_dir = os.path.join(args.output, "spirit_stick")
    os.makedirs(spirit_dir, exist_ok=True)

    spirit_files = load_spirit_stick(args.spirit_stick, spirit_dir)
    mega_prompt = read_text_file(args.mega_prompt)

    context = {
        "phase": args.phase,
        "seed": args.seed,
        "mega_prompt_len": len(mega_prompt),
        "spirit_files": spirit_files,
    }

    for file_path in TARGET_FILES:
        patch_path = os.path.join(args.output, os.path.basename(file_path).replace(".", "_") + ".patch")
        patch_body = generate_placeholder_patch(file_path, args.seed)
        with open(patch_path, "w", encoding="utf-8") as handle:
            handle.write(patch_body)

    rationale_path = os.path.join(args.output, "RATIONALE.md")
    with open(rationale_path, "w", encoding="utf-8") as handle:
        handle.write("# Phase 1 Patch Rationale\n\n")
        handle.write("This is a placeholder rationale for the minimal prototype.\n")
        handle.write("\n## Run Context\n")
        handle.write("```json\n")
        handle.write(json.dumps(context, indent=2))
        handle.write("\n```\n")

    if args.compile_check:
        result = run_compile_check()
        compile_log = os.path.join(args.output, "compile_check.log")
        with open(compile_log, "w", encoding="utf-8") as handle:
            handle.write(result.stdout)
            handle.write(result.stderr)
        if result.returncode != 0:
            raise SystemExit("Compile check failed. See compile_check.log")

    print(f"Generated patches in {args.output}")


if __name__ == "__main__":
    main()
