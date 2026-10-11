#pragma once

#include <cstdint>

namespace easy_input::debug_console {

using WorkReadyCallback = void (*)(void* context);

// Installs an ESP_LOG sink that mirrors every log line onto TinyUSB CDC ACM
// interface 0 once the host has configured the composite device and the CDC
// bulk endpoints are open (bulk traffic does not require the ACM DTR line).
// The pre-existing UART0 sink is preserved: the CDC sink chains to the
// previous vprintf handler instead of replacing it.
void begin();

// Owner-task hook. Drains CDC RX commands, prints the connect banner and a
// bounded bring-up diagnostic burst. Commands: 'h' help, 'l' cycle CDC log
// mode (quiet/errors/info), 'v' toggle verbose protocol traces.
void poll();

// Wakes the owner task when CDC RX arrives or the DTE opens the port, so the
// owner can observe commands even while blocked waiting for other work.
void set_work_ready_callback(WorkReadyCallback callback, void* context);

// True while the host has the CDC function configured and its bulk endpoints
// are open (i.e. console output can reach the host). DTR alone is not used.
bool dte_active();

// Runtime toggle for verbose protocol traces (HID usage codes, per-report
// queue events, wheel events, compact alive ticks). Defaults to off.
bool verbose();

}  // namespace easy_input::debug_console
