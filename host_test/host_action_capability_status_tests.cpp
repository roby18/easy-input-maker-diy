#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

#include "keyboard/ble_status_wire.h"
#include "keyboard/config_status.h"

// T09: the Host Action v1 capability declaration / 512-byte budget tests were
// authored at T08 inside config_status_tests.cpp; they are relocated here
// verbatim so the course contract target host_action_capability_status_tests
// is a registered CTest test. No assertion was added, removed or weakened
// during the move.
namespace {

static_assert(ai_keyboard::kConfigStatusGattSafeLen <= 512);

bool is_valid_utf8(const std::string& value) {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto lead = static_cast<unsigned char>(value[index]);
    if (lead <= 0x7F) {
      ++index;
      continue;
    }

    std::size_t length = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
    } else {
      return false;
    }
    if (index + length > value.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
      const auto continuation = static_cast<unsigned char>(value[index + offset]);
      if ((continuation & 0xC0) != 0x80) {
        return false;
      }
    }
    const auto second = static_cast<unsigned char>(value[index + 1]);
    if ((lead == 0xE0 && second < 0xA0) ||
        (lead == 0xED && second > 0x9F) ||
        (lead == 0xF0 && second < 0x90) ||
        (lead == 0xF4 && second > 0x8F)) {
      return false;
    }
    index += length;
  }
  return true;
}

std::size_t count_needle(const std::string& haystack, const std::string& needle) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

// T08: every published config-status variant advertises the Host Action v1
// capability exactly once, as a JSON boolean inside the single capabilities
// object. It is never a string, never false, never duplicated, and never
// placed at the document top level.
void assert_host_action_capability_boolean_once(const std::string& json) {
  assert(count_needle(json, "host_action_v1") == 1);
  assert(count_needle(json, R"("host_action_v1":true)") == 1);
  assert(json.find(R"("host_action_v1":"true")") == std::string::npos);
  assert(json.find(R"("host_action_v1": false)") == std::string::npos);
  assert(json.find(R"("host_action_v1":false)") == std::string::npos);
  assert(json.find(R"(,"host_action_v1":)") !=
         std::string::npos);  // lives inside an object, never document root
  assert(is_valid_utf8(json));
}

ai_keyboard::BleStatusWireSnapshot worst_case_simple_ble_snapshot() {
  return {
      true,
      true,
      std::numeric_limits<std::uint16_t>::max(),
      std::numeric_limits<std::uint16_t>::max(),
      std::numeric_limits<std::uint16_t>::max(),
  };
}

ai_keyboard::BleDetailedStatusWireSnapshot worst_case_detailed_ble_snapshot() {
  ai_keyboard::BleDetailedStatusWireSnapshot snapshot;
  snapshot.connected = true;
  snapshot.update_in_flight = true;
  snapshot.connection_handle = std::numeric_limits<std::uint16_t>::max();
  snapshot.parameters_valid = true;
  snapshot.interval = std::numeric_limits<std::uint16_t>::max();
  snapshot.latency = std::numeric_limits<std::uint16_t>::max();
  snapshot.supervision_timeout =
      std::numeric_limits<std::uint16_t>::max();
  snapshot.update_status = std::numeric_limits<std::int32_t>::min();
  snapshot.queued_reports = std::numeric_limits<std::uint32_t>::max();
  snapshot.enqueued_reports = std::numeric_limits<std::uint32_t>::max();
  snapshot.transmitted_reports =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.dropped_reports = std::numeric_limits<std::uint32_t>::max();
  snapshot.retryable_reports = std::numeric_limits<std::uint32_t>::max();
  snapshot.hid_queue_high_watermark =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.queued_wheel_reports =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.enqueued_wheel_reports =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.coalesced_wheel_reports =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.transmitted_wheel_reports =
      std::numeric_limits<std::uint32_t>::max();
  snapshot.dropped_wheel_reports =
      std::numeric_limits<std::uint32_t>::max();
  return snapshot;
}

struct PublishedVariantRow {
  const char* name;
  std::size_t raw_bytes;
  std::size_t final_ble_bytes;
};

// Battery refreshes receive the simple live-connection overlay; every other
// status receives the detailed HID queue overlay. Both shared encoders append
// only while the complete UTF-8 JSON stays within the 512-byte envelope,
// exactly as BleHidTransport::status_json_for_publish publishes them.
std::string final_ble_wire_for(const std::string& json, bool battery_phase) {
  return battery_phase
             ? ai_keyboard::append_ble_status_wire_json(
                   json, worst_case_simple_ble_snapshot())
             : ai_keyboard::append_ble_detailed_status_wire_json(
                   json, worst_case_detailed_ble_snapshot());
}

ai_keyboard::SpeakerProbeSnapshot worst_case_speaker_probe() {
  ai_keyboard::SpeakerProbeSnapshot speaker;
  speaker.present = true;
  speaker.version = ai_keyboard::kSpeakerProbeStatusVersion;
  speaker.stage = ai_keyboard::SpeakerProbeStage::Done;
  speaker.result = ai_keyboard::SpeakerProbeResult::Cancelled;
  speaker.error = ai_keyboard::SpeakerProbeError::Unknown;
  speaker.generation = std::numeric_limits<std::uint32_t>::max();
  speaker.raw_error = std::numeric_limits<std::int32_t>::min();
  speaker.microphone_generation =
      std::numeric_limits<std::uint32_t>::max();
  speaker.first_submit_us = std::numeric_limits<std::uint32_t>::max();
  speaker.decode_total_us = std::numeric_limits<std::uint32_t>::max();
  speaker.decode_max_us = std::numeric_limits<std::uint32_t>::max();
  speaker.decoded_frames = std::numeric_limits<std::uint32_t>::max();
  speaker.decoded_pcm_bytes = std::numeric_limits<std::uint32_t>::max();
  speaker.stack_high_water_bytes =
      std::numeric_limits<std::uint32_t>::max();
  speaker.heap_begin_free = std::numeric_limits<std::uint32_t>::max();
  speaker.heap_terminal_free =
      std::numeric_limits<std::uint32_t>::max();
  speaker.heap_largest_block =
      std::numeric_limits<std::uint32_t>::max();
  speaker.heap_minimum_free =
      std::numeric_limits<std::uint32_t>::max();
  speaker.decoded_abs_peak = std::numeric_limits<std::uint32_t>::max();
  speaker.decoded_rms_permille =
      std::numeric_limits<std::uint32_t>::max();
  return speaker;
}

void host_action_v1_is_boolean_once_in_every_published_variant() {
  constexpr unsigned kMaxRows = 16;
  PublishedVariantRow rows[kMaxRows];
  unsigned row_count = 0;
  auto record = [&rows, &row_count](const char* name,
                                    const std::string& json,
                                    bool battery_phase) {
    assert(row_count < kMaxRows);
    assert(json.size() <= ai_keyboard::kConfigStatusGattSafeLen);
    assert_host_action_capability_boolean_once(json);
    const auto wire = final_ble_wire_for(json, battery_phase);
    assert(wire.size() <= ai_keyboard::kConfigStatusGattSafeLen);
    assert(is_valid_utf8(wire));
    rows[row_count++] = {name, json.size(), wire.size()};
  };

  // 1. Full normal status (small snapshot -> the full builder is selected).
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "0.4.40-host-action";
    snapshot.phase = "status";
    snapshot.status = "ok";
    snapshot.saved = true;
    snapshot.battery_mv = 3900;
    snapshot.battery_percent = 72;
    snapshot.target_platform = "macos";

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    // The full builder is identifiable by its uncompacted hotkey fields and
    // keeps every pre-existing capability value; the exact contiguous
    // capabilities object is locked here.
    assert(json.find(R"("ptt_hotkey":"")") != std::string::npos);
    assert(json.find(
               R"("capabilities":{"semantic_actions":true,"offline_platform_switch":true,"config_max_bytes":2048,"usb_management_v1":true,"host_action_v1":true})") !=
           std::string::npos);
    record("full normal", json, false);
  }

  // 2. Compact normal status that still has room for the recent cycle.
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "v";
    snapshot.phase = "status";
    // The unclipped runtime status forces the full builder past 512 bytes;
    // compact clips it to 12 bytes, leaving room for all cycle evidence.
    snapshot.status = std::string(260, 's');
    snapshot.saved = true;
    snapshot.target_platform = "macos";
    snapshot.power = {
        "awake", 1'800'123, "idle", "key",
        4, false, 9, 420'000, true, "key",
    };

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("power":{"compact":true,"mode":"awake")") !=
           std::string::npos);
    assert(json.find(R"("inactive_ms":1800123)") != std::string::npos);
    assert(json.find(R"("deep_sleep_block":"idle")") !=
           std::string::npos);
    assert(json.find(R"("last_wake":"key")") != std::string::npos);
    assert(json.find(R"("cycle_seq":9)") != std::string::npos);
    assert(json.find(R"("cycle_inactive_ms":420000)") !=
           std::string::npos);
    assert(json.find(R"("cycle_deep_sleep":true)") != std::string::npos);
    assert(json.find(R"("cycle_wake":"key")") != std::string::npos);
    record("compact normal with cycle", json, false);
  }

  // 3. Compact normal status that must shed the four cycle fields while
  //    retaining the complete current power object (approved rule 2).
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = std::string(40, 'f');
    snapshot.phase = "status";
    snapshot.status = "ok";
    snapshot.saved = true;
    snapshot.power = {
        "awake", 1'800'123, "key_wake_low", "deep_key",
        4, false, 9, 1'800'000, true, "deep_key",
    };

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("power":{"compact":true,"mode":"awake")") !=
           std::string::npos);
    assert(json.find(R"("inactive_ms":1800123)") != std::string::npos);
    assert(json.find(R"("deep_sleep_block":"key_wake_low")") !=
           std::string::npos);
    assert(json.find(R"("last_wake":"deep_key")") != std::string::npos);
    assert(json.find(R"("cycle_seq")") == std::string::npos);
    assert(json.find(R"("cycle_inactive_ms")") == std::string::npos);
    assert(json.find(R"("cycle_deep_sleep")") == std::string::npos);
    assert(json.find(R"("cycle_wake")") == std::string::npos);
    record("compact normal no cycle", json, false);
  }

  // 4. Deepest runtime fallback: mic control progress retained, static
  //    legacy capabilities and power detail are shed, host_action_v1 stays.
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "0.4.40-idf-v2-speaker-assets";
    snapshot.phase = "status";
    snapshot.status = std::string(64, 's');
    snapshot.saved = true;
    snapshot.target_platform = "windows";
    snapshot.audio.enabled = true;
    snapshot.audio.source = "wifi_udp";
    snapshot.audio.control_state = "ready loops=4294967295 hb=4294967295";
    snapshot.audio.last_error = std::string(240, 'x');
    snapshot.audio.sent_packets =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.sent_bytes = std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.last_rms_milli =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.peak_rms_milli =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.send_errors =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.read_errors =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.recovery_count =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.session_generation =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.audio.session_id =
        std::numeric_limits<std::uint64_t>::max();
    snapshot.audio.stream_phase = std::string(64, 'p');
    snapshot.audio.stop_reason = std::string(64, 'r');
    snapshot.audio.stream_host = "192.168.31.80";
    snapshot.audio.stream_port = 17333;
    snapshot.power.mode = "awake";
    snapshot.power.inactive_ms =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.power.deep_sleep_block = std::string(64, 'd');
    snapshot.power.last_wake = std::string(64, 'w');

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("control_state":")") != std::string::npos);
    assert(json.find(R"("semantic_actions")") == std::string::npos);
    assert(json.find(R"("power")") == std::string::npos);
    record("compact minimal audio", json, false);
  }

  // 5. Diagnostics-only compact view (long-press branch, no audio state).
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "0.4.30-idf-v2-readable-status";
    snapshot.phase = "diag";
    snapshot.status =
        "wifi=ready ds=config_window state=awake up=4294967s vin=1";
    snapshot.saved = true;
    snapshot.battery_mv = 3508;
    snapshot.target_platform = "windows";
    snapshot.power = {
        "awake", std::numeric_limits<std::uint32_t>::max(),
        "config_window", "encoder_long_press",
        std::numeric_limits<std::uint32_t>::max(), false,
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max(), true, "status_read",
    };
    snapshot.diagnostics.board = "v2";
    snapshot.diagnostics.keys = "11101111";
    snapshot.diagnostics.encoder = "101";
    snapshot.diagnostics.pinmap =
        "K1=2,K2=47,K3=38,K4=41,K5=1,K6=6,K7=7,K8=48,E=17/16/18";
    snapshot.diagnostics.raw_gpio = "2:1,47:1,38:1,41:1,1:1,6:1,7:1";
    snapshot.diagnostics.last_input = "KEY2:pressed:gpio=47";
    snapshot.diagnostics.last_input_age_ms = 1;
    snapshot.diagnostics.key_wake = 8;

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("diag":{"compact":true,"board":"v2")") !=
           std::string::npos);
    record("compact diagnostics", json, false);
  }

  // 6. Speaker probe: every versioned metric remains; the dedicated firmware
  //    budget is 16 bytes and the capability is still declared.
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = std::string(40, 'f');
    snapshot.phase = "spk_probe";
    snapshot.status = "cancelled";
    snapshot.bytes = std::numeric_limits<std::uint16_t>::max();
    snapshot.crc16 = std::numeric_limits<std::uint16_t>::max();
    snapshot.saved = true;
    snapshot.target_platform = "windows";
    const auto speaker = worst_case_speaker_probe();
    snapshot.speaker = &speaker;

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("firmware":")" + std::string(16, 'f') + R"(")") !=
           std::string::npos);
    assert(json.find(std::string(17, 'f')) == std::string::npos);
    assert(json.find(
               R"("capabilities":{"config_max_bytes":2048,"host_action_v1":true})") !=
           std::string::npos);
    assert(json.find(R"("semantic_actions")") == std::string::npos);
    const char* required_metrics[] = {
        R"("v":1)",       R"("g":4294967295)", R"("st":21)",
        R"("r":6)",       R"("e":19)",         R"("x":-2147483648)",
        R"("mg":4294967295)", R"("fu":4294967295)",
        R"("du":4294967295)", R"("mu":4294967295)",
        R"("n":4294967295)", R"("p":4294967295)",
        R"("sw":4294967295)", R"("h0":4294967295)",
        R"("h1":4294967295)", R"("hl":4294967295)",
        R"("hm":4294967295)", R"("pk":4294967295)",
        R"("rm":4294967295)",
    };
    for (const auto* metric : required_metrics) {
      assert(json.find(metric) != std::string::npos);
    }
    record("speaker probe", json, false);
  }

  // 7. Config confirmation (push response): diagnostics/power omitted, the
  //    capability stays.
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "0.4.24-idf-v2-host-platform";
    snapshot.phase = "push";
    snapshot.status = "ok";
    snapshot.bytes = 979;
    snapshot.crc16 = 0xA55A;
    snapshot.saved = true;
    snapshot.target_platform = "windows";
    snapshot.diagnostics.board = "v2";

    const auto json =
        ai_keyboard::build_config_confirmation_status_json(snapshot);
    assert(json.find(R"("audio")") == std::string::npos);
    assert(json.find(R"("power")") == std::string::npos);
    assert(json.find(R"("diag")") == std::string::npos);
    record("confirmation", json, false);
  }

  // 8. Compact battery refresh: current power truth is present and the
  //    worst-case simple BLE overlay really gets appended (the published
  //    battery wire fits, rather than only the base JSON).
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "v";
    snapshot.phase = "battery";
    snapshot.status = "ok";
    snapshot.saved = true;
    snapshot.battery_mv = 3790;
    snapshot.battery_percent = 50;
    snapshot.power.mode = "awake";
    snapshot.power.inactive_ms = 1;
    snapshot.power.deep_sleep_block = "idle";
    snapshot.power.last_wake = "k";

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("phase":"battery")") != std::string::npos);
    assert(json.find(R"("battery_state")") == std::string::npos);
    assert(json.find(R"("cycle_seq")") == std::string::npos);
    assert(json.find(R"("power":{"compact":true,"mode":"awake")") !=
           std::string::npos);
    const auto wire = final_ble_wire_for(json, true);
    assert(wire.find(R"("ble":{"connected":1)") != std::string::npos);
    assert(wire.size() > json.size());
    record("battery compact + ble", json, true);
  }

  // 9. Battery refresh at counter/string limits: intermediate fallback keeps
  //    current power and sheds only the four cycle fields first.
  {
    ai_keyboard::ConfigStatusSnapshot snapshot;
    snapshot.firmware = "0.4.30-idf-v2-readable-status";
    snapshot.phase = "battery";
    snapshot.status = std::string(64, 's');
    snapshot.bytes = std::numeric_limits<std::uint16_t>::max();
    snapshot.crc16 = std::numeric_limits<std::uint16_t>::max();
    snapshot.battery_mv = std::numeric_limits<std::uint16_t>::max();
    snapshot.battery_percent =
        std::numeric_limits<std::uint8_t>::max();
    snapshot.saved = true;
    snapshot.target_platform = "windows";
    snapshot.battery = {
        std::numeric_limits<std::uint16_t>::max(),
        std::string(64, 'b'),
        std::numeric_limits<std::uint32_t>::max(),
        true,
    };
    snapshot.power.mode = "awake";
    snapshot.power.inactive_ms =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.power.deep_sleep_block = std::string(64, 'd');
    snapshot.power.last_wake = std::string(64, 'w');
    snapshot.power.cycle_seq =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.power.cycle_inactive_ms =
        std::numeric_limits<std::uint32_t>::max();
    snapshot.power.cycle_deep_sleep = true;
    snapshot.power.cycle_wake = std::string(64, 'c');

    const auto json = ai_keyboard::build_config_status_json(snapshot);
    assert(json.find(R"("power":{"compact":true,"mode":"awake")") !=
           std::string::npos);
    assert(json.find(R"("inactive_ms":4294967295)") !=
           std::string::npos);
    assert(json.find(
               R"("deep_sleep_block":"dddddddddddddddddddddddd")") !=
           std::string::npos);
    assert(json.find(R"("last_wake":"wwwwwwwwwwww")") !=
           std::string::npos);
    assert(json.find(R"("cycle_seq")") == std::string::npos);
    assert(json.find(R"("cycle_inactive_ms")") == std::string::npos);
    assert(json.find(R"("cycle_deep_sleep")") == std::string::npos);
    assert(json.find(R"("cycle_wake")") == std::string::npos);
    record("battery bounded base", json, true);
  }

  // 10. Shared fallback constant, published when even the selected bounded
  //     status cannot be produced. It is one self-contained document.
  {
    const std::string fallback(ai_keyboard::kConfigStatusFallbackJson);
    assert(fallback.find(
               R"("capabilities":{"config_max_bytes":2048,"host_action_v1":true})") !=
           std::string::npos);
    assert(fallback.size() <= ai_keyboard::kConfigStatusGattSafeLen);
    assert_host_action_capability_boolean_once(fallback);
    rows[row_count++] = {"fallback constant", fallback.size(), 0};

    // When BLE publishes the fallback it goes through the same detailed
    // overlay encoder; the complete result must remain bounded.
    const auto wire = final_ble_wire_for(fallback, false);
    assert(wire.size() <= ai_keyboard::kConfigStatusGattSafeLen);
    rows[row_count - 1].final_ble_bytes = wire.size();
  }

  // Measured byte budget for every actually published variant. These are the
  // serialized UTF-8 byte counts, not source constants.
  std::size_t largest = 0;
  std::printf("[T08] host_action_v1 published variant UTF-8 byte budget:\n");
  for (unsigned i = 0; i < row_count; ++i) {
    std::printf("  %-28s raw=%3u final_ble=%3u\n",
                rows[i].name,
                static_cast<unsigned>(rows[i].raw_bytes),
                static_cast<unsigned>(rows[i].final_ble_bytes));
    if (rows[i].final_ble_bytes > largest) {
      largest = rows[i].final_ble_bytes;
    }
  }
  std::printf("  max=%u limit=512 headroom=%d\n",
              static_cast<unsigned>(largest),
              static_cast<int>(ai_keyboard::kConfigStatusGattSafeLen -
                               largest));
  assert(largest <= ai_keyboard::kConfigStatusGattSafeLen);
}

}  // namespace

int main() {
  host_action_v1_is_boolean_once_in_every_published_variant();
  return 0;
}
