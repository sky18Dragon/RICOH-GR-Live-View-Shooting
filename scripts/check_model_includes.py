#!/usr/bin/env python3
"""Include rules for per-model code (multi-brand plan, section 2.3).

A file in src/brands/<brand>/<model>/ may include its own model, its brand's
common/ and shared layers, but never another model, app/, ui/ or main. The
single-model build envs catch link-level coupling; this catches header-level
coupling those envs cannot see. Exits non-zero on a violation.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent / "src"
SHARED_BRAND_DIRS = {"common", "flow"}
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def main() -> int:
    violations = []
    brands = ROOT / "brands"
    for brand in sorted(p for p in brands.iterdir() if p.is_dir()):
        models = {p.name for p in brand.iterdir() if p.is_dir()} - SHARED_BRAND_DIRS
        for model in sorted(models):
            for source in sorted((brand / model).rglob("*")):
                if source.suffix not in (".h", ".cpp"):
                    continue
                for include in INCLUDE.findall(source.read_text(encoding="utf-8")):
                    target = (source.parent / include).resolve()
                    try:
                        relative = target.relative_to(ROOT)
                    except ValueError:
                        continue  # library header
                    parts = relative.parts
                    reason = None
                    if parts[:2] == ("brands", brand.name) and len(parts) > 2:
                        if parts[2] in models and parts[2] != model:
                            reason = f"includes another model ({parts[2]})"
                    elif parts[0] in ("app", "ui") or relative.name == "main.cpp":
                        reason = f"includes {parts[0]}/"
                    if reason:
                        violations.append(f"{source.relative_to(ROOT)}: {reason}: \"{include}\"")
    for line in violations:
        print(line)
    print(f"model include check: {len(violations)} violation(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
