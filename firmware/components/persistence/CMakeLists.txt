idf_component_register(SRCS "src/NvsCredentialStore.cpp" INCLUDE_DIRS "include"
                       REQUIRES nvs_flash camera_domain PRIV_REQUIRES mbedtls esp_partition)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)