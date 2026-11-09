idf_component_register(SRCS "src/BoardConfig.cpp" INCLUDE_DIRS "include"
                       REQUIRES board_capabilities)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)