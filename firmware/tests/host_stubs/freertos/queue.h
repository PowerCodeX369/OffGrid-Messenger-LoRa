#pragma once
#include <stddef.h>
#include "freertos/FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned,size_t);
int xQueueSend(QueueHandle_t,const void*,uint32_t);
int xQueueReceive(QueueHandle_t,void*,uint32_t);
