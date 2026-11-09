idf_component_register(SRCS "app_main.cpp" REQUIRES esp_hosted esp_wifi esp_event esp_wifi_remote c6_update
					   EMBED_FILES "c6_image.bin")