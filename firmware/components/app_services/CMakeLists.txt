idf_component_register(SRCS "src/NetworkProvisioner.cpp" INCLUDE_DIRS "include"
                       REQUIRES camera_domain)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)