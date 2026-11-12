#include "profile_store.hpp"
#include "nvs_flash.h"
#include <cstdio>

namespace {
void keyFor(size_t slot, char (&key)[12]) { snprintf(key, sizeof(key), "remote%u", unsigned(slot)); }
constexpr size_t kMaxBlob = 37 + remote::kButtons.size() * (2 + remote::kMaxDurations * 2);
}
esp_err_t ProfileStore::begin() {
    // Do not auto-erase on a storage error: learned codes must be preserved.
    auto err = nvs_flash_init();
    return err == ESP_OK ? nvs_open("ir_remotes", NVS_READWRITE, &handle_) : err;
}
esp_err_t ProfileStore::load(size_t slot, remote::Profile& profile) {
    if (slot >= remote::kMaxProfiles) return ESP_ERR_INVALID_ARG;
    char key[12]; keyFor(slot, key);
    size_t size = 0;
    auto err = nvs_get_blob(handle_, key, nullptr, &size);
    if (err != ESP_OK) return err;
    if (size > kMaxBlob) return ESP_ERR_INVALID_SIZE;
    std::vector<uint8_t> bytes(size);
    err = nvs_get_blob(handle_, key, bytes.data(), &size);
    if (err != ESP_OK) return err;
    return remote::decodeProfile(bytes, profile) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
esp_err_t ProfileStore::save(size_t slot, const remote::Profile& profile) {
    if (slot >= remote::kMaxProfiles) return ESP_ERR_INVALID_ARG;
    std::vector<uint8_t> bytes;
    if (!remote::encodeProfile(profile, bytes)) return ESP_ERR_INVALID_ARG;
    char key[12]; keyFor(slot, key);
    auto err = nvs_set_blob(handle_, key, bytes.data(), bytes.size());
    return err == ESP_OK ? nvs_commit(handle_) : err;
}
esp_err_t ProfileStore::remove(size_t slot) {
    if (slot >= remote::kMaxProfiles) return ESP_ERR_INVALID_ARG;
    char key[12]; keyFor(slot, key);
    auto err = nvs_erase_key(handle_, key);
    return err == ESP_OK ? nvs_commit(handle_) : err;
}
