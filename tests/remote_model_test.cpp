#include "remote_model.hpp"
#include <cassert>
#include <cstdio>

int main() {
    remote::Profile original;
    original.name = "RM-ED060";
    original.messages[0].pulses = {9000, 4500, 560, 560, 560, 1690, 560};
    original.messages[23].pulses = {2400, 600, 600, 1200, 600, 600, 600};
    std::vector<uint8_t> encoded;
    assert(remote::encodeProfile(original, encoded));
    remote::Profile decoded;
    assert(remote::decodeProfile(encoded, decoded));
    assert(decoded.name == original.name);
    for (size_t i = 0; i < remote::kButtons.size(); ++i)
        assert(decoded.messages[i].pulses == original.messages[i].pulses);

    // Every truncated blob must fail without altering the last valid profile.
    for (size_t size = 0; size < encoded.size(); ++size) {
        std::vector<uint8_t> truncated(encoded.begin(), encoded.begin() + size);
        assert(!remote::decodeProfile(truncated, decoded));
        assert(decoded.name == original.name);
        assert(decoded.messages[0].pulses == original.messages[0].pulses);
    }
    auto invalid = encoded;
    invalid[3] = 2;
    assert(!remote::decodeProfile(invalid, decoded));
    invalid = encoded;
    invalid.push_back(0);
    assert(!remote::decodeProfile(invalid, decoded));
    invalid = encoded;
    size_t countOffset = 5 + original.name.size();
    invalid[countOffset] = 255;
    invalid[countOffset + 1] = 255;
    assert(!remote::decodeProfile(invalid, decoded));
    invalid = encoded;
    invalid[countOffset + 2] = invalid[countOffset + 3] = 0;
    assert(!remote::decodeProfile(invalid, decoded));

    // Exercise the largest possible stored profile and capacity boundaries.
    original.name.assign(32, 'X');
    for (auto& message : original.messages) message.pulses.assign(remote::kMaxDurations, 32767);
    assert(remote::encodeProfile(original, encoded));
    assert(remote::decodeProfile(encoded, decoded));
    assert(decoded.messages[23].pulses.size() == remote::kMaxDurations);
    original.messages[0].pulses.push_back(1);
    assert(!remote::encodeProfile(original, encoded));
    original.messages[0].pulses = {1, 2};
    assert(!remote::encodeProfile(original, encoded));
    original.messages[0].pulses.clear();
    original.name.clear();
    assert(!remote::encodeProfile(original, encoded));
    original.name.assign(33, 'X');
    assert(!remote::encodeProfile(original, encoded));
    original.name = "Bad\nName";
    assert(!remote::encodeProfile(original, encoded));
    puts("Profile round-trip, corruption, truncation and capacity tests passed.");
}
