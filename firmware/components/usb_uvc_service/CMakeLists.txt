idf_component_register(SRCS "src/UsbUvcService.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES camera_stream usb_device_uvc
                       PRIV_REQUIRES heap esp_timer espressif__tinyusb)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
