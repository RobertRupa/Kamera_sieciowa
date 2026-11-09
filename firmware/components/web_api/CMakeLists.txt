set(WEB_DIST_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../web/dist")
if(NOT EXISTS "${WEB_DIST_DIR}/index.html")
    message(FATAL_ERROR "Wi-Fi WebGUI assets are missing; run npm run build in web/ before idf.py build")
endif()

find_program(GZIP_EXECUTABLE gzip REQUIRED)
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE WEB_DIST_FILES LIST_DIRECTORIES false "${WEB_DIST_DIR}/*")
else()
    file(GLOB_RECURSE WEB_DIST_FILES LIST_DIRECTORIES false CONFIGURE_DEPENDS "${WEB_DIST_DIR}/*")
endif()
set(WEB_EMBEDDED_FILES "")
set(WEB_ASSET_HEADER "#pragma once\n#include <cstdint>\n\nnamespace web_api {\nstruct EmbeddedWebAsset { const char* path; const std::uint8_t* start; const std::uint8_t* end; const char* contentType; };\n")

foreach(WEB_FILE IN LISTS WEB_DIST_FILES)
    if(WEB_FILE MATCHES "\\.gz$")
        continue()
    endif()

    get_filename_component(WEB_EXTENSION "${WEB_FILE}" LAST_EXT)
    string(TOLOWER "${WEB_EXTENSION}" WEB_EXTENSION)
    if(WEB_EXTENSION STREQUAL ".html")
        set(WEB_CONTENT_TYPE "text/html; charset=utf-8")
    elseif(WEB_EXTENSION STREQUAL ".js")
        set(WEB_CONTENT_TYPE "text/javascript; charset=utf-8")
    elseif(WEB_EXTENSION STREQUAL ".css")
        set(WEB_CONTENT_TYPE "text/css; charset=utf-8")
    elseif(WEB_EXTENSION STREQUAL ".svg")
        set(WEB_CONTENT_TYPE "image/svg+xml")
    elseif(WEB_EXTENSION STREQUAL ".woff2")
        set(WEB_CONTENT_TYPE "font/woff2")
    elseif(WEB_EXTENSION STREQUAL ".woff")
        set(WEB_CONTENT_TYPE "font/woff")
    elseif(WEB_EXTENSION STREQUAL ".ttf")
        set(WEB_CONTENT_TYPE "font/ttf")
    elseif(WEB_EXTENSION STREQUAL ".eot")
        set(WEB_CONTENT_TYPE "application/vnd.ms-fontobject")
    else()
        message(FATAL_ERROR "Unsupported embedded WebGUI asset type: ${WEB_FILE}")
    endif()

    set(WEB_GZIP_FILE "${WEB_FILE}.gz")
    execute_process(COMMAND "${GZIP_EXECUTABLE}" -n -f -k "${WEB_FILE}"
                    RESULT_VARIABLE GZIP_RESULT)
    if(NOT GZIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to gzip WebGUI asset: ${WEB_FILE}")
    endif()
    list(APPEND WEB_EMBEDDED_FILES "${WEB_GZIP_FILE}")

    get_filename_component(WEB_GZIP_NAME "${WEB_GZIP_FILE}" NAME)
    string(MAKE_C_IDENTIFIER "${WEB_GZIP_NAME}" WEB_SYMBOL)
    file(RELATIVE_PATH WEB_RELATIVE_PATH "${WEB_DIST_DIR}" "${WEB_FILE}")
    string(APPEND WEB_ASSET_HEADER
        "extern \"C\" { extern const std::uint8_t _binary_${WEB_SYMBOL}_start[]; extern const std::uint8_t _binary_${WEB_SYMBOL}_end[]; }\n")
    string(APPEND WEB_ASSET_HEADER
        "inline constexpr EmbeddedWebAsset kWebAsset_${WEB_SYMBOL}{\"/${WEB_RELATIVE_PATH}\", _binary_${WEB_SYMBOL}_start, _binary_${WEB_SYMBOL}_end, \"${WEB_CONTENT_TYPE}\"};\n")
    list(APPEND WEB_ASSET_SYMBOLS "kWebAsset_${WEB_SYMBOL}")
endforeach()

string(JOIN ",\n    " WEB_ASSET_TABLE ${WEB_ASSET_SYMBOLS})
string(APPEND WEB_ASSET_HEADER
    "inline constexpr EmbeddedWebAsset kWebAssets[]{\n    ${WEB_ASSET_TABLE}\n};\n}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wifi_web_assets.hpp" "${WEB_ASSET_HEADER}")

idf_component_register(SRCS "src/ProvisioningApi.cpp" "src/CameraApi.cpp" INCLUDE_DIRS "include"
                       EMBED_FILES ${WEB_EMBEDDED_FILES}
                       REQUIRES app_services board_capabilities camera_domain camera_stream esp_http_server
                       PRIV_REQUIRES cjson esp_app_format esp_timer esp_hw_support heap lwip mbedtls)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)