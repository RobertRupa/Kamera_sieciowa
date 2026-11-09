idf_component_register(SRCS "src/MdnsService.cpp" INCLUDE_DIRS "include"
                       REQUIRES camera_domain PRIV_REQUIRES mdns)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)