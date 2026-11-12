#include "ir_decoder.hpp"
#include "keyboard_keys.hpp"
#include <cassert>
#include <cstdio>

namespace {
remote::Message distance(uint32_t raw, uint16_t header = 9000) {
    remote::Message message;
    message.pulses = {header, 4500};
    for (unsigned i = 0; i < 32; ++i) {
        message.pulses.push_back(560);
        message.pulses.push_back(raw & (uint32_t(1) << i) ? 1690 : 560);
    }
    message.pulses.push_back(560);
    return message;
}
remote::Message sony(uint16_t address, uint8_t command, unsigned bits) {
    remote::Message message;
    uint32_t raw = (uint32_t(address) << 7) | command;
    message.pulses = {2400, 600};
    for (unsigned i = 0; i < bits; ++i) {
        message.pulses.push_back(raw & (uint32_t(1) << i) ? 1200 : 600);
        if (i + 1 < bits) message.pulses.push_back(600);
    }
    return message;
}
}
int main() {
    using remote::Protocol;
    auto frame = distance(0xcb34ef10);
    auto decoded = remote::decodeIr(frame);
    assert(decoded.protocol == Protocol::Nec && decoded.address == 0x10 && decoded.command == 0x34);
    assert(decoded.bits == 32 && decoded.raw == 0xcb34ef10);
    assert(remote::describeIr(frame).find("Command 0x34") != std::string::npos);
    // Saved version-1 waveform blobs need no migration to expose decoded fields.
    remote::Profile profile, restored;
    profile.name = "Test remote";
    profile.messages[0] = frame;
    std::vector<uint8_t> stored;
    assert(remote::encodeProfile(profile, stored));
    assert(remote::decodeProfile(stored, restored));
    assert(remote::decodeIr(restored.messages[0]).raw == decoded.raw);
    decoded = remote::decodeIr(distance(0xcb341234));
    assert(decoded.protocol == Protocol::NecExtended && decoded.address == 0x1234);
    decoded = remote::decodeIr(distance(0xf8070707, 4500));
    assert(decoded.protocol == Protocol::Samsung32 && decoded.address == 0x0707 && decoded.command == 7);
    // Bad complement bytes, marks, spaces, stop marks and partial frames must not
    // produce plausible-looking decoded commands.
    assert(remote::decodeIr(distance(0xca34ef10)).protocol == Protocol::Unknown);
    for (size_t i = 0; i < frame.pulses.size(); ++i) {
        auto corrupted = frame;
        corrupted.pulses[i] = 100;
        assert(remote::decodeIr(corrupted).protocol == Protocol::Unknown);
    }
    for (size_t size = 0; size < frame.pulses.size(); ++size) {
        auto truncated = frame;
        truncated.pulses.resize(size);
        assert(remote::decodeIr(truncated).protocol == Protocol::Unknown);
    }
    auto jittered = frame;
    for (size_t i = 0; i < jittered.pulses.size(); ++i)
        jittered.pulses[i] = jittered.pulses[i] * (i % 2 ? 115 : 85) / 100;
    assert(remote::decodeIr(jittered).raw == 0xcb34ef10);
    for (unsigned bits : {12u, 15u, 20u}) {
        uint16_t address = bits == 12 ? 0x1a : bits == 15 ? 0xba : 0x1aba;
        auto message = sony(address, 0x55, bits);
        decoded = remote::decodeIr(message);
        assert(decoded.protocol == Protocol::Sony && decoded.bits == bits);
        assert(decoded.address == address && decoded.command == 0x55);
        message.pulses[3] = 1500;
        assert(remote::decodeIr(message).protocol == Protocol::Unknown);
    }
    assert(remote::decodeIr({{9000, 2250, 560}}).protocol == Protocol::Unknown);
    assert(remote::describeIr({}).find("No message") != std::string::npos);

    assert(decodeKeyboardKey(0, 4).character == 'a');
    assert(decodeKeyboardKey(0x02, 4).character == 'A');
    assert(decodeKeyboardKey(0x20, 29).character == 'Z');
    assert(decodeKeyboardKey(0, 39).character == '0');
    assert(decodeKeyboardKey(2, 30).character == '!');
    assert(decodeKeyboardKey(1, 17).control);  // Ctrl+N
    assert(decodeKeyboardKey(0x10, 17).control);
    assert(decodeKeyboardKey(4, 4).alt);
    assert(decodeKeyboardKey(0, 40).key == Key::Enter);
    assert(decodeKeyboardKey(0, 41).key == Key::Escape);
    assert(decodeKeyboardKey(0, 42).key == Key::Backspace);
    assert(decodeKeyboardKey(0, 76).key == Key::Delete);
    assert(decodeKeyboardKey(0, 79).key == Key::Right);
    assert(decodeKeyboardKey(0, 82).key == Key::Up);
    assert(decodeKeyboardKey(0, 49).character == '\\');
    assert(decodeKeyboardKey(2, 52).character == '"');
    assert(decodeKeyboardKey(0, 0).key == Key::None);
    assert(decodeKeyboardKey(255, 255).key == Key::None);
    puts("IR protocol, corruption, timing-tolerance and keyboard mapping tests passed.");
}
