#include "remote_model.hpp"
#include <utility>

namespace remote {
bool validMessage(const Message& message) {
    if (message.pulses.size() < 6 || message.pulses.size() > kMaxDurations) return false;
    for (auto pulse : message.pulses) if (!pulse || pulse > 32767) return false;
    return true;
}
bool encodeProfile(const Profile& profile, std::vector<uint8_t>& bytes) {
    if (profile.name.empty() || profile.name.size() > 32) return false;
    for (unsigned char c : profile.name) if (c < 32 || c > 126) return false;
    // Versioned little-endian format, independent of compiler padding and pointers.
    bytes = {'I', 'R', 'P', 1, static_cast<uint8_t>(profile.name.size())};
    bytes.insert(bytes.end(), profile.name.begin(), profile.name.end());
    auto put16 = [&](uint16_t n) { bytes.push_back(n & 255); bytes.push_back(n >> 8); };
    for (const auto& message : profile.messages) {
        if (!message.empty() && !validMessage(message)) return false;
        put16(message.pulses.size());
        for (auto pulse : message.pulses) put16(pulse);
    }
    return true;
}
bool decodeProfile(const std::vector<uint8_t>& bytes, Profile& profile) {
    if (bytes.size() < 5 || bytes[0] != 'I' || bytes[1] != 'R' || bytes[2] != 'P' || bytes[3] != 1)
        return false;
    size_t pos = 5, length = bytes[4];
    if (!length || length > 32 || pos + length > bytes.size()) return false;
    Profile decoded;
    decoded.name.assign(reinterpret_cast<const char*>(bytes.data() + pos), length);
    for (unsigned char c : decoded.name) if (c < 32 || c > 126) return false;
    pos += length;
    auto get16 = [&]() { auto n = bytes[pos] | (bytes[pos + 1] << 8); pos += 2; return n; };
    for (auto& message : decoded.messages) {
        if (pos + 2 > bytes.size()) return false;
        size_t count = get16();
        if (count > kMaxDurations || pos + count * 2 > bytes.size()) return false;
        for (size_t i = 0; i < count; ++i) message.pulses.push_back(get16());
        if (count && !validMessage(message)) return false;
    }
    if (pos != bytes.size()) return false;
    profile = std::move(decoded);  // Failed loads leave the caller unchanged.
    return true;
}
}
