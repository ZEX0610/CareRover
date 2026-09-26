idf_component_register(SRCS "main.cpp" "video_server.cpp"
  INCLUDE_DIRS "." "../../main_wireless"
  PRIV_REQUIRES esp_timer esp_http_server esp_wifi esp_event nvs_flash esp_netif driver)
