#pragma once

#include "mavlink_types.h"

// Generated MAVLink packers currently finalize through MAVLINK_COMM_0. Keep the
// global helper surface compatible with QGC, but make each thread own its
// channel status so independent vehicle adapters never race on current_tx_seq.
#ifndef MAVLINK_GET_CHANNEL_STATUS
#define MAVLINK_GET_CHANNEL_STATUS
#endif

#ifdef __cplusplus
extern "C" {
#endif

mavlink_status_t* mavlink_get_channel_status(uint8_t channel);

#ifdef __cplusplus
}
#endif
