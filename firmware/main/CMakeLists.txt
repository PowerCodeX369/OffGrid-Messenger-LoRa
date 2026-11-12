# Deliberately no esp_wifi, esp_now, esp_netif, or esp_event dependencies.
if(EXISTS "${IDF_PATH}/components/esp_driver_usb_serial_jtag")
  set(og_usb_dep esp_driver_usb_serial_jtag)
else()
  set(og_usb_dep driver)
endif()
idf_component_register(
  SRCS "main.c" "ble_bridge.c"
  INCLUDE_DIRS "."
  REQUIRES bt mbedtls nvs_flash esp_timer esp_hw_support
  PRIV_REQUIRES offgrid_sx1278 ${og_usb_dep}
)
