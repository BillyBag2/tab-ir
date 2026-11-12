#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace remote {
// Stable button identifiers are used as indexes in the saved profile format.
inline constexpr std::array<const char*, 24> kButtons = {
    "Power", "Mute", "Volume +", "Volume -", "Channel +", "Channel -",
    "Up", "Down", "Left", "Right", "OK", "Back", "Menu", "Home",
    "Input", "Play/Pause", "Stop", "Record", "Red", "Green", "Yellow",
    "Blue", "Custom 1", "Custom 2"};
inline constexpr size_t kMaxProfiles = 8;
inline constexpr size_t kMaxDurations = 1024;
struct Message {
    // Alternating mark/space timings in microseconds, starting with an LED-on mark.
    std::vector<uint16_t> pulses;
    bool empty() const { return pulses.empty(); }
};
struct Profile {
    std::string name;
    std::array<Message, kButtons.size()> messages;
};
bool validMessage(const Message& message);
bool encodeProfile(const Profile& profile, std::vector<uint8_t>& bytes);
bool decodeProfile(const std::vector<uint8_t>& bytes, Profile& profile);
}
