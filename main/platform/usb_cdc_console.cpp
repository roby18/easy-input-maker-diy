#include "platform/usb_cdc_console.h"

#include <cstdarg>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "tusb.h"

namespace easy_input::debug_console {
namespace {

constexpr const char* kTag = "cdc_diag";
constexpr std::size_t kLogLineCapacity = 256;
constexpr std::int64_t kDiagIntervalUs = 1000 * 1000;

// ESP_LOG severity ranks (lower = more severe). Records above the compiled
// CONFIG_LOG_DEFAULT_LEVEL (INFO) are not present in the firmware, so the
// console never offers debug/verbose modes.
constexpr unsigned kRankError = 1;
constexpr unsigned kRankWarning = 2;
constexpr unsigned kRankInfo = 3;

// CDC-only output policy. It filters the mirrored stream only; the chained
// UART0 sink and the firmware behaviour are untouched.
enum class ConsoleMode : std::uint8_t {
  // Default: protocol tags at INFO+, every other tag at WARN+; chatty
  // unconfigured-network stack tags are muted on CDC.
  kQuiet = 0,
  // Protocol tags at INFO+, every other tag at ERROR+; same mutes.
  kErrors = 1,
  // Every compiled-in tag at INFO+, nothing muted.
  kInfo = 2,
};

// Tags carrying the console's reason to exist: key events, HID traffic and
// console status. They stay readable at INFO in every mode.
constexpr const char* kProtocolTags[] = {
    "easy_input", "usb_hid", "cdc_diag"};
// Background stack chatter (reconnect retries while no network is
// configured) that makes the CDC stream unreadable; muted unless the user
// explicitly selects the full INFO view.
constexpr const char* kMutedTags[] = {"wifi", "keyboard_audio"};

// Set once in begin(); read from every task that logs. A single function
// pointer / context pair published before tinyusb_driver_install() is enough.
WorkReadyCallback s_work_ready_callback = nullptr;
void* s_work_ready_context = nullptr;
vprintf_like_t s_previous_sink = nullptr;
bool s_sink_installed = false;
bool s_was_active = false;
bool s_was_dtr = false;
bool s_rx_ever_seen = false;
std::uint8_t s_diag_remaining = 0;
std::int64_t s_last_diag_us = 0;
ConsoleMode s_mode{ConsoleMode::kQuiet};
bool s_verbose{false};

// The CDC bulk data path is live as soon as the host has configured the
// composite device and TinyUSB has opened the CDC bulk endpoints. DTR is an
// ACM modem status line, not a requirement for bulk transfers: some host
// stacks (and serial tools) never assert it, so gating output on
// tud_cdc_n_connected(0) would leave the console permanently silent even
// though the port enumerated fine.
bool output_ready() {
  return tud_ready() && tud_cdc_n_ready(0);
}

unsigned severity_rank(char level_char) {
  switch (level_char) {
    case 'E':
      return kRankError;
    case 'W':
      return kRankWarning;
    case 'I':
      return kRankInfo;
    default:
      // DEBUG/VERBOSE are compiled out; an unrecognised prefix fails open so
      // an unusual record (e.g. third-party sink format) is never hidden.
      return 0;
  }
}

// ESP_LOG RTOS lines look like "I (12345) tag: message". Return the tag start
// (terminated by ':') or nullptr when the line does not match the layout.
const char* extract_tag(const char* line) {
  const char* separator = std::strstr(line, ") ");
  return separator == nullptr ? nullptr : separator + 2;
}

bool tag_equals(const char* tag, const char* name) {
  if (tag == nullptr || name == nullptr) {
    return false;
  }
  while (*name != '\0') {
    if (*tag != *name) {
      return false;
    }
    ++tag;
    ++name;
  }
  return *tag == ':';
}

bool tag_in_list(const char* tag, const char* const* list, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    if (tag_equals(tag, list[i])) {
      return true;
    }
  }
  return false;
}

// CDC-only admission test for one formatted ESP_LOG line.
bool line_allowed(const char* line) {
  if (line == nullptr) {
    return true;
  }
  const unsigned rank = severity_rank(line[0]);
  if (rank == 0) {
    return true;
  }
  const char* tag = extract_tag(line);
  if (tag != nullptr && s_mode != ConsoleMode::kInfo) {
    if (tag_in_list(tag,
                    kMutedTags,
                    sizeof(kMutedTags) / sizeof(kMutedTags[0]))) {
      return false;
    }
    if (tag_in_list(tag,
                    kProtocolTags,
                    sizeof(kProtocolTags) / sizeof(kProtocolTags[0]))) {
      return rank <= kRankInfo;
    }
  }
  const unsigned threshold = s_mode == ConsoleMode::kErrors
                                 ? kRankError
                                 : (s_mode == ConsoleMode::kQuiet
                                        ? kRankWarning
                                        : kRankInfo);
  return rank <= threshold;
}

const char* mode_name(ConsoleMode mode) {
  switch (mode) {
    case ConsoleMode::kQuiet:
      return "quiet (key/HID traces INFO, other tags WARN+, wifi/audio "
             "muted)";
    case ConsoleMode::kErrors:
      return "errors (key/HID traces INFO, other tags ERROR+, wifi/audio "
             "muted)";
    case ConsoleMode::kInfo:
      return "info (all tags INFO+, nothing muted)";
  }
  return "?";
}

void cycle_mode() {
  s_mode = static_cast<ConsoleMode>(
      (static_cast<std::uint8_t>(s_mode) + 1U) % 3U);
  ESP_LOGI(kTag, "log mode: %s", mode_name(s_mode));
}

void arm_diag_burst(std::uint8_t lines) {
  if (lines > s_diag_remaining) {
    s_diag_remaining = lines;
  }
  // Emit the first diagnostic on the next poll() call.
  s_last_diag_us = 0;
}

void emit_diag() {
  ESP_LOGI(kTag,
           "cdc diag ep_ready=%u dtr=%u rx_avail=%lu tx_free=%lu",
           tud_cdc_n_ready(0) ? 1U : 0U,
           tud_cdc_n_connected(0) ? 1U : 0U,
           static_cast<unsigned long>(tud_cdc_n_available(0)),
           static_cast<unsigned long>(tud_cdc_n_write_available(0)));
}

void pump_diag_burst() {
  if (s_diag_remaining == 0) {
    return;
  }
  const std::int64_t now_us = esp_timer_get_time();
  if (s_last_diag_us != 0 && now_us - s_last_diag_us < kDiagIntervalUs) {
    return;
  }
  s_last_diag_us = now_us;
  --s_diag_remaining;
  emit_diag();
}

void console_write(const char* text) {
  if (text == nullptr || !output_ready()) {
    return;
  }
  std::size_t length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  if (length == 0) {
    return;
  }
  std::uint32_t offset = 0;
  while (offset < length) {
    const std::uint32_t written =
        tud_cdc_n_write(0, text + offset, static_cast<std::uint32_t>(length - offset));
    if (written == 0) {
      break;
    }
    offset += written;
  }
  tud_cdc_n_write_flush(0);
}

void print_banner() {
  console_write("\r\n"
                "EasyInput AI - USB CDC debug console\r\n"
                "commands: h=help, l=log level, v=verbose protocol traces\r\n"
                "default log mode is quiet: key/HID traces stay visible,\r\n"
                "background wifi/audio chatter is muted. Press l to cycle.\r\n"
                "logs show protocol metadata only (input ids, HID report "
                "ids/lengths,\r\n"
                "modifier bitmaps and, after 'v', HID usage codes); typed "
                "text and\r\n"
                "config payloads are never printed.\r\n");
}

void print_help() {
  console_write("h : this help\r\n"
                "l : cycle log mode quiet -> errors -> info -> quiet\r\n"
                "v : toggle verbose protocol traces "
                "(HID usage codes / per-report events)\r\n");
}

void notify_work_ready() {
  if (s_work_ready_callback != nullptr) {
    s_work_ready_callback(s_work_ready_context);
  }
}

// ESP_LOG sink: keep the previous sink (UART0) and additionally mirror the
// formatted line onto CDC. Called while the global ESP_LOG lock is held, so
// log lines from different tasks cannot interleave.
int cdc_log_sink(const char* format, va_list args) {
  int result = 0;
  if (s_previous_sink != nullptr) {
    va_list previous_args;
    va_copy(previous_args, args);
    result = s_previous_sink(format, previous_args);
    va_end(previous_args);
  }

  if (output_ready()) {
    char line[kLogLineCapacity];
    va_list cdc_args;
    va_copy(cdc_args, args);
    const int needed = vsnprintf(line, sizeof(line), format, cdc_args);
    va_end(cdc_args);
    if (needed > 0) {
      const std::size_t length =
          needed < static_cast<int>(sizeof(line))
              ? static_cast<std::size_t>(needed)
              : sizeof(line) - 1U;
      line[length] = '\0';
      if (!line_allowed(line)) {
        return result;
      }
      std::uint32_t offset = 0;
      while (offset < length) {
        const std::uint32_t written = tud_cdc_n_write(
            0, line + offset, static_cast<std::uint32_t>(length - offset));
        if (written == 0) {
          break;
        }
        offset += written;
      }
      tud_cdc_n_write_flush(0);
    }
  }
  return result;
}

}  // namespace

// Internal hook for the TinyUSB task callbacks below. Kept out of the public
// header because application code only registers its owner through
// set_work_ready_callback().
void on_tinyusb_task_event() {
  notify_work_ready();
}

void begin() {
  if (s_sink_installed) {
    return;
  }
  s_sink_installed = true;
  s_previous_sink = esp_log_set_vprintf(&cdc_log_sink);
}

void set_work_ready_callback(WorkReadyCallback callback, void* context) {
  s_work_ready_callback = callback;
  s_work_ready_context = context;
}

bool dte_active() {
  return output_ready();
}

bool verbose() {
  return s_verbose;
}

void poll() {
  const bool active = output_ready();
  if (active != s_was_active) {
    s_was_active = active;
    if (active) {
      print_banner();
      // One immediate status line right after enumeration/config restore.
      arm_diag_burst(1);
    }
  }
  if (!active) {
    s_was_dtr = false;
    s_diag_remaining = 0;
    return;
  }

  // DTR is informational only: a terminal that does assert it gets the banner
  // again plus a short, bounded diagnostic burst for bring-up evidence.
  const bool dtr = tud_cdc_n_connected(0);
  if (dtr && !s_was_dtr) {
    print_banner();
    arm_diag_burst(5);
  }
  s_was_dtr = dtr;
  pump_diag_burst();

  std::uint8_t byte = 0;
  while (tud_cdc_n_available(0) > 0 &&
         tud_cdc_n_read(0, &byte, sizeof(byte)) == sizeof(byte)) {
    if (!s_rx_ever_seen) {
      s_rx_ever_seen = true;
      // First host byte proves the RX path; report both path states once.
      arm_diag_burst(2);
      pump_diag_burst();
    }
    switch (std::tolower(static_cast<unsigned char>(byte))) {
      case 'h':
        print_help();
        break;
      case 'l':
        cycle_mode();
        break;
      case 'v':
        s_verbose = !s_verbose;
        ESP_LOGI(kTag,
                 "verbose protocol traces %s",
                 s_verbose ? "on" : "off");
        break;
      case '\r':
      case '\n':
      case '\t':
      case ' ':
        break;
      default:
        console_write("unknown command; press h for help\r\n");
        break;
    }
  }
}

}  // namespace easy_input::debug_console

// TinyUSB CDC weak callback overrides. They run on the TinyUSB task and only
// wake the owner; all command parsing stays on the owner task in poll().
extern "C" void tud_cdc_rx_cb(uint8_t itf) {
  if (itf == 0) {
    easy_input::debug_console::on_tinyusb_task_event();
  }
}

extern "C" void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)rts;
  if (itf == 0 && dtr) {
    easy_input::debug_console::on_tinyusb_task_event();
  }
}
