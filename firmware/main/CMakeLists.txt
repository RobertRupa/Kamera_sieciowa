idf_component_register(SRCS "app_main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES board_support board_capabilities camera_domain camera_stream persistence mbedtls connectivity discovery app_services web_api)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)