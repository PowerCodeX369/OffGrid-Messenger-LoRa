/* Emulate SX1278's SPI FIFO addressing to verify real driver packet I/O.
 * This does not simulate RF reception, propagation, or ESP-IDF SPI DMA. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "offgrid_sx1278.h"
#include "driver/spi_master.h"

static uint8_t regs[128], fifo[256];
static unsigned fifo_ptr, tx_writes, rx_reads, bulk_writes, bulk_reads;
static int dma_selected;
static int64_t time_us;
static int rx_armed;

esp_err_t gpio_config(const gpio_config_t *cfg){(void)cfg;return ESP_OK;}
esp_err_t gpio_set_level(gpio_num_t pin,int val){(void)pin;(void)val;return ESP_OK;}
int gpio_get_level(gpio_num_t pin){(void)pin;return 1;}
int64_t esp_timer_get_time(void){return time_us;}
void vTaskDelay(uint32_t ticks){time_us+=(ticks?ticks:1)*10000;}
const char *esp_err_to_name(esp_err_t e){(void)e;return "mock";}
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *bus,int dma){
    assert(host==SPI2_HOST && (unsigned)bus->max_transfer_sz>=OFFGRID_SX1278_MAX_PAYLOAD+1);
    dma_selected=dma;return ESP_OK;
}
esp_err_t spi_bus_add_device(int host,const spi_device_interface_config_t *cfg,spi_device_handle_t *device){
    assert(host==SPI2_HOST && cfg->mode==0);
    *device=(void*)0x1;regs[0x42]=0x12;return ESP_OK;
}
esp_err_t spi_bus_remove_device(spi_device_handle_t handle){(void)handle;return ESP_OK;}
esp_err_t spi_bus_free(int host){(void)host;return ESP_OK;}

esp_err_t spi_device_transmit(spi_device_handle_t device,spi_transaction_t *t){
    assert(device && t && t->length>0 && t->length%8==0);
    size_t count=t->length/8;
    const uint8_t *out=(t->flags&SPI_TRANS_USE_TXDATA)?t->tx_data:t->tx_buffer;
    uint8_t *in=(t->flags&SPI_TRANS_USE_RXDATA)?t->rx_data:t->rx_buffer;
    assert(out && in && count<=OFFGRID_SX1278_MAX_PAYLOAD+1);
    uint8_t addr=out[0]&0x7f;int write=!!(out[0]&0x80);
    memset(in,0,count);
    if(addr==0){
        if(write)bulk_writes++;else bulk_reads++;
        for(size_t i=1;i<count;i++){
            if(write){fifo[fifo_ptr++&255]=out[i];tx_writes++;}
            else{in[i]=fifo[fifo_ptr++&255];rx_reads++;}
        }
        return ESP_OK;
    }
    assert(count==2);
    if(write){
        if(addr==0x12)regs[0x12]&=(uint8_t)~out[1];
        else{
            regs[addr]=out[1];
            if(addr==0x0d)fifo_ptr=out[1];
            if(addr==0x01 && out[1]==0x8b)regs[0x12]|=0x08;
            if(addr==0x01 && out[1]==0x8d && rx_armed){regs[0x12]|=0x40;}
        }
    }else in[1]=regs[addr];
    return ESP_OK;
}
int main(void){
    const offgrid_sx1278_profile_t p={.frequency_hz=433000000,.bandwidth=OFFGRID_SX1278_BW_125_KHZ,.spreading_factor=7,.coding_denominator=5,.tx_power_dbm=10,.sync_word=0x2d,.preamble_symbols=8};
    offgrid_sx1278_t *radio=NULL;
    offgrid_sx1278_pins_t pins=offgrid_sx1278_default_pins();
    assert(offgrid_sx1278_open(&pins,&p,&radio)==ESP_OK);
    assert(dma_selected==SPI_DMA_CH_AUTO);
    uint8_t written[218];
    for(unsigned i=0;i<sizeof(written);i++)written[i]=(uint8_t)(i^0x5a);
    assert(offgrid_sx1278_send(radio,written,sizeof(written),1600)==ESP_OK);
    assert(bulk_writes==1 && tx_writes==sizeof(written));
    assert(!memcmp(fifo,written,sizeof(written)));
    bool busy=false;
    regs[0x18]=0x04; /* Valid LoRa RX in progress. Query must not switch modes. */
    assert(offgrid_sx1278_rx_busy(radio,&busy)==ESP_OK && busy);
    regs[0x18]=0;assert(offgrid_sx1278_rx_busy(radio,&busy)==ESP_OK && !busy);
    uint8_t readback[218]={0};size_t n=0;
    offgrid_sx1278_packet_info_t info={0};
    regs[0x13]=sizeof(written);regs[0x10]=0;regs[0x19]=0x10;regs[0x1a]=80;
    rx_armed=1;
    assert(offgrid_sx1278_receive(radio,readback,sizeof(readback),&n,&info,50)==ESP_OK);
    assert(n==sizeof(written) && !memcmp(readback,written,sizeof(written)));
    assert(bulk_reads==1 && rx_reads==sizeof(written));
    offgrid_sx1278_close(radio);
    puts("PASS: simulated SX1278 full-packet 218-byte TX/RX, one FIFO burst each, DMA selection");
    return 0;
}
