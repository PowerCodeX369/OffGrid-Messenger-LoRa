#pragma once
#include "freertos/FreeRTOS.h"
int xTaskCreate(void (*fn)(void*),const char*,uint32_t,void*,unsigned,void*);
void vTaskDelay(uint32_t);
void vTaskDelete(void*);
