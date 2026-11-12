#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_INVALID_RESPONSE 4
#define ESP_ERR_INVALID_SIZE 5
#define ESP_ERR_TIMEOUT 6
#define ESP_ERR_NVS_NO_FREE_PAGES 7
#define ESP_ERR_NVS_NEW_VERSION_FOUND 8
#define ESP_ERROR_CHECK(x) ((void)(x))
const char *esp_err_to_name(esp_err_t e);
