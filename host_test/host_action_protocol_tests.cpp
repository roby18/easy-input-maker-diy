#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "keyboard/fixed_text_protocol.h"
#include "keyboard/host_action_protocol.h"
#include "keyboard/status_hid_protocol.h"

using ai_keyboard::kHostActionAppCommandHeaderLen;
using ai_keyboard::kHostActionAppCommandKind;
using ai_keyboard::kHostActionAppCommandPayloadLen;
using ai_keyboard::kHostActionAppCommandReportId;
using ai_keyboard::kHostActionChunkIndex;
using ai_keyboard::kHostActionConfigPrefix;
using ai_keyboard::kHostActionDataLen;
using ai_keyboard::kHostActionTotalChunks;
using ai_keyboard::kHostActionUuidTextLen;

namespace {

// Fixed example UUIDs exist only inside host tests (T02 contract). They are
// not application mappings, defaults, or firmware identities.
constexpr const char* kTestUuidLower =
    "123e4567-e89b-42d3-a456-426614174000";
constexpr const char* kTestValueLower =
    "host_action:123e4567-e89b-42d3-a456-426614174000";
constexpr const char* kTestNilUuid =
    "00000000-0000-0000-0000-000000000000";
constexpr const char* kTestMaxUuid =
    "ffffffff-ffff-ffff-ffff-ffffffffffff";

bool accepts(std::string_view uuid) {
  return ai_keyboard::is_canonical_host_action_uuid(uuid);
}

void accepts_canonical_lowercase_uuids_without_version_or_nil_limits() {
  assert(accepts(kTestUuidLower));
  // No nil UUID restriction: the all-zero UUID is still canonical.
  assert(accepts(kTestNilUuid));
  // No UUID version restriction: every lowercase hex digit is legal at the
  // version and variant positions.
  assert(accepts("00000000-0000-0000-0000-00000000000f"));
  assert(accepts("123e4567-e89b-02d3-f456-426614174000"));
  assert(accepts(kTestMaxUuid));
}

void rejects_uppercase_without_lowercasing() {
  // Same UUID as kTestUuidLower with one uppercase hex digit: reject, never
  // auto-normalize to lowercase.
  const std::string uppercase =
      "123E4567-e89b-42d3-a456-426614174000";
  assert(!accepts(uppercase));
  assert(accepts(kTestUuidLower));
  assert(!accepts("123e4567-e89b-42d3-A456-426614174000"));
  assert(!accepts("123e4567-E89B-42D3-A456-426614174000"));
}

void rejects_wrong_lengths() {
  assert(!accepts(""));
  assert(!accepts("123e4567-e89b-42d3-a456-42661417400"));   // 35
  assert(!accepts("123e4567-e89b-42d3-a456-4266141740000")); // 37
  assert(!accepts("123e4567e89b42d3a456426614174000"));      // no hyphens, 32
}

void rejects_wrong_hyphen_positions() {
  // Same 36 characters as kTestUuidLower, but the first hyphen is swapped
  // with the following digit (hyphens at 9/13/18/23 instead of 8/13/18/23).
  assert(!accepts("123e4567e-89b-42d3-a456-426614174000"));
  // Hyphen replaced by a digit at slot 8: length stays 36 but layout fails.
  assert(!accepts("123e45670e89b-42d3-a456-426614174000"));
  // Extra hyphen in the final digit group.
  assert(!accepts("123e4567-e89b-42d3-a456-42661417-4000"));
}

void rejects_illegal_characters() {
  assert(!accepts("g23e4567-e89b-42d3-a456-426614174000"));  // 'g'
  assert(!accepts("123e4567-e89b-42d3-a456-42661417400 "));  // trailing space
  assert(!accepts("123e4567_e89b-42d3-a456-426614174000"));  // underscore
  assert(!accepts("123e4567 e89b-42d3-a456-426614174000"));  // space for '-'
}

void config_value_requires_exact_prefix_and_canonical_body() {
  assert(ai_keyboard::is_canonical_host_action_value(kTestValueLower));
  assert(ai_keyboard::is_canonical_host_action_value(
      std::string(kHostActionConfigPrefix) + kTestNilUuid));
  // Bare UUID without the host_action: prefix is not a config value.
  assert(!ai_keyboard::is_canonical_host_action_value(kTestUuidLower));
  // Prefix alone or a wrong-cased prefix is rejected.
  assert(!ai_keyboard::is_canonical_host_action_value("host_action:"));
  assert(!ai_keyboard::is_canonical_host_action_value(
      "Host_Action:123e4567-e89b-42d3-a456-426614174000"));
  // Prefix plus any non-canonical body is rejected.
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:123E4567-e89b-42d3-a456-426614174000"));
}

void body_helper_returns_prefix_free_uuid() {
  const std::string_view body =
      ai_keyboard::host_action_uuid_body(kTestValueLower);
  assert(body == kTestUuidLower);
  assert(body.size() == kHostActionUuidTextLen);
  assert(body.find(':') == std::string_view::npos);
  // Invalid input yields an empty body instead of a partially stripped value.
  assert(ai_keyboard::host_action_uuid_body("host_action:bad").empty());
  assert(ai_keyboard::host_action_uuid_body(kTestUuidLower).empty());
}

void encoded_report_matches_the_frozen_v1_byte_layout() {
  std::array<std::uint8_t, kHostActionAppCommandPayloadLen> report{};
  assert(ai_keyboard::encode_host_action_app_command(kTestValueLower, &report));

  static_assert(kHostActionAppCommandReportId == 0x11);
  static_assert(kHostActionAppCommandKind == 0x05);
  static_assert(kHostActionChunkIndex == 0);
  static_assert(kHostActionTotalChunks == 1);
  static_assert(kHostActionDataLen == 36);
  static_assert(kHostActionAppCommandHeaderLen == 4);
  static_assert(kHostActionAppCommandPayloadLen == 63);

  assert(report[0] == 0x05);
  assert(report[1] == 0);
  assert(report[2] == 1);
  assert(report[3] == 36);

  // Data area [4..39] is exactly the 36 ASCII UUID bytes with no prefix.
  const std::string_view data(
      reinterpret_cast<const char*>(report.data() + 4), kHostActionDataLen);
  assert(data == kTestUuidLower);
  for (std::size_t i = 4; i < 40; ++i) {
    assert(report[i] != ':');
  }

  // [40..62] is only the existing 63-byte container remainder and is
  // deterministically zero padded.
  for (std::size_t i = 40; i < report.size(); ++i) {
    assert(report[i] == 0);
  }
}

void encoding_reuses_the_existing_app_command_container() {
  // Host Action rides the same Report ID / 63-byte / 4-byte-header container
  // as fixed text instead of defining a parallel wire format.
  static_assert(
      kHostActionAppCommandReportId ==
      ai_keyboard::kFixedTextAppCommandReportId);
  static_assert(
      kHostActionAppCommandPayloadLen ==
      ai_keyboard::kFixedTextAppCommandPayloadLen);
  static_assert(
      kHostActionAppCommandHeaderLen ==
      ai_keyboard::kFixedTextAppCommandHeaderLen);
  static_assert(
      kHostActionAppCommandHeaderLen + kHostActionDataLen == 40);
  // kind 0x04 stays owned by the status response; Host Action uses 0x05.
  static_assert(
      kHostActionAppCommandKind != ai_keyboard::kStatusResponseCommandKind);
  static_assert(ai_keyboard::kStatusResponseCommandKind == 0x04);
}

void encoding_fails_closed_for_invalid_values() {
  std::array<std::uint8_t, kHostActionAppCommandPayloadLen> report{};
  const auto before = report;
  assert(!ai_keyboard::encode_host_action_app_command(
      "host_action:123E4567-e89b-42d3-a456-426614174000", &report));
  assert(!ai_keyboard::encode_host_action_app_command(
      "host_action:123e4567-e89b-42d3-a456-42661417400", &report));
  assert(!ai_keyboard::encode_host_action_app_command(
      kTestUuidLower, &report));
  assert(!ai_keyboard::encode_host_action_app_command("", &report));
  assert(!ai_keyboard::encode_host_action_app_command(kTestValueLower, nullptr));
  // A rejected value must not have partially populated the report.
  assert(report == before);
}

}  // namespace

int main() {
  accepts_canonical_lowercase_uuids_without_version_or_nil_limits();
  rejects_uppercase_without_lowercasing();
  rejects_wrong_lengths();
  rejects_wrong_hyphen_positions();
  rejects_illegal_characters();
  config_value_requires_exact_prefix_and_canonical_body();
  body_helper_returns_prefix_free_uuid();
  encoded_report_matches_the_frozen_v1_byte_layout();
  encoding_reuses_the_existing_app_command_container();
  encoding_fails_closed_for_invalid_values();
  return 0;
}
