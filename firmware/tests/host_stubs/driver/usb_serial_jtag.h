#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct {int rx_buffer_size,tx_buffer_size;} usb_serial_jtag_driver_config_t;
esp_err_t usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t*);
int usb_serial_jtag_read_bytes(uint8_t*,uint32_t,uint32_t);
