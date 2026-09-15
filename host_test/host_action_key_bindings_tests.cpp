#include <cassert>
#include <string>

#include "keyboard/config_payload.h"
#include "keyboard/keymap.h"

using ai_keyboard::ActionKind;
using ai_keyboard::ConfigParseStatus;
using ai_keyboard::InputId;

// T09: these KEY1-KEY8 Host Action binding tests were authored at T04 inside
// config_payload_tests.cpp; they are relocated here verbatim so the course
// contract target host_action_key_bindings_tests is a registered CTest test.
// No assertion was added, removed or weakened during the move.
namespace {

// T04 fixed example UUID (version 1). Test fixture only; never a default
// keymap entry, production config or real application mapping.
static constexpr const char* kT04ExampleHostAction =
    "host_action:123e4567-e89b-12d3-a456-426614174000";

std::string payload_with_action_on_key(int key_number,
                                       const std::string& action_json) {
  std::string keys;
  for (int candidate = 1; candidate <= 8; ++candidate) {
    const std::string action =
        (candidate == key_number) ? action_json : "\"disabled\"";
    keys += "        \"KEY" + std::to_string(candidate) +
            "\":{\"press\":" + action + (candidate == 8 ? "}\n" : "},\n");
  }
  return std::string(R"({
    "schema":"ai_keyboard.v1",
    "audio_enabled":false,
    "profiles":[{
      "id":"default",
      "keys":{
)") + keys + R"(
      },
      "encoder":{"left":"disabled","right":"disabled","press":"disabled"}
    }]
  })";
}

void assert_press_emits_one_host_action_and_release_none(
    const ai_keyboard::Action& action) {
  const auto press = ai_keyboard::event_for_action(
      action, ai_keyboard::InputPhase::Pressed, "", "");
  const auto release = ai_keyboard::event_for_action(
      action, ai_keyboard::InputPhase::Released, "", "");

  // Exactly one Host Action on press, carrying the complete config value
  // including the host_action: prefix retained through the Keymap.
  assert(press.kind == ai_keyboard::FirmwareEventKind::HostAction);
  assert(press.value == kT04ExampleHostAction);
  assert(press.value.rfind("host_action:", 0) == 0);
  assert(!press.bridge_app_hotkey);

  // Release emits no second action.
  assert(release.kind == ai_keyboard::FirmwareEventKind::None);
  assert(release.value.empty());
}

void each_of_the_eight_keys_parses_host_action_through_shared_path() {
  for (int key_number = 1; key_number <= 8; ++key_number) {
    const std::string json =
        payload_with_action_on_key(key_number, std::string("\"") +
                                                   kT04ExampleHostAction +
                                                   "\"");
    const auto result = ai_keyboard::parse_config_payload(json);
    assert(result.status == ConfigParseStatus::Ok);

    const auto input = static_cast<InputId>(key_number - 1);
    const ai_keyboard::Action& configured =
        result.config.keymap.action_for(input);
    assert(configured.kind == ActionKind::HostAction);
    assert(configured.host_action == kT04ExampleHostAction);

    assert_press_emits_one_host_action_and_release_none(configured);

    // The other seven keys in this payload stay disabled: proves parsing is
    // per-key independent yet routed through the same shared code path.
    for (int other = 1; other <= 8; ++other) {
      if (other == key_number) {
        continue;
      }
      const auto other_action =
          result.config.keymap.action_for(static_cast<InputId>(other - 1));
      assert(other_action.kind == ActionKind::Disabled);
    }
  }
}

void all_eight_keys_coexist_as_host_actions_in_one_profile() {
  std::string keys;
  for (int key_number = 1; key_number <= 8; ++key_number) {
    keys += "        \"KEY" + std::to_string(key_number) +
            "\":{\"press\":\"" + kT04ExampleHostAction + "\"" +
            (key_number == 8 ? "}\n" : "},\n");
  }
  const std::string json = std::string(R"({
    "schema":"ai_keyboard.v1",
    "audio_enabled":false,
    "profiles":[{
      "id":"default",
      "keys":{
)") + keys + R"(
      },
      "encoder":{"left":"disabled","right":"disabled","press":"disabled"}
    }]
  })";

  const auto result = ai_keyboard::parse_config_payload(json);
  assert(result.status == ConfigParseStatus::Ok);
  for (int key_number = 1; key_number <= 8; ++key_number) {
    const auto& action =
        result.config.keymap.action_for(static_cast<InputId>(key_number - 1));
    assert(action.kind == ActionKind::HostAction);
    assert_press_emits_one_host_action_and_release_none(action);
  }
}

void host_action_coexists_with_legacy_actions_without_regression() {
  // One mixed profile: Host Action next to hotkey, semantic copy/paste/undo
  // and fixed text, all passing the same parser and event mapping.
  const std::string json = R"({
    "schema":"ai_keyboard.v1",
    "audio_enabled":false,
    "profiles":[{
      "id":"default",
      "keys":{
        "KEY1":{"press":")" + std::string(kT04ExampleHostAction) + R"("},
        "KEY2":{"press":{"hotkey":"Return"}},
        "KEY3":{"press":{"text":"hi"}},
        "KEY4":{"press":"disabled"},
        "KEY5":{"press":"select_all"},
        "KEY6":{"press":"copy"},
        "KEY7":{"press":"paste"},
        "KEY8":{"press":"undo"}
      },
      "encoder":{"left":"disabled","right":"disabled","press":"disabled"}
    }]
  })";

  const auto result = ai_keyboard::parse_config_payload(json);
  assert(result.status == ConfigParseStatus::Ok);
  const auto& keymap = result.config.keymap;

  assert_press_emits_one_host_action_and_release_none(
      keymap.action_for(InputId::Key1));

  const auto return_down = ai_keyboard::event_for_action(
      keymap.action_for(InputId::Key2), ai_keyboard::InputPhase::Pressed,
      "", "");
  assert(return_down.kind == ai_keyboard::FirmwareEventKind::HidKeyDown);
  assert(return_down.value == "Return");

  const auto text_down = ai_keyboard::event_for_action(
      keymap.action_for(InputId::Key3), ai_keyboard::InputPhase::Pressed,
      "", "");
  assert(text_down.kind == ai_keyboard::FirmwareEventKind::FixedText);
  assert(text_down.value == "hi");
  assert(ai_keyboard::event_for_action(
             keymap.action_for(InputId::Key3),
             ai_keyboard::InputPhase::Released, "", "")
             .kind == ai_keyboard::FirmwareEventKind::None);

  assert(keymap.action_for(InputId::Key4).kind == ActionKind::Disabled);

  // Semantic actions keep their MacOS chord regression behavior.
  const struct {
    InputId input;
    const char* chord;
  } semantics[] = {
      {InputId::Key5, "Meta+A"},
      {InputId::Key6, "Meta+C"},
      {InputId::Key7, "Meta+V"},
      {InputId::Key8, "Meta+Z"},
  };
  for (const auto& item : semantics) {
    const auto event = ai_keyboard::event_for_action(
        keymap.action_for(item.input), ai_keyboard::InputPhase::Pressed,
        "", "", ai_keyboard::HostPlatform::MacOS);
    assert(event.kind == ai_keyboard::FirmwareEventKind::HidKeyDown);
    assert(event.value == item.chord);
  }
}

}  // namespace

int main() {
  each_of_the_eight_keys_parses_host_action_through_shared_path();
  all_eight_keys_coexist_as_host_actions_in_one_profile();
  host_action_coexists_with_legacy_actions_without_regression();
  return 0;
}
