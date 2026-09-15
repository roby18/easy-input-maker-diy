#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ai_keyboard {

// Host Action v1 frozen wire/config contract. See
// flow/tasks/T02-Host-Action-v1-固定兼容协议.md. This header is pure logic:
// it never touches USB, BLE, NVS, or any ESP-IDF API so both transports can
// share one encoder and host tests can exercise it directly.

// Config layer value, kept complete (prefix included) in the keymap and
// persisted config. The prefix is removed only while encoding the wire data.
inline constexpr std::string_view kHostActionConfigPrefix = "host_action:";

// Canonical lowercase UUID body: 8-4-4-4-12 characters, always 36 bytes.
inline constexpr std::size_t kHostActionUuidTextLen = 36;

// App Command report values (frozen).
inline constexpr std::uint8_t kHostActionAppCommandReportId = 0x11;
inline constexpr std::uint8_t kHostActionAppCommandKind = 0x05;
inline constexpr std::uint8_t kHostActionChunkIndex = 0;
inline constexpr std::uint8_t kHostActionTotalChunks = 1;
inline constexpr std::uint8_t kHostActionDataLen =
    static_cast<std::uint8_t>(kHostActionUuidTextLen);
inline constexpr std::size_t kHostActionAppCommandPayloadLen = 63;
inline constexpr std::size_t kHostActionAppCommandHeaderLen = 4;

// Strict canonical-lowercase check of a bare 36-character UUID body.
// Accepts nil and every version digit; rejects uppercase, wrong length,
// wrong hyphen positions and illegal characters without normalizing them.
bool is_canonical_host_action_uuid(std::string_view uuid);

// Validates a complete config value: exactly "host_action:" plus a canonical
// lowercase UUID body.
bool is_canonical_host_action_value(std::string_view value);

// Returns the prefix-free UUID body of a validated config value, or an empty
// view when the value is not canonical.
std::string_view host_action_uuid_body(std::string_view value);

// Builds one 63-byte Report ID 0x11 payload for a validated
// "host_action:<uuid>" config value: kind 0x05, chunk 0/1, data length 36 and
// the 36 bare UUID ASCII bytes at [4..39]; [40..62] stay zero as container
// remainder. Returns false (fail closed) for any non-canonical value and does
// not partially populate the report. The caller supplies Report ID 0x11 to
// the HID stack, matching the existing App Command calling convention.
bool encode_host_action_app_command(
    std::string_view host_action_value,
    std::array<std::uint8_t, kHostActionAppCommandPayloadLen>* out);

}  // namespace ai_keyboard
