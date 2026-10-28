#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make Sill's site (GitHub Pages, .github/workflows/firmware.yml):

  index.html                  the start page: the logo, Install, Try, Source
  install/index.html          web/sill_installer.html, self-contained (fm1pkg.js and fm1ota.js inlined, as
                              Felucca's web/make_site.py does for its installer)
  firmware/sill-X.Y-VER.fwsc  the package (+ LICENSE, LICENSING.md, LICENSES/: it holds JieLi SDK files under
                              Apache-2.0, see LICENSING.md)
  try/                        the browser build (build/si_emu, web/emu/si_build.sh), when it was built
  sill-logo.png

  web/si_make_site.py build/felucca.fwsc VERSION OUT_DIR

The workflow makes dev's test build in OUT_DIR/dev first (SI_SITE_TRY=0: no try/ there), then main's release in
OUT_DIR, whose start page then links the test build.

VERSION is the build's (the commit); Sill's own (X.Y) is SI_VERSION in firmware/src/si_version.h.

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files); its identity
(FM-1_9xx) is what the FM-1 must report after the install.
"""
import json
import os
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the identity, the inlined modules)


def felucca_version():
    m = re.search(r'#define FELUCCA_VERSION "([^"]*)"', (ROOT / "firmware/src/felucca.c").read_text(encoding="utf-8"))
    return m[1] if m else "?"


def si_version():
    m = re.search(r'#define SI_VERSION "([^"]*)"', (ROOT / "firmware/src/si_version.h").read_text(encoding="utf-8"))
    return m[1] if m else "?"


def main(pkg, build, out):
    version = f"{si_version()} ({build})"
    test = build.startswith("dev-")                 # a build from dev: said so on both pages
    test_note = ('<p style="color:#ff8c64">A test build from the dev branch: not yet released.</p>' if test else "")
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca-built package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca's loader: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages")
    html = (HERE / "sill_installer.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    name = f"sill-{si_version()}-{re.sub(r'[^A-Za-z0-9.-]', '-', build)}.fwsc"
    meta = json.dumps({"version": version, "felucca": felucca_version(), "product": product,
                       "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"sill_installer.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, fw = out / "install", out / "firmware"
    for d in (inst, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("sill-*.fwsc"):              # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    lic = ROOT / "LICENSES"
    (fw / "LICENSES").mkdir(exist_ok=True)
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Sill licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    shutil.copy(ROOT / "docs/sill-logo.png", out / "sill-logo.png")
    emu = ROOT / "build/si_emu"
    try_link = ""
    if (emu / "sill.wasm").exists() and os.environ.get("SI_SITE_TRY") != "0":
        dst = out / "try"
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(emu, dst, ignore=shutil.ignore_patterns("obj", "shim"))
        try_link = '<a class="b" href="try/">Try it in the browser</a>'
    if not test and (out / "dev" / "index.html").exists():
        try_link += '<br><a href="dev/">The test build from the dev branch</a>'
    (out / "index.html").write_text(f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sill</title><link rel="icon" href="sill-logo.png">
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin: 0; background: #0c0a26; color: #ece6ff; font: 16px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI",
         Arial, sans-serif; }}
  main {{ max-width: 540px; margin: 0 auto; padding: 40px 16px; text-align: center; }}
  img {{ width: 220px; height: 220px; border-radius: 10px; }}
  h1 {{ font-size: 28px; margin: 12px 0 4px; }}
  p {{ color: #a39cc8; margin: 0 0 24px; }}
  a {{ color: #54aaff; }}
  .b {{ display: inline-block; margin: 6px; padding: 10px 22px; border-radius: 6px; background: #54aaff; color: #0c0a26;
        font-weight: 600; text-decoration: none; }}
  small {{ display: block; margin-top: 28px; color: #a39cc8; }}
</style></head>
<body><main>
<img src="sill-logo.png" alt="Sill">
<h1>Sill</h1>
<p>Generative 90s eurodance for the M-VAVE FM-1, pickled in 1995: offbeat basses, piano and supersaw stabs, big
choruses, a crowd that shouts and a key change for the last chorus. Ten styles; press play.</p>{test_note}
<a class="b" href="install/">Install on the FM-1</a>{try_link}
<small>Sill {version}, on Felucca {felucca_version()} · GPL-3.0 · <a href="https://github.com/delirioux/sill">Source</a> ·
Felucca by Leo Kuroshita, Hügelton Instruments</small>
</main></body></html>
""", encoding="utf-8")
    print(f"site: {out}: install ({len(html)} B), firmware/{name} ({len(raw)} B, {product})"
          + (", try" if try_link else ""))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
