#!/usr/bin/env python3
"""Builds the static site of Pragma Chess into docs/, for GitHub Pages.

No dependencies beyond Python: pages are composed from the templates in
templates/ (HTML with {{placeholders}} and {{> partial}} includes) and the
texts in content/<lang>.json, one file per language. The root index.html
sends the browser to its language (en/ or it/); every page links to the
other languages and to the download of the latest release.

The blog is blog/<slug>/: an article in every language, <lang>.html — a
head of `key: value` lines (title, summary) between two `---` lines, then
the article as HTML —, meta.json (date, the image of the list and of the
page's preview) and its pictures, published beside it as
docs/<lang>/blog/<slug>/. An article missing in a language is not
published in it.

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


def front_matter(text):
    """The `key: value` head of an article and the HTML after it."""
    lines = text.split("\n")
    if not lines or lines[0].strip() != "---":
        return {}, text
    end = lines.index("---", 1)
    head = {}
    for line in lines[1:end]:
        key, _, value = line.partition(":")
        head[key.strip()] = value.strip()
    return head, "\n".join(lines[end + 1:]).strip() + "\n"


def date_text(date, months):
    """"2026-10-10" as the language writes it, by the month names of its content."""
    year, month, day = (int(part) for part in date.split("-"))
    return months["format"].format(day=day, month=months["names"][month - 1], year=year)


def articles(lang):
    """The blog's articles in `lang`, newest first."""
    found = []
    for folder in sorted((ROOT / "blog").iterdir()):
        source = folder / f"{lang}.html"
        if not folder.is_dir() or not source.exists():
            continue
        meta = json.loads((folder / "meta.json").read_text(encoding="utf-8"))
        head, body = front_matter(source.read_text(encoding="utf-8"))
        found.append({"slug": folder.name, "folder": folder, "date": meta["date"], "image": meta["image"],
                      "title": head.get("title", folder.name), "summary": head.get("summary", ""), "body": body})
    return sorted(found, key=lambda article: article["date"], reverse=True)


def languages_of(content, lang, path=""):
    """The language links of a page: the same page, `path` under each language's folder."""
    return [{"code": code, "name": content["language_names"][code], "current": code == lang,
             "href": "../" * (path.count("/") + 1) + f"{code}/{path}"} for code in LANGUAGES]


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
    blog_page = (ROOT / "templates" / "blog.html").read_text(encoding="utf-8")
    post_page = (ROOT / "templates" / "post.html").read_text(encoding="utf-8")
    for lang in LANGUAGES:
        content = json.loads((ROOT / "content" / f"{lang}.json").read_text(encoding="utf-8"))
        context = {
            **content,
            "lang": lang,
            "repo": REPO,
            "repo_url": f"https://github.com/{REPO}",
            "releases_url": f"https://github.com/{REPO}/releases/latest",
            # Where the site's root and the language's home are from the page.
            "root": "../",
            "home": "",
            "page": {**content["meta"], "image": "../assets/screenshots/hero.png"},
            "languages": languages_of(content, lang),
        }
        (OUT / lang).mkdir(exist_ok=True)
        (OUT / lang / "index.html").write_text(render(page, context), encoding="utf-8")

        # The blog: its list, then each article with its pictures.
        posts = articles(lang)
        for post in posts:
            post["date_text"] = date_text(post["date"], content["blog"]["months"])
        blog = {**context, "root": "../../", "home": "../", "in_blog": True, "posts": posts,
                "page": {"title": content["blog"]["page_title"], "description": content["blog"]["lead"],
                         "image": "../../assets/screenshots/hero.png"},
                "languages": languages_of(content, lang, "blog/")}
        (OUT / lang / "blog").mkdir(exist_ok=True)
        (OUT / lang / "blog" / "index.html").write_text(render(blog_page, blog), encoding="utf-8")
        for post in posts:
            target = OUT / lang / "blog" / post["slug"]
            shutil.copytree(post["folder"], target, ignore=shutil.ignore_patterns("*.html", "meta.json"))
            article = {**context, "root": "../../../", "home": "../../", "in_blog": True, "post": post,
                       "page": {"title": f"{post['title']} – Pragma Chess", "description": post["summary"],
                                "image": post["image"]},
                       "languages": [
                           {**language, "href": language["href"] if (post["folder"] / f"{language['code']}.html").exists()
                            else language["href"].removesuffix(post["slug"] + "/")}
                           for language in languages_of(content, lang, f"blog/{post['slug']}/")]}
            (target / "index.html").write_text(render(post_page, article), encoding="utf-8")

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
