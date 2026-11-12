#pragma once
#include "remote_model.hpp"
#include "nvs.h"

// Each profile occupies an independent, atomically replaced NVS blob.
class ProfileStore {
public:
    esp_err_t begin();
    esp_err_t load(size_t slot, remote::Profile& profile);
    esp_err_t save(size_t slot, const remote::Profile& profile);
    esp_err_t remove(size_t slot);
private:
    nvs_handle_t handle_ = 0;
};
