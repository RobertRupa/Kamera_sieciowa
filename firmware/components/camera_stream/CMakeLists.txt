idf_component_register(SRCS "src/FrameBroker.cpp" "src/FrameLease.cpp"
                       "src/CameraHardwareProbe.cpp"
                       "src/CameraNetworkStream.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES camera_domain
                       PRIV_REQUIRES esp_driver_i2c esp_driver_jpeg esp_video esp_timer esp_rom heap rtsp lwip)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)