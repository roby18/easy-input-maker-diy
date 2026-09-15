#include "keyboard/host_action_protocol.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "keyboard/fixed_text_protocol.h"
#include "keyboard/status_hid_protocol.h"

namespace ai_keyboard {
namespace {

// 0-based hyphen positions of the 8-4-4-4-12 layout. From 1 they are
// positions 9, 14, 19 and 24.
constexpr std::size_t kUuidHyphenPositions[] = {8, 13, 18, 23};

bool is_lowercase_hex_digit(char value) {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f');
}

bool is_hyphen_position(std::size_t index) {
  for (const std::size_t position : kUuidHyphenPositions) {
    if (index == position) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool is_canonical_host_action_uuid(std::string_view uuid) {
  if (uuid.size() != kHostActionUuidTextLen) {
    return false;
  }
  for (std::size_t index = 0; index < uuid.size(); ++index) {
    const char value = uuid[index];
    if (is_hyphen_position(index)) {
      if (value != '-') {
        return false;
      }
    } else if (!is_lowercase_hex_digit(value)) {
      // Uppercase A-F is rejected here on purpose; v1 never normalizes.
      return false;
    }
  }
  return true;
}

bool is_canonical_host_action_value(std::string_view value) {
  if (value.size() !=
      kHostActionConfigPrefix.size() + kHostActionUuidTextLen) {
    return false;
  }
  if (value.substr(0, kHostActionConfigPrefix.size()) !=
      kHostActionConfigPrefix) {
    return false;
  }
  return is_canonical_host_action_uuid(
      value.substr(kHostActionConfigPrefix.size()));
}

std::string_view host_action_uuid_body(std::string_view value) {
  if (!is_canonical_host_action_value(value)) {
    return {};
  }
  return value.substr(kHostActionConfigPrefix.size());
}

bool encode_host_action_app_command(
    std::string_view host_action_value,
    std::array<std::uint8_t, kHostActionAppCommandPayloadLen>* out) {
  if (out == nullptr) {
    return false;
  }
  const std::string_view body =
      host_action_uuid_body(host_action_value);
  if (body.size() != kHostActionUuidTextLen) {
    return false;
  }

  std::array<std::uint8_t, kHostActionAppCommandPayloadLen> report{};
  report[0] = kHostActionAppCommandKind;
  report[1] = kHostActionChunkIndex;
  report[2] = kHostActionTotalChunks;
  report[3] = kHostActionDataLen;
  std::copy_n(body.data(), body.size(),
              report.begin() + kHostActionAppCommandHeaderLen);
  // Remaining bytes stay value-initialized zero: [40..62] is only the
  // existing container remainder, never Host Action data.
  *out = std::move(report);
  return true;
}

// Host Action reuses the existing fixed-text App Command container instead of
// inventing a parallel report shape.
static_assert(kHostActionAppCommandReportId ==
              kFixedTextAppCommandReportId);
static_assert(kHostActionAppCommandPayloadLen ==
              kFixedTextAppCommandPayloadLen);
static_assert(kHostActionAppCommandHeaderLen ==
              kFixedTextAppCommandHeaderLen);
static_assert(kHostActionAppCommandHeaderLen + kHostActionDataLen == 40);
// Kind 0x04 stays exclusive to the status response.
static_assert(kHostActionAppCommandKind != kStatusResponseCommandKind);
static_assert(kHostActionAppCommandKind == 0x05);
static_assert(kStatusResponseCommandKind == 0x04);

}  // namespace ai_keyboard
