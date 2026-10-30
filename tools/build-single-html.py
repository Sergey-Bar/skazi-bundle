"""Build dist/skazi-site.html: the docs/ website as one self-contained file.

Inlines the local stylesheets, scripts, images and audio referenced by
docs/index.html. External links (fonts, downloads) are left as they are.

Usage: python3 tools/build-single-html.py
"""
import base64
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / "docs"
OUT = ROOT / "dist" / "skazi-site.html"

MIME = {
    ".webp": "image/webp",
    ".png": "image/png",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
    ".mp3": "audio/mpeg",
}

STYLESHEET = re.compile(r'<link rel="stylesheet" href="([\w-]+\.css)">')
SCRIPT = re.compile(r'<script src="([\w-]+\.js)"></script>')
ASSET = re.compile(r"(?<![\w/])(?:img|audio)/[\w.-]+\.(?:webp|png|svg|ico|mp3)\b")


def site_file(name):
    path = (SITE / name).resolve()
    if not path.is_relative_to(SITE):
        raise ValueError(f"reference outside docs/: {name}")
    return path


def read_text(name):
    return site_file(name).read_text(encoding="utf-8")


_cache = {}


def data_uri(match):
    name = match.group(0)
    if name not in _cache:
        path = site_file(name)
        payload = base64.b64encode(path.read_bytes()).decode()
        _cache[name] = f"data:{MIME[path.suffix]};base64,{payload}"
    return _cache[name]


def main():
    html = read_text("index.html")
    html = STYLESHEET.sub(lambda m: f"<style>\n{read_text(m.group(1))}\n</style>", html)
    html = SCRIPT.sub(
        lambda m: "<script>\n" + read_text(m.group(1)).replace("</script", "<\\/script") + "\n</script>",
        html,
    )
    html = ASSET.sub(data_uri, html)

    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(html) // 1024} KB")


if __name__ == "__main__":
    main()
