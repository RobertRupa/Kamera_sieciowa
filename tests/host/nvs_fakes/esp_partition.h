#pragma once
#include <cstddef>
struct esp_partition_t { std::size_t size; };
constexpr int ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_SUBTYPE_DATA_NVS = 2;
const esp_partition_t* esp_partition_find_first(int, int, const char*);