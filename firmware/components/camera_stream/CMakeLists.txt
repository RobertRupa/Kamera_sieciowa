idf_component_register(SRCS "src/FrameBroker.cpp" "src/FrameLease.cpp"
                       "src/CameraHardwareProbe.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES camera_domain
                       PRIV_REQUIRES esp_driver_i2c esp_video)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)