#!/usr/bin/env python3
"""Builds the static site of Pragma Chess into docs/, for GitHub Pages.

No dependencies beyond Python: pages are composed from the templates in
templates/ (HTML with {{placeholders}} and {{> partial}} includes) and the
texts in content/<lang>.json, one file per language. The root index.html
sends the browser to its language (en/ or it/); every page links to the
other languages and to the download of the latest release.

    python3 site/build.py            # writes docs/
"""

import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT.parent / "docs"
LANGUAGES = ["en", "it"]
DEFAULT_LANGUAGE = "en"
REPO = "francescobianco/pragma-chess"

PARTIAL = re.compile(r"\{\{>\s*([\w-]+)\s*\}\}")
EACH = re.compile(r"\{\{#each\s+([\w.]+)\s*\}\}(.*?)\{\{/each\}\}", re.S)
IF = re.compile(r"\{\{#if\s+([\w.]+)\s*\}\}(.*?)\{\{/if\}\}", re.S)
VALUE = re.compile(r"\{\{\s*([\w.]+)\s*\}\}")


def lookup(context, path):
    """A dotted path into nested dicts; missing keys give an empty string."""
    value = context
    for part in path.split("."):
        if isinstance(value, dict) and part in value:
            value = value[part]
        else:
            return ""
    return value


def render(template, context):
    """Partials first, then loops and conditions, then plain values."""
    def partial(match):
        return render((ROOT / "templates" / f"{match.group(1)}.html").read_text(encoding="utf-8"), context)

    template = PARTIAL.sub(partial, template)

    def each(match):
        items = lookup(context, match.group(1)) or []
        return "".join(render(match.group(2), {**context, **item, "this": item}) for item in items)

    def condition(match):
        return render(match.group(2), context) if lookup(context, match.group(1)) else ""

    while EACH.search(template) or IF.search(template):
        template = EACH.sub(each, template)
        template = IF.sub(condition, template)
    return VALUE.sub(lambda m: str(lookup(context, m.group(1))), template)


def build():
    if OUT.exists():
        for child in OUT.iterdir():
            # The design notes under docs/tech stay; the generated site is rebuilt around them.
            if child.name != "tech":
                shutil.rmtree(child) if child.is_dir() else child.unlink()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ".nojekyll").write_text("")
    shutil.copytree(ROOT / "assets", OUT / "assets")

    page = (ROOT / "templates" / "page.html").read_text(encoding="utf-8")
    for lang in LANGUAGES:
        content = json.loads((ROOT / "content" / f"{lang}.json").read_text(encoding="utf-8"))
        context = {
            **content,
            "lang": lang,
            "repo": REPO,
            "repo_url": f"https://github.com/{REPO}",
            "releases_url": f"https://github.com/{REPO}/releases/latest",
            "languages": [
                {"code": code, "name": content["language_names"][code], "current": code == lang}
                for code in LANGUAGES
            ],
        }
        (OUT / lang).mkdir(exist_ok=True)
        (OUT / lang / "index.html").write_text(render(page, context), encoding="utf-8")

    redirect = (ROOT / "templates" / "redirect.html").read_text(encoding="utf-8")
    (OUT / "index.html").write_text(
        render(redirect, {"languages": json.dumps(LANGUAGES), "default": DEFAULT_LANGUAGE}), encoding="utf-8"
    )
    (OUT / "404.html").write_text(
        render(redirect, {"languages": json.dumps(LANGUAGES), "default": DEFAULT_LANGUAGE}), encoding="utf-8"
    )
    print(f"site written to {OUT}")


if __name__ == "__main__":
    build()
