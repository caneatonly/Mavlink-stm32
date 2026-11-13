#ifndef TEXT_FORMAT_H
#define TEXT_FORMAT_H

#include <stdint.h>
#include <stdio.h>

/* Fixed-point output avoids enabling floating-point printf on the MCU. */
static inline void Text_FormatFixed(char *out, size_t capacity, int32_t value,
                                    uint32_t scale, int decimals) {
    uint32_t magnitude = value < 0 ? 0U - (uint32_t)value : (uint32_t)value;
    snprintf(out, capacity, "%s%lu.%0*lu", value < 0 ? "-" : "",
             (unsigned long)(magnitude / scale), decimals,
             (unsigned long)(magnitude % scale));
}

#endif
