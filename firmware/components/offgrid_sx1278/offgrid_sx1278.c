/*
 * OffGrid Messenger LoRa: SX1278 / Ra-02 ESP-IDF SPI radio driver.
 *
 * Small, deterministic blocking API intended for initial hardware integration.
 * No encryption or app protocol is implemented here; it is NOT a chat driver.
 * A single dedicated radio-worker task must own the handle in future phases.
 *
 * SX1278 LoRa register addresses follow Semtech SX1276/77/78/79 Rev 7.
 */
#include "offgrid_sx1278.h"

#include <stdlib.h>
#include <string.h>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RETURN_ON_ERROR(expression) do { \
    const esp_err_t error_ = (expression); \
    if (error_ != ESP_OK) return error_; \
} while (0)

/* LoRa mode register addresses. */
enum {
    REG_FIFO = 0x00, REG_OP_MODE = 0x01,
    REG_FRF_MSB = 0x06, REG_FRF_MID = 0x07, REG_FRF_LSB = 0x08,
    REG_PA_CONFIG = 0x09, REG_OCP = 0x0B,
    REG_FIFO_ADDR_PTR = 0x0D, REG_FIFO_TX_BASE = 0x0E,
    REG_FIFO_RX_BASE = 0x0F, REG_FIFO_RX_CURRENT = 0x10,
    REG_IRQ_FLAGS = 0x12, REG_RX_BYTES = 0x13,
    REG_PKT_SNR = 0x19, REG_PKT_RSSI = 0x1A,
    REG_MODEM_CONFIG1 = 0x1D, REG_MODEM_CONFIG2 = 0x1E,
    REG_PREAMBLE_MSB = 0x20, REG_PREAMBLE_LSB = 0x21,
    REG_PAYLOAD_LEN = 0x22, REG_MODEM_CONFIG3 = 0x26,
    REG_DETECT_OPTIMIZE = 0x31, REG_DETECTION_THRESHOLD = 0x37,
    REG_SYNC_WORD = 0x39, REG_DIO_MAPPING1 = 0x40,
    REG_VERSION = 0x42, REG_PA_DAC = 0x4D,
};

enum {
    MODE_SLEEP = 0x88, MODE_STANDBY = 0x89,
    MODE_TX = 0x8B, MODE_RX_CONTINUOUS = 0x8D, MODE_CAD = 0x8F,
    IRQ_TX_DONE = 0x08, IRQ_RX_DONE = 0x40, IRQ_CRC_ERROR = 0x20,
    IRQ_CAD_DONE = 0x04, IRQ_CAD_DETECTED = 0x01,
};

struct offgrid_sx1278 {
    spi_device_handle_t device;
    offgrid_sx1278_pins_t pins;
    offgrid_sx1278_profile_t profile;
    offgrid_sx1278_counters_t stats;
    bool owns_bus;
    /* Continuous RX survives short worker polling timeouts. Restarting RX
     * every 120 ms drops frames whose airtime exceeds that interval. */
    bool rx_listening;
};

static const char *TAG = "offgrid_sx1278";

/* Each register accesses two SPI bytes; no DMA or multi-byte buffer concerns. */
static esp_err_t radio_transfer(offgrid_sx1278_t *r, uint8_t addr,
                                 uint8_t value, uint8_t *received)
{
    spi_transaction_t t = {
        .length = 16,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    t.tx_data[0] = addr;
    t.tx_data[1] = value;
    const esp_err_t err = spi_device_transmit(r->device, &t);
    if (err == ESP_OK && received != NULL) *received = t.rx_data[1];
    return err;
}

static esp_err_t read_reg(offgrid_sx1278_t *r, uint8_t reg, uint8_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    return radio_transfer(r, reg & 0x7FU, 0, out);
}

static esp_err_t write_reg(offgrid_sx1278_t *r, uint8_t reg, uint8_t value)
{
    return radio_transfer(r, reg | 0x80U, value, NULL);
}

static esp_err_t radio_reset(offgrid_sx1278_t *r)
{
    RETURN_ON_ERROR(gpio_set_level(r->pins.reset, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    RETURN_ON_ERROR(gpio_set_level(r->pins.reset, 1));
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static esp_err_t enter_lora_sleep(offgrid_sx1278_t *r)
{
    /* LongRangeMode bit may be changed only while the chip is asleep. */
    uint8_t previous = 0;
    RETURN_ON_ERROR(read_reg(r, REG_OP_MODE, &previous));
    RETURN_ON_ERROR(write_reg(r, REG_OP_MODE, previous & 0x88U));
    vTaskDelay(pdMS_TO_TICKS(10));
    RETURN_ON_ERROR(write_reg(r, REG_OP_MODE, MODE_SLEEP));
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

static esp_err_t set_mode(offgrid_sx1278_t *r, uint8_t mode)
{
    const esp_err_t err = write_reg(r, REG_OP_MODE, mode);
    if (err == ESP_OK) r->rx_listening = (mode == MODE_RX_CONTINUOUS);
    return err;
}

static esp_err_t clear_irqs(offgrid_sx1278_t *r)
{
    /* SX1278 IRQ flags are write-one-to-clear. */
    return write_reg(r, REG_IRQ_FLAGS, 0xFFU);
}

offgrid_sx1278_pins_t offgrid_sx1278_default_pins(void)
{
    return (offgrid_sx1278_pins_t) {
        .nss = GPIO_NUM_10, .sck = GPIO_NUM_6,
        .mosi = GPIO_NUM_7, .miso = GPIO_NUM_5,
        .reset = GPIO_NUM_3, .dio0 = GPIO_NUM_4,
        .dio1 = GPIO_NUM_0, .spi_clock_hz = 4000000U,
    };
}

esp_err_t offgrid_sx1278_sleep(offgrid_sx1278_t *r)
{
    if (r == NULL) return ESP_ERR_INVALID_ARG;
    return set_mode(r, MODE_SLEEP);
}

esp_err_t offgrid_sx1278_standby(offgrid_sx1278_t *r)
{
    if (r == NULL) return ESP_ERR_INVALID_ARG;
    return set_mode(r, MODE_STANDBY);
}

esp_err_t offgrid_sx1278_set_profile(offgrid_sx1278_t *r,
                                     const offgrid_sx1278_profile_t *p)
{
    if (r == NULL) return ESP_ERR_INVALID_ARG;
    offgrid_sx1278_register_profile_t reg;
    if (!offgrid_sx1278_make_registers(p, &reg)) return ESP_ERR_INVALID_ARG;

    RETURN_ON_ERROR(enter_lora_sleep(r));
    RETURN_ON_ERROR(set_mode(r, MODE_STANDBY));
    vTaskDelay(pdMS_TO_TICKS(10));

    RETURN_ON_ERROR(write_reg(r, REG_FRF_MSB, (reg.frf >> 16) & 0xFFU));
    RETURN_ON_ERROR(write_reg(r, REG_FRF_MID, (reg.frf >> 8) & 0xFFU));
    RETURN_ON_ERROR(write_reg(r, REG_FRF_LSB, reg.frf & 0xFFU));

    /* Conservative normal PA_BOOST path. High-power PA_DAC mode NOT enabled. */
    RETURN_ON_ERROR(write_reg(r, REG_PA_DAC, 0x84U));
    RETURN_ON_ERROR(write_reg(r, REG_PA_CONFIG, reg.pa_config));
    /* Preserve ordinary over-current protection settings for <=17 dBm. */
    RETURN_ON_ERROR(write_reg(r, REG_OCP, 0x2BU));

    RETURN_ON_ERROR(write_reg(r, REG_MODEM_CONFIG1, reg.modem_config1));
    RETURN_ON_ERROR(write_reg(r, REG_MODEM_CONFIG2, reg.modem_config2));
    RETURN_ON_ERROR(write_reg(r, REG_MODEM_CONFIG3, reg.modem_config3));
    RETURN_ON_ERROR(write_reg(r, REG_PREAMBLE_MSB, p->preamble_symbols >> 8));
    RETURN_ON_ERROR(write_reg(r, REG_PREAMBLE_LSB, p->preamble_symbols & 0xFFU));
    RETURN_ON_ERROR(write_reg(r, REG_SYNC_WORD, p->sync_word));
    /* LoRa v1.0 carries at most 218 bytes (202-byte OGM2 frame + HMAC).
     * Start each half-duplex FIFO direction at zero so even maximum packets
     * fit without depending on address-pointer wrapping at 0xFF. */
    RETURN_ON_ERROR(write_reg(r, REG_FIFO_TX_BASE, 0x00U));
    RETURN_ON_ERROR(write_reg(r, REG_FIFO_RX_BASE, 0x00U));
    RETURN_ON_ERROR(write_reg(r, REG_DETECT_OPTIMIZE, 0x03U));
    RETURN_ON_ERROR(write_reg(r, REG_DETECTION_THRESHOLD, 0x0AU));
    RETURN_ON_ERROR(clear_irqs(r));
    r->profile = *p;
    return ESP_OK;
}

esp_err_t offgrid_sx1278_open(const offgrid_sx1278_pins_t *pins,
                              const offgrid_sx1278_profile_t *profile,
                              offgrid_sx1278_t **out)
{
    if (pins == NULL || out == NULL || !offgrid_sx1278_profile_valid(profile) ||
        pins->spi_clock_hz == 0 || pins->spi_clock_hz > 10000000U) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    offgrid_sx1278_t *r = calloc(1, sizeof(*r));
    if (r == NULL) return ESP_ERR_NO_MEM;
    r->pins = *pins;

    gpio_config_t reset = {
        .pin_bit_mask = 1ULL << pins->reset,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    esp_err_t err = gpio_config(&reset);
    if (err != ESP_OK) goto fail;
    err = gpio_set_level(pins->reset, 1);
    if (err != ESP_OK) goto fail;
    gpio_config_t interrupt_inputs = {
        .pin_bit_mask = (1ULL << pins->dio0) | (1ULL << pins->dio1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    err = gpio_config(&interrupt_inputs);
    if (err != ESP_OK) goto fail;

    spi_bus_config_t bus = {
        .mosi_io_num = pins->mosi, .miso_io_num = pins->miso,
        .sclk_io_num = pins->sck,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 4,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) goto fail;
    r->owns_bus = true;
    spi_device_interface_config_t dev = {
        .clock_speed_hz = (int)pins->spi_clock_hz,
        .mode = 0, .spics_io_num = pins->nss,
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &dev, &r->device);
    if (err != ESP_OK) goto fail;
    err = radio_reset(r);
    if (err != ESP_OK) goto fail;

    uint8_t version = 0;
    err = read_reg(r, REG_VERSION, &version);
    if (err != ESP_OK) goto fail;
    if (version != 0x12U) {
        ESP_LOGE(TAG, "SX1278 RegVersion=0x%02X (expected 0x12)", version);
        err = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }
    err = offgrid_sx1278_set_profile(r, profile);
    if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "SX1278 ready: %lu Hz, SF%u, BW %lu Hz, %d dBm, SPI %lu Hz",
             (unsigned long)profile->frequency_hz,
             (unsigned)profile->spreading_factor,
             (unsigned long)profile->bandwidth,
             (int)profile->tx_power_dbm,
             (unsigned long)pins->spi_clock_hz);
    *out = r;
    return ESP_OK;
fail:
    offgrid_sx1278_close(r);
    return err;
}

/* Wait for the hardware IRQ flag with a bounded, cooperative polling loop. */
static esp_err_t wait_flag(offgrid_sx1278_t *r, uint8_t expected,
                           uint32_t timeout_ms, uint8_t *flags,
                           bool *dio0_observed)
{
    const int64_t until = esp_timer_get_time() + (int64_t)timeout_ms * 1000LL;
    for (;;) {
        uint8_t current = 0;
        RETURN_ON_ERROR(read_reg(r, REG_IRQ_FLAGS, &current));
        if (current & expected) {
            if (flags != NULL) *flags = current;
            if (dio0_observed != NULL) {
                *dio0_observed = gpio_get_level(r->pins.dio0) != 0;
            }
            return ESP_OK;
        }
        if (esp_timer_get_time() >= until) return ESP_ERR_TIMEOUT;
        vTaskDelay(1); /* Always yield at least one FreeRTOS tick. */
    }
}

esp_err_t offgrid_sx1278_cad(offgrid_sx1278_t *r,
                              bool *activity_detected, uint32_t timeout_ms)
{
    if (r == NULL || activity_detected == NULL || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *activity_detected = false;
    RETURN_ON_ERROR(offgrid_sx1278_standby(r));
    RETURN_ON_ERROR(clear_irqs(r));
    /* DIO0=10 means CadDone; IRQ flags are still authoritative. */
    RETURN_ON_ERROR(write_reg(r, REG_DIO_MAPPING1, 0x80U));
    RETURN_ON_ERROR(set_mode(r, MODE_CAD));
    uint8_t flags = 0;
    const esp_err_t err = wait_flag(r, IRQ_CAD_DONE, timeout_ms, &flags, NULL);
    const esp_err_t stop = offgrid_sx1278_standby(r);
    const esp_err_t clear = clear_irqs(r);
    if (err != ESP_OK) return err;
    if (stop != ESP_OK) return stop;
    if (clear != ESP_OK) return clear;
    *activity_detected = (flags & IRQ_CAD_DETECTED) != 0;
    return ESP_OK;
}

esp_err_t offgrid_sx1278_send(offgrid_sx1278_t *r, const uint8_t *data,
                               size_t size, uint32_t timeout_ms)
{
    if (r == NULL || data == NULL || size == 0 ||
        size > OFFGRID_SX1278_MAX_PAYLOAD || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    RETURN_ON_ERROR(offgrid_sx1278_standby(r));
    RETURN_ON_ERROR(clear_irqs(r));
    RETURN_ON_ERROR(write_reg(r, REG_FIFO_ADDR_PTR, 0x00U));
    for (size_t i = 0; i < size; ++i) {
        RETURN_ON_ERROR(write_reg(r, REG_FIFO, data[i]));
    }
    RETURN_ON_ERROR(write_reg(r, REG_PAYLOAD_LEN, (uint8_t)size));
    /* DIO0 mapping 01 = TxDone. */
    RETURN_ON_ERROR(write_reg(r, REG_DIO_MAPPING1, 0x40U));
    RETURN_ON_ERROR(set_mode(r, MODE_TX));
    const esp_err_t err = wait_flag(r, IRQ_TX_DONE, timeout_ms, NULL, NULL);
    if (err == ESP_OK) r->stats.tx_ok++;
    if (err == ESP_ERR_TIMEOUT) r->stats.tx_timeouts++;
    /* Leave the device in standby even if a packet timed out. */
    const esp_err_t stop = offgrid_sx1278_standby(r);
    const esp_err_t clear = clear_irqs(r);
    if (err != ESP_OK) return err;
    if (stop != ESP_OK) return stop;
    return clear;
}

esp_err_t offgrid_sx1278_receive(offgrid_sx1278_t *r, uint8_t *buffer,
                                  size_t capacity, size_t *out_len,
                                  offgrid_sx1278_packet_info_t *info,
                                  uint32_t timeout_ms)
{
    if (r == NULL || buffer == NULL || out_len == NULL || capacity == 0 ||
        timeout_ms == 0) return ESP_ERR_INVALID_ARG;
    *out_len = 0;
    if (info != NULL) memset(info, 0, sizeof(*info));

    /* Receive continuously across short application polling intervals.
     * A LoRa packet with the v1.0 fixed profile can take several hundred
     * milliseconds. In particular, a 120 ms poll timeout is NOT an RF
     * reception timeout and must never interrupt an ongoing packet. */
    if (!r->rx_listening) {
        RETURN_ON_ERROR(offgrid_sx1278_standby(r));
        RETURN_ON_ERROR(clear_irqs(r));
        /* DIO0 mapping 00 = RxDone; DIO1 mapping 00 = RxTimeout. */
        RETURN_ON_ERROR(write_reg(r, REG_DIO_MAPPING1, 0x00U));
        RETURN_ON_ERROR(set_mode(r, MODE_RX_CONTINUOUS));
    }
    uint8_t flags = 0;
    bool dio0_high = false;
    const esp_err_t err = wait_flag(r, IRQ_RX_DONE, timeout_ms,
                                    &flags, &dio0_high);
    if (err == ESP_ERR_TIMEOUT) {
        /* Keep RX_CONTINUOUS active. A packet may still be in flight. */
        r->stats.rx_timeouts++;
        return err;
    }
    if (err != ESP_OK) {
        (void)offgrid_sx1278_standby(r);
        (void)clear_irqs(r);
        return err;
    }
    if ((flags & IRQ_CRC_ERROR) != 0) {
        r->stats.rx_crc_errors++;
        /* Clear the packet IRQ but keep the receiver listening. */
        (void)clear_irqs(r);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t count = 0;
    RETURN_ON_ERROR(read_reg(r, REG_RX_BYTES, &count));
    if (count == 0 || count > OFFGRID_SX1278_MAX_PAYLOAD || count > capacity) {
        r->stats.rx_oversize++;
        (void)clear_irqs(r);
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t start = 0;
    RETURN_ON_ERROR(read_reg(r, REG_FIFO_RX_CURRENT, &start));
    RETURN_ON_ERROR(write_reg(r, REG_FIFO_ADDR_PTR, start));
    for (uint8_t i = 0; i < count; ++i) {
        RETURN_ON_ERROR(read_reg(r, REG_FIFO, &buffer[i]));
    }
    int8_t snr_q4 = 0;
    uint8_t snr_raw = 0, rssi_raw = 0;
    RETURN_ON_ERROR(read_reg(r, REG_PKT_SNR, &snr_raw));
    RETURN_ON_ERROR(read_reg(r, REG_PKT_RSSI, &rssi_raw));
    snr_q4 = (int8_t)snr_raw;
    if (info != NULL) {
        info->snr_db = (float)snr_q4 / 4.0f;
        info->rssi_dbm = (int16_t)(-164 + rssi_raw +
                                  (snr_q4 < 0 ? snr_q4 / 4 : 0));
        info->dio0_asserted = dio0_high;
    }
    *out_len = count;
    r->stats.rx_ok++;
    /* RX_CONTINUOUS automatically resumes packet detection. Acknowledge
     * RxDone without going to standby or resetting the receive preamble. */
    return clear_irqs(r);
}

void offgrid_sx1278_counters(const offgrid_sx1278_t *r,
                             offgrid_sx1278_counters_t *out)
{
    if (out == NULL) return;
    if (r == NULL) memset(out, 0, sizeof(*out));
    else *out = r->stats;
}

void offgrid_sx1278_close(offgrid_sx1278_t *r)
{
    if (r == NULL) return;
    if (r->device != NULL) {
        (void)offgrid_sx1278_standby(r);
        spi_bus_remove_device(r->device);
    }
    if (r->owns_bus) spi_bus_free(SPI2_HOST);
    free(r);
}
