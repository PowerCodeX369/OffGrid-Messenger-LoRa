/* Pure v1.2 half-duplex contention timing policy. RF owner task only.
 * CAD is a best-effort preamble detector: it cannot guarantee no collision.
 */
#ifndef OFFGRID_LORA_SCHEDULE_H
#define OFFGRID_LORA_SCHEDULE_H
#include <stdint.h>
/* Backoff is independent of the browser's ACK timeout. MAC suffix breaks
 * accidental repeated equal pseudorandom samples across two transmitters. */
static inline uint32_t og_busy_backoff_ms(uint32_t random, unsigned busy_count, uint8_t mac_suffix) {
    if (busy_count > 8u) busy_count=8u;
    return 65u + (random % 190u) + busy_count * 35u + (mac_suffix % 17u);
}
static inline uint32_t og_air_retry_jitter_ms(uint32_t random, unsigned attempt, uint8_t mac_suffix) {
    if (attempt > 6u) attempt=6u;
    return 110u + (random % 235u) + attempt * 60u + (mac_suffix % 29u);
}
#endif
