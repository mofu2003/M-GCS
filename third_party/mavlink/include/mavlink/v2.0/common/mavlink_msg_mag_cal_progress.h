#pragma once
// MESSAGE MAG_CAL_PROGRESS PACKING
// ArduPilot dialect message retained in the common include tree so the
// receiver can validate and decode it without switching the whole project
// away from the existing MAVLink common headers.

#define MAVLINK_MSG_ID_MAG_CAL_PROGRESS 191

MAVPACKED(
typedef struct __mavlink_mag_cal_progress_t {
    float direction_x;
    float direction_y;
    float direction_z;
    uint8_t compass_id;
    uint8_t cal_mask;
    uint8_t cal_status;
    uint8_t attempt;
    uint8_t completion_pct;
    uint8_t completion_mask[10];
}) mavlink_mag_cal_progress_t;

#define MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN 27
#define MAVLINK_MSG_ID_MAG_CAL_PROGRESS_MIN_LEN 17
#define MAVLINK_MSG_ID_191_LEN 27
#define MAVLINK_MSG_ID_191_MIN_LEN 17
#define MAVLINK_MSG_ID_MAG_CAL_PROGRESS_CRC 92
#define MAVLINK_MSG_ID_191_CRC 92

static inline uint16_t mavlink_msg_mag_cal_progress_pack(
    uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
    uint8_t compass_id, uint8_t cal_mask, uint8_t cal_status,
    uint8_t attempt, uint8_t completion_pct, const uint8_t* completion_mask,
    float direction_x, float direction_y, float direction_z) {
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN] = {};
    _mav_put_float(buf, 0, direction_x);
    _mav_put_float(buf, 4, direction_y);
    _mav_put_float(buf, 8, direction_z);
    _mav_put_uint8_t(buf, 12, compass_id);
    _mav_put_uint8_t(buf, 13, cal_mask);
    _mav_put_uint8_t(buf, 14, cal_status);
    _mav_put_uint8_t(buf, 15, attempt);
    _mav_put_uint8_t(buf, 16, completion_pct);
    if (completion_mask) memcpy(buf + 17, completion_mask, 10);
    memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN);
#else
    mavlink_mag_cal_progress_t packet{};
    packet.direction_x = direction_x;
    packet.direction_y = direction_y;
    packet.direction_z = direction_z;
    packet.compass_id = compass_id;
    packet.cal_mask = cal_mask;
    packet.cal_status = cal_status;
    packet.attempt = attempt;
    packet.completion_pct = completion_pct;
    if (completion_mask) memcpy(packet.completion_mask, completion_mask, 10);
    memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN);
#endif
    msg->msgid = MAVLINK_MSG_ID_MAG_CAL_PROGRESS;
    return mavlink_finalize_message(msg, system_id, component_id,
        MAVLINK_MSG_ID_MAG_CAL_PROGRESS_MIN_LEN,
        MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN,
        MAVLINK_MSG_ID_MAG_CAL_PROGRESS_CRC);
}

static inline void mavlink_msg_mag_cal_progress_decode(
    const mavlink_message_t* msg, mavlink_mag_cal_progress_t* packet) {
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    packet->direction_x = _MAV_RETURN_float(msg, 0);
    packet->direction_y = _MAV_RETURN_float(msg, 4);
    packet->direction_z = _MAV_RETURN_float(msg, 8);
    packet->compass_id = _MAV_RETURN_uint8_t(msg, 12);
    packet->cal_mask = _MAV_RETURN_uint8_t(msg, 13);
    packet->cal_status = _MAV_RETURN_uint8_t(msg, 14);
    packet->attempt = _MAV_RETURN_uint8_t(msg, 15);
    packet->completion_pct = _MAV_RETURN_uint8_t(msg, 16);
    for (uint8_t i = 0; i < 10; ++i) packet->completion_mask[i] = _MAV_RETURN_uint8_t(msg, 17 + i);
#else
    uint8_t len = msg->len < MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN
        ? msg->len : MAVLINK_MSG_ID_MAG_CAL_PROGRESS_LEN;
    memset(packet, 0, sizeof(*packet));
    memcpy(packet, _MAV_PAYLOAD(msg), len);
#endif
}
