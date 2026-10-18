// Text command protocol shared by USB serial, the Pi UART, WebSocket and BLE.
// See docs/protocol.md for the full list.
#pragma once
#include "state.h"

namespace protocol {
// Handle one command line. Writes an optional reply (e.g. "OK", "ERR ...") into
// `reply` (may be empty). Returns false for unknown commands.
bool handle(Source src, const char* line, String& reply);
// Build one telemetry line (JSON, no trailing newline).
size_t telemetry(char* buf, size_t len);
}
