# PlatformIO pre-build step: gzip web/index.html into src/web_index.h so the
# phone app is served straight from flash. The car serves only this one file, so
# the page's local stylesheet (tokens.css), script (crane.js) and fonts are
# inlined first. The same files sit next to the page for GitHub Pages and for
# the website in docs/, which loads them from here.
import base64, gzip, os, re
Import("env")  # noqa: F821  (provided by PlatformIO)

proj = env["PROJECT_DIR"]  # noqa: F821
web = os.path.join(proj, "web")
src = os.path.join(web, "index.html")
dst = os.path.join(proj, "src", "web_index.h")


def read(name):
    return open(os.path.join(web, name), encoding="utf-8").read()


def font(m):
    data = open(os.path.join(web, m.group(1)), "rb").read()
    return "url(data:font/woff2;base64," + base64.b64encode(data).decode() + ")"


page = read("index.html")
page = re.sub(r'<link rel="stylesheet" href="([\w.-]+\.css)">', lambda m: "<style>" + read(m.group(1)) + "</style>", page)
page = re.sub(r'<script src="([\w.-]+\.js)"></script>', lambda m: "<script>" + read(m.group(1)) + "</script>", page)
page = re.sub(r"url\(([\w./-]+\.woff2)\)", font, page)
data = gzip.compress(page.encode("utf-8"), 9, mtime=0)
with open(dst, "w") as f:
    f.write("// Generated from web/index.html by scripts/embed_web.py. Do not edit.\n#pragma once\n#include <Arduino.h>\n")
    f.write(f"const size_t WEB_INDEX_GZ_LEN = {len(data)};\n")
    f.write("const uint8_t WEB_INDEX_GZ[] PROGMEM = {\n")
    for i in range(0, len(data), 20):
        f.write("  " + ", ".join(f"0x{b:02x}" for b in data[i:i+20]) + ",\n")
    f.write("};\n")
print(f"embed_web: {len(data)} bytes gzipped")
