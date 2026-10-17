#ifndef DIAGNOSTIC_CLOCK_H
#define DIAGNOSTIC_CLOCK_H
#include <stdint.h>
/* Main-loop sampling must occur at least once per60000ms hardware tick cycle. */
typedef struct { uint32_t uptime; uint16_t sampled; } hl_diagnostic_clock_t;
static uint32_t Hl_DiagnosticNow(hl_diagnostic_clock_t *clock, uint16_t now)
{
    uint16_t elapsed;
    elapsed=(now>=clock->sampled) ? (uint16_t)(now-clock->sampled) :
        (uint16_t)(60000U-clock->sampled+now);
    clock->uptime+=elapsed;
    clock->sampled=now;
    return clock->uptime;
}
#endif
