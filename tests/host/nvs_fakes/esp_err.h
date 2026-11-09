#pragma once
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
constexpr esp_err_t ESP_ERR_NOT_FOUND = 0x105;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_NOT_INITIALIZED = 0x1101;
constexpr esp_err_t ESP_ERR_NVS_PART_NOT_FOUND = 0x110f;
constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 0x110d;