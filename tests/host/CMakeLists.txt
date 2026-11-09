cmake_minimum_required(VERSION 3.16)
project(camera_hub_host_tests LANGUAGES CXX)
enable_testing()
find_package(Threads REQUIRED)
set(STREAM_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/camera_stream")
add_executable(frame_broker_tests frame_broker_tests.cpp
    "${STREAM_COMPONENT}/src/FrameBroker.cpp"
    "${STREAM_COMPONENT}/src/FrameLease.cpp")
target_include_directories(frame_broker_tests PRIVATE "${STREAM_COMPONENT}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/camera_domain/include")
target_compile_features(frame_broker_tests PRIVATE cxx_std_17)
target_link_libraries(frame_broker_tests PRIVATE Threads::Threads)
add_test(NAME frame_broker COMMAND frame_broker_tests)

set(DOMAIN_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/camera_domain")
add_executable(profile_policy_tests profile_policy_tests.cpp
    "${DOMAIN_COMPONENT}/src/ProfilePolicy.cpp")
target_include_directories(profile_policy_tests PRIVATE "${DOMAIN_COMPONENT}/include")
target_compile_features(profile_policy_tests PRIVATE cxx_std_17)
add_test(NAME profile_policy COMMAND profile_policy_tests)

add_executable(camera_probe_result_tests camera_probe_result_tests.cpp
    "${DOMAIN_COMPONENT}/src/CameraProbeResult.cpp")
target_include_directories(camera_probe_result_tests PRIVATE "${DOMAIN_COMPONENT}/include")
target_compile_features(camera_probe_result_tests PRIVATE cxx_std_17)
add_test(NAME camera_probe_result COMMAND camera_probe_result_tests)

add_executable(provisioning_policy_tests provisioning_policy_tests.cpp
    "${DOMAIN_COMPONENT}/src/ProvisioningPolicy.cpp")
target_include_directories(provisioning_policy_tests PRIVATE "${DOMAIN_COMPONENT}/include")
target_compile_features(provisioning_policy_tests PRIVATE cxx_std_17)
add_test(NAME provisioning_policy COMMAND provisioning_policy_tests)

add_executable(wifi_credential_tests wifi_credential_tests.cpp
    "${DOMAIN_COMPONENT}/src/WifiCredentialCodec.cpp"
    "${DOMAIN_COMPONENT}/src/ProvisioningPolicy.cpp")
target_include_directories(wifi_credential_tests PRIVATE "${DOMAIN_COMPONENT}/include")
target_compile_features(wifi_credential_tests PRIVATE cxx_std_17)
add_test(NAME wifi_credentials COMMAND wifi_credential_tests)

set(PERSISTENCE_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/persistence")
add_executable(nvs_credential_store_tests nvs_credential_store_tests.cpp
    "${PERSISTENCE_COMPONENT}/src/NvsCredentialStore.cpp"
    "${DOMAIN_COMPONENT}/src/WifiCredentialCodec.cpp"
    "${DOMAIN_COMPONENT}/src/ProvisioningPolicy.cpp")
target_include_directories(nvs_credential_store_tests PRIVATE nvs_fakes
    "${PERSISTENCE_COMPONENT}/include" "${DOMAIN_COMPONENT}/include")
target_compile_features(nvs_credential_store_tests PRIVATE cxx_std_17)
target_link_libraries(nvs_credential_store_tests PRIVATE Threads::Threads)
add_test(NAME plain_nvs_store COMMAND nvs_credential_store_tests)

set(SERVICES_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../firmware/components/app_services")
add_executable(network_provisioner_tests network_provisioner_tests.cpp
    "${SERVICES_COMPONENT}/src/NetworkProvisioner.cpp"
    "${DOMAIN_COMPONENT}/src/WifiCredentialCodec.cpp"
    "${DOMAIN_COMPONENT}/src/ProvisioningPolicy.cpp")
target_include_directories(network_provisioner_tests PRIVATE
    "${SERVICES_COMPONENT}/include" "${DOMAIN_COMPONENT}/include")
target_compile_features(network_provisioner_tests PRIVATE cxx_std_17)
target_link_libraries(network_provisioner_tests PRIVATE Threads::Threads)
add_test(NAME network_provisioner COMMAND network_provisioner_tests)

add_executable(network_scan_service_tests network_scan_service_tests.cpp
    "${SERVICES_COMPONENT}/src/NetworkScanService.cpp")
target_include_directories(network_scan_service_tests PRIVATE
    "${SERVICES_COMPONENT}/include" "${DOMAIN_COMPONENT}/include")
target_compile_features(network_scan_service_tests PRIVATE cxx_std_17)
target_link_libraries(network_scan_service_tests PRIVATE Threads::Threads)
add_test(NAME network_scan_service COMMAND network_scan_service_tests)

set(CAPABILITY_COMPONENT "${CMAKE_CURRENT_LIST_DIR}/../../templates/esp-idf/components/board_capabilities")
add_executable(capability_registry_tests
    "${CAPABILITY_COMPONENT}/tests/capability_registry_host_test.cpp"
    "${CAPABILITY_COMPONENT}/src/BoardProfile.cpp"
    "${CAPABILITY_COMPONENT}/src/CapabilityRegistry.cpp")
target_include_directories(capability_registry_tests PRIVATE "${CAPABILITY_COMPONENT}/include")
target_compile_features(capability_registry_tests PRIVATE cxx_std_17)
add_test(NAME capability_registry COMMAND capability_registry_tests)

foreach(test_target frame_broker_tests profile_policy_tests camera_probe_result_tests provisioning_policy_tests wifi_credential_tests nvs_credential_store_tests network_provisioner_tests network_scan_service_tests capability_registry_tests)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()