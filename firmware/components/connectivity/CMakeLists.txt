idf_component_register(SRCS "src/WifiService.cpp" "src/HostedProbe.cpp" INCLUDE_DIRS "include"
                       REQUIRES camera_domain esp_event esp_netif freertos
                       PRIV_REQUIRES esp_wifi esp_wifi_remote esp_hosted mbedtls)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)