# On ESP-IDF 5.2 some driver APIs belong to the combined `driver` component.
# Newer ESP-IDF versions split SPI and GPIO into their own components.
if(EXISTS "${IDF_PATH}/components/esp_driver_spi")
    set(og_spi_dep esp_driver_spi)
else()
    set(og_spi_dep driver)
endif()
if(EXISTS "${IDF_PATH}/components/esp_driver_gpio")
    set(og_gpio_dep esp_driver_gpio)
else()
    set(og_gpio_dep driver)
endif()
idf_component_register(
    SRCS "offgrid_sx1278.c" "offgrid_sx1278_profile.c"
    INCLUDE_DIRS "include"
    REQUIRES ${og_spi_dep} ${og_gpio_dep} esp_timer
)
