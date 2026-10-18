# PlatformIO pre-build step: gzip web/index.html into src/web_index.h so the
# phone app is served straight from flash.
import gzip, os
Import("env")  # noqa: F821  (provided by PlatformIO)

proj = env["PROJECT_DIR"]  # noqa: F821
src = os.path.join(proj, "web", "index.html")
dst = os.path.join(proj, "src", "web_index.h")
data = gzip.compress(open(src, "rb").read(), 9)
with open(dst, "w") as f:
    f.write("// Generated from web/index.html by scripts/embed_web.py. Do not edit.\n#pragma once\n#include <Arduino.h>\n")
    f.write(f"const size_t WEB_INDEX_GZ_LEN = {len(data)};\n")
    f.write("const uint8_t WEB_INDEX_GZ[] PROGMEM = {\n")
    for i in range(0, len(data), 20):
        f.write("  " + ", ".join(f"0x{b:02x}" for b in data[i:i+20]) + ",\n")
    f.write("};\n")
print(f"embed_web: {len(data)} bytes gzipped")
