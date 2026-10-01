/**
 * mavlink_adapter_dispatch.cc - Raw-frame compatibility and dispatch
 *
 * Definitions are out of line. Dispatch, locking, timeout checks,
 * and callback execution semantics are unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

void MavlinkAdapter::Impl::feed_mag_progress_compat(const uint8_t* data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        const uint8_t byte = data[i];
        if (mag_progress_frame_len == 0) {
            if (byte != MAVLINK_STX)
                continue;
            mag_progress_frame[mag_progress_frame_len++] = byte;
            continue;
        }
        if (mag_progress_frame_len >= mag_progress_frame.size()) {
            mag_progress_frame_len = 0;
            mag_progress_frame_expected = 0;
            continue;
        }
        mag_progress_frame[mag_progress_frame_len++] = byte;
        if (mag_progress_frame_len == 3) {
            const uint8_t payload_len = mag_progress_frame[1];
            const uint8_t incompat_flags = mag_progress_frame[2];
            mag_progress_frame_expected = 10u + payload_len + 2u + ((incompat_flags & 0x01u) ? 13u : 0u);
        }
        if (mag_progress_frame_expected == 0 || mag_progress_frame_len < mag_progress_frame_expected)
            continue;

        const uint32_t msgid = static_cast<uint32_t>(mag_progress_frame[7]) |
                               (static_cast<uint32_t>(mag_progress_frame[8]) << 8) |
                               (static_cast<uint32_t>(mag_progress_frame[9]) << 16);
        if (msgid == MAVLINK_MSG_ID_MAG_CAL_PROGRESS) {
            const uint8_t payload_len = mag_progress_frame[1];
            const uint16_t crc_offset = static_cast<uint16_t>(10u + payload_len);
            uint16_t checksum = crc_calculate(&mag_progress_frame[1], static_cast<uint16_t>(9u + payload_len));
            crc_accumulate(MAVLINK_MSG_ID_MAG_CAL_PROGRESS_CRC, &checksum);
            const uint16_t received = static_cast<uint16_t>(mag_progress_frame[crc_offset]) |
                                      (static_cast<uint16_t>(mag_progress_frame[crc_offset + 1]) << 8);
            if (checksum == received && payload_len >= MAVLINK_MSG_ID_MAG_CAL_PROGRESS_MIN_LEN) {
                mavlink_message_t message{};
                message.len = payload_len;
                message.incompat_flags = mag_progress_frame[2];
                message.compat_flags = mag_progress_frame[3];
                message.seq = mag_progress_frame[4];
                message.sysid = mag_progress_frame[5];
                message.compid = mag_progress_frame[6];
                message.msgid = msgid;
                memcpy(_MAV_PAYLOAD_NON_CONST(&message), &mag_progress_frame[10], payload_len);
                process(message);
            }
        }
        mag_progress_frame_len = 0;
        mag_progress_frame_expected = 0;
    }
}

void MavlinkAdapter::Impl::process(const mavlink_message_t& m)
{
    std::vector<std::function<void()>> callbacks;
    {
        mg::core::platform::LockGuard lk(mtx);
        msg_count++;
        link_quality.observe(m);

        switch (m.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:
            handle_heartbeat(m);
            break;
        case MAVLINK_MSG_ID_SYS_STATUS:
            handle_sys_status(m);
            break;
        case MAVLINK_MSG_ID_GPS_RAW_INT:
            handle_gps(m);
            break;
        case MAVLINK_MSG_ID_SCALED_IMU:
            handle_scaled_imu(m, 0);
            break;
        case MAVLINK_MSG_ID_SCALED_IMU2:
            handle_scaled_imu(m, 1);
            break;
        case MAVLINK_MSG_ID_SCALED_IMU3:
            handle_scaled_imu(m, 2);
            break;
        case MAVLINK_MSG_ID_SCALED_PRESSURE:
            handle_scaled_pressure(m);
            break;
        case MAVLINK_MSG_ID_ATTITUDE:
            handle_attitude(m);
            break;
        case MAVLINK_MSG_ID_VFR_HUD:
            handle_vfr_hud(m);
            break;
        case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
            handle_autopilot_version(m);
            break;
        case MAVLINK_MSG_ID_PARAM_VALUE:
            handle_param_value(m);
            break;
        case MAVLINK_MSG_ID_PARAM_ERROR:
            handle_param_error(m);
            break;
        case MAVLINK_MSG_ID_COMMAND_ACK:
            handle_command_ack(m);
            break;
        case MAVLINK_MSG_ID_COMMAND_LONG:
            handle_command_long(m);
            break;
        case 191:
            handle_mag_cal_progress(m);
            break;
        case MAVLINK_MSG_ID_MAG_CAL_REPORT:
            handle_mag_cal_report(m);
            break;
        case MAVLINK_MSG_ID_RC_CHANNELS:
            handle_rc_channels(m);
            break;
        case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:
            handle_servo_output_raw(m);
            break;
        case MAVLINK_MSG_ID_STATUSTEXT:
            handle_statustext(m);
            break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
            handle_global_position(m);
            break;
        case MAVLINK_MSG_ID_HOME_POSITION:
            handle_home_position(m);
            break;
        case MAVLINK_MSG_ID_MISSION_COUNT:
            handle_mission_count(m);
            break;
        case MAVLINK_MSG_ID_MISSION_ITEM:
            handle_mission_item(m);
            break;
        case MAVLINK_MSG_ID_MISSION_ITEM_INT:
            handle_mission_item_int(m);
            break;
        case MAVLINK_MSG_ID_MISSION_REQUEST_INT:
            handle_mission_request_int(m);
            break;
        case MAVLINK_MSG_ID_MISSION_REQUEST:
            handle_mission_request(m);
            break;
        case MAVLINK_MSG_ID_MISSION_ACK:
            handle_mission_ack(m);
            break;
        case MAVLINK_MSG_ID_MISSION_CURRENT:
            handle_mission_current(m);
            break;
        case MAVLINK_MSG_ID_MISSION_ITEM_REACHED:
            handle_mission_item_reached(m);
            break;
        case MAVLINK_MSG_ID_BATTERY_STATUS:
            handle_battery_status(m);
            break;
        case MAVLINK_MSG_ID_HIGHRES_IMU:
            handle_highres_imu(m);
            break;
        case MAVLINK_MSG_ID_EXTENDED_SYS_STATE:
            handle_extended_sys_state(m);
            break;
        // PREARM_CHECK_STATUS not in standard MAVLink v2.0 headers (ArduPilot extension)
        case MAVLINK_MSG_ID_VIBRATION:
            handle_vibration(m);
            break;
        case MAVLINK_MSG_ID_ESC_STATUS:
            handle_esc_status(m);
            break;
        // WIND_ESTIMATE not in standard MAVLink v2.0 common headers
        case MAVLINK_MSG_ID_LOG_ENTRY:
            handle_log_entry(m);
            break;
        case MAVLINK_MSG_ID_LOG_DATA:
            handle_log_data(m);
            break;
        case MAVLINK_MSG_ID_DISTANCE_SENSOR:
            handle_distance_sensor(m);
            break;
        case MAVLINK_MSG_ID_ADSB_VEHICLE:
            handle_adsb_vehicle(m);
            break;
        case 110: { // FILE_TRANSFER_PROTOCOL
            mavlink_file_transfer_protocol_t ftp_message{};
            mavlink_msg_file_transfer_protocol_decode(&m, &ftp_message);
            ftp.handle_ftp_message(ftp_message);
            break;
        }
        // 相机/云台消息
        case 261: /* CAMERA_INFORMATION */
            handle_camera_information(m);
            break;
        case 254: /* CAMERA_CAPTURE_STATUS */
            handle_camera_capture_status(m);
            break;
        case 269: /* VIDEO_STREAM_INFORMATION */
            handle_video_stream_information(m);
            break;
        case 280: /* GIMBAL_MANAGER_INFORMATION */
            handle_gimbal_manager_info(m);
            break;
        case 281: /* GIMBAL_MANAGER_STATUS */
            handle_gimbal_manager_status(m);
            break;
        default:
            break;
        }

        // 超时检查节流：每 100ms 检查一次，避免每条消息都扫描队列
        auto now = std::chrono::steady_clock::now();
        auto since_check = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_timeout_check_).count();
        if (since_check >= 100) {
            last_timeout_check_ = now;

            // 命令队列超时检查
            auto timed_out = cmd_queue.check_timeouts(
                [this](const CommandQueueEntry& e) -> bool { return send_command_queue_entry(e); });
            for (auto& entry : timed_out) {
                if (entry.result_cb) {
                    CommandAckInfo fail_info{};
                    fail_info.command = entry.command;
                    fail_info.result = 4; // MAV_RESULT_FAILED
                    fail_info.result_text = "command ack timeout - no response";
                    deferred_cbs.push_back(
                        [cb = entry.result_cb, fail_info = std::move(fail_info)]() { cb(fail_info); });
                }
            }
            cmd_queue.purge();

            // 任务传输超时检查
            mission.check_timeout();
            rally.check_timeout();
            fence.check_timeout();

            // 参数下载/写入超时
            process_param_timeouts();
        }

        // 将延迟回调移出，以便在锁释放后执行
        callbacks = std::move(deferred_cbs);
        deferred_cbs.clear();
    } // LockGuard 在此释放（RAII）

    // 在锁外执行回调（防止 POSIX 死锁）
    for (auto& cb : callbacks) {
        cb();
    }
}

} // namespace mg::core::mavlink
