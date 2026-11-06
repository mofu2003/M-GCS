#include "core/mavlink/mavlink_channel_status.h"

namespace {

thread_local mavlink_status_t channel_status[MAVLINK_COMM_NUM_BUFFERS]{};

} // namespace

extern "C" mavlink_status_t* mavlink_get_channel_status(uint8_t channel) {
    if (channel >= MAVLINK_COMM_NUM_BUFFERS) return nullptr;
    return &channel_status[channel];
}
