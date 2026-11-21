#ifndef STATUS_REPORTER_H
#define STATUS_REPORTER_H

#include "vendor_protocol.h"

#define STATUS_TEXT_PERIOD_MS 200U
#define STATUS_TEXT_CAPACITY 384U

/* Writes must consume/copy the bytes before returning (blocking HAL in main). */
typedef bool (*StatusWrite_t)(void *context, const uint8_t *data, uint16_t length);

typedef struct {
    VendorParser_t parser;
    GimbalStatus_t status;
    StatusWrite_t write_text;
    void *context;
    uint32_t last_report_ms;
    bool status_pending;
    bool report_started;
} StatusReporter_t;

void StatusReporter_Init(StatusReporter_t *bridge, StatusWrite_t write_text, void *context);
bool StatusReporter_FeedStatus(StatusReporter_t *bridge, uint8_t byte);
bool StatusReporter_Report(StatusReporter_t *bridge, uint32_t now_ms);

#endif
