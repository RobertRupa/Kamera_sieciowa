idf_component_register(SRCS "src/ProvisioningApi.cpp" INCLUDE_DIRS "include"
                       REQUIRES app_services camera_domain esp_http_server
                       PRIV_REQUIRES cjson esp_timer esp_hw_support lwip mbedtls)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)