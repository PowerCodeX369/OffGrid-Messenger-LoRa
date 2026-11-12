#pragma once
#include <stdint.h>
typedef void * QueueHandle_t;
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) ((uint32_t)((x)/10))
