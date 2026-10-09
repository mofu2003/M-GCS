/**
 * mock_flight_controller.h - In-process simulated flight controller
 *
 * Inspired by QGC's MockLink. Generates telemetry, responds to parameter
 * requests, mission protocol, and command ACKs via InMemoryLink.
 *
 * Wiring: link.receive_cb → gcs.feed_raw (FC→GCS), gcs.send_cb → link.send,
 *         then poll link.take_outgoing() → mock.on_gcs_data() (GCS→FC).
 */

#ifndef MOCK_FLIGHT_CONTROLLER_H
#define MOCK_FLIGHT_CONTROLLER_H

#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "ardupilotmega/mavlink.h"
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

class MockFlightController {
public:
    enum FirmwareType { ArduPilot, PX4 };
    enum VehicleType  { Copter, Plane, Rover };

    struct Config {
        FirmwareType firmware;
        VehicleType  vehicle;
        uint8_t system_id;
        uint8_t component_id;
        Config() : firmware(ArduPilot), vehicle(Copter), system_id(1), component_id(1) {}
    };

    struct ParamDef { float value; uint8_t type; };

    // Monitoring
    struct CommandRecord { uint16_t command; float p1,p2,p3,p4; int32_t x,y,z; uint8_t frame; };
    std::vector<CommandRecord> received_cmds;
    std::vector<std::string>   received_params_set;
    std::vector<std::string>   received_param_reads;
    int  mission_items_received = 0;
    bool mission_clear_received = false;
    bool mission_set_current_received = false;
    int  mission_set_current_seq = -1;
    bool cal_start_received = false;
    uint8_t last_cal_param1 = 0;

    // Telemetry state (modify to simulate dynamics)
    struct TelemetryState {
        int32_t lat_deg7 = 399042000, lon_deg7 = 1164074000, alt_mm = 50000;
        float roll=0, pitch=0, yaw=0, vx=0, vy=0, vz=0;
        float airspeed=0, groundspeed=0; uint8_t throttle=0;
        float voltage_v=12.6f; int8_t battery_remaining=100;
        uint8_t satellites=12, fix_type=3; uint16_t heading=0; float climb_m_s=0;
    };
    TelemetryState telem;

    MockFlightController(mg::core::transport::InMemoryLink& link, const Config& cfg = Config())
        : link_(link), config_(cfg) {}

    // Process incoming bytes from GCS (after polling link.take_outgoing())
    void on_gcs_data(const uint8_t* data, size_t size) {
        mavlink_message_t msg; mavlink_status_t st;
        for (size_t i = 0; i < size; i++)
            if (mavlink_parse_char(MAVLINK_COMM_1, data[i], &msg, &st))
                handle_message(msg);
    }

    // Tick — generates periodic telemetry
    void tick(int elapsed_ms) {
        tick_ms_ += elapsed_ms;
        if (heartbeat_enabled_ && tick_ms_ - last_hb_ms_ >= 1000) { last_hb_ms_ = tick_ms_; send_heartbeat(); }
        if (tick_ms_ - last_tel_ms_ >= 100) { last_tel_ms_ = tick_ms_; send_gps(); send_att(); send_gpos(); send_vfr(); }
        if (tick_ms_ - last_bat_ms_ >= 1000) { last_bat_ms_ = tick_ms_; send_batt(); send_sys(); }
        if (param_streaming_) stream_params();
    }

    // Parameters
    void add_param(const std::string& n, float v, uint8_t t=9) { params_[n]={v,t}; }
    void load_default_params() {
        add_param("WPNAV_SPEED",5); add_param("WPNAV_RADIUS",2); add_param("WPNAV_SPEED_UP",2.5f);
        add_param("ATC_RAT_RLL_P",0.08f); add_param("ATC_RAT_PIT_P",0.08f); add_param("ATC_RAT_YAW_P",0.18f);
        add_param("GPS1_TYPE",1,2); add_param("GPS2_TYPE",0,2); add_param("BATT_MONITOR",4,2); add_param("ARMING_CHECK",1,2);
        add_param("FRAME_CLASS",1,2); add_param("MOT_SPIN_ARM",0.1f); add_param("MOT_SPIN_MIN",0.15f);
        add_param("MOT_PWM_MIN",1000,4); add_param("MOT_PWM_MAX",2000,4); add_param("SERIAL0_BAUD",57,4);
        add_param("COMPASS_ENABLE",1,2); add_param("FENCE_ENABLE",0,2); add_param("MAV_SYSID",1,4);
        add_param("MAV_GCS_SYSID",255,4); add_param("TELEM1_BAUD",57,4); add_param("AHRS_GPS_GAIN",1);
        add_param("INS_GYRO_FILTER",20); add_param("THR_MID",500); add_param("MIS_RESTART",0,2);
        add_param("PILOT_SPEED_DN",150);
        // 校准结果参数：校准完成后的读回校验需要它们存在（否则校验必失败）。
        add_param("INS_GYROFFS_X",0.0012f); add_param("INS_GYROFFS_Y",-0.0008f); add_param("INS_GYROFFS_Z",0.0005f);
        add_param("INS_ACCOFFS_X",0.05f); add_param("INS_ACCOFFS_Y",-0.03f); add_param("INS_ACCOFFS_Z",0.02f);
        add_param("INS_ACCSCAL_X",1.0f); add_param("INS_ACCSCAL_Y",1.0f); add_param("INS_ACCSCAL_Z",1.0f);
    }

    // Mission
    void set_mission(const std::vector<mavlink_mission_item_int_t>& v) { stored_mission_=v; }
    const std::vector<mavlink_mission_item_int_t>& stored_mission() const { return stored_mission_; }
    // 非 0 mission type（fence=1 / rally=2）的任务列表，供端到端测试断言。
    const std::vector<mavlink_mission_item_int_t>& stored_mission_type(
        uint8_t mission_type) const {
        if (mission_type == MAV_MISSION_TYPE_MISSION) return stored_mission_;
        static const std::vector<mavlink_mission_item_int_t> empty;
        const auto it = mission_by_type_.find(mission_type);
        return it == mission_by_type_.end() ? empty : it->second;
    }

    // Command ACK
    void set_next_cmd_result(uint8_t r) { next_cmd_result_=r; }

    // Failure injection
    void set_heartbeat_enabled(bool e) { heartbeat_enabled_=e; }
    void set_cmd_no_response(bool e) { cmd_no_response_=e; }
    /// 上传完成后回给 GCS 的 MISSION_ACK 类型（默认 ACCEPTED）。
    void set_mission_ack_type(uint8_t type) { mission_ack_type_=type; }
    /// 让飞控不再自动回 MISSION_ACK（用于只保留测试注入的那一条 ACK）。
    void set_mission_ack_suppressed(bool suppress) { mission_ack_suppressed_=suppress; }
    /// 回 MISSION_ACK 时伪造来源（用于验证"别的飞机的 ACK 不能结束本机上传"）。
    void set_mission_ack_source(uint8_t sys, uint8_t comp) {
        mission_ack_sys_=sys; mission_ack_comp_=comp; mission_ack_source_override_=true; }
    void set_param_drop(int n) { param_drop_=n; }

    // 新增故障注入（移植自 QGC MockLink）
    /// 前 N 次命令不响应，之后正常响应（测试命令重试）
    void set_cmd_drop_count(int n) { cmd_drop_count_=n; cmd_dropped_so_far_=0; }
    /// 参数列表不响应（测试 PARAM_REQUEST_LIST 超时重发）
    void set_param_list_no_response(bool e) { param_list_no_response_=e; }
    /// 通信丢失：停止所有出站消息
    void set_comm_lost(bool e) { comm_lost_=e; }
    /// 任务传输第一次不响应，重试时正常（测试任务超时重发）
    void set_mission_first_attempt_drop(bool e) { mission_first_drop_=e; mission_dropped_=false; }

    // --- MAVLink FTP (FILE_TRANSFER_PROTOCOL) ---
    /// 注册一个可被 FTP 读取的虚拟文件；未注册的路径返回 FILE_NOT_FOUND。
    void set_ftp_file(const std::string& path, const std::string& content) {
        ftp_files_[path] = content;
    }

    /// 发送一条 LOG_ENTRY，模拟飞控的日志列表。
    void send_log_entry(uint16_t id, uint32_t time_utc, uint32_t size) {
        mavlink_message_t m;
        mavlink_msg_log_entry_pack(config_.system_id, config_.component_id, &m,
                                   id, 1, id, time_utc, size);
        send(m);
    }

    /// 发送一条 STATUSTEXT，模拟飞控的提示/告警文本。
    void send_statustext(const std::string& text, uint8_t severity) {
        mavlink_message_t m;
        mavlink_msg_statustext_pack(config_.system_id, config_.component_id, &m,
                                    severity, text.c_str(), 0, 0);
        send(m);
    }

    /// 发送 MISSION_ITEM_REACHED，模拟飞控到达某个航点。
    void send_mission_item_reached(uint16_t seq) {
        mavlink_message_t m;
        mavlink_msg_mission_item_reached_pack(config_.system_id, config_.component_id,
                                              &m, seq);
        send(m);
    }

    /// 发送 MISSION_CURRENT，模拟飞控上报当前目标项。
    void send_mission_current(uint16_t seq) {
        mavlink_message_t m;
        mavlink_msg_mission_current_pack(config_.system_id, config_.component_id,
                                         &m, seq, 0, 0, 0, 0, 0, 0);
        send(m);
    }

    /// 发送 HOME_POSITION，模拟飞控上报 home 点（经纬度用度、高度用米）。
    void send_home_position(double lat_deg, double lon_deg, double alt_m) {
        const float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        mavlink_message_t m;
        mavlink_msg_home_position_pack(
            config_.system_id, config_.component_id, &m,
            static_cast<int32_t>(lat_deg * 1e7),
            static_cast<int32_t>(lon_deg * 1e7),
            static_cast<int32_t>(alt_m * 1000.0),
            0.0f, 0.0f, 0.0f, q, 0.0f, 0.0f, 0.0f, 0);
        send(m);
    }

    // 故障注入状态（public，供测试断言）
    int cmd_drop_count_=0, cmd_dropped_so_far_=0;
    bool param_list_no_response_=false;
    bool comm_lost_=false;
    bool mission_first_drop_=false, mission_dropped_=false;
    bool mission_use_compat_request_=false;
    bool mission_download_use_compat_item_=false;

    /// 按当前旋钮回一个 MISSION_ACK（类型/来源可注入）。
    void send_mission_ack(uint8_t mission_type) {
        if (mission_ack_suppressed_) return;
        const uint8_t sys = mission_ack_source_override_ ? mission_ack_sys_ : config_.system_id;
        const uint8_t comp = mission_ack_source_override_ ? mission_ack_comp_ : config_.component_id;
        mavlink_message_t a;
        mavlink_msg_mission_ack_pack(sys, comp, &a,
            0, MAV_COMP_ID_AUTOPILOT1, mission_ack_type_, mission_type, 0);
        send(a);
    }

    void set_mission_use_compat_request(bool e) { mission_use_compat_request_=e; }
    void set_mission_download_use_compat_item(bool e) { mission_download_use_compat_item_=e; }

private:
    mg::core::transport::InMemoryLink& link_;
    Config config_;
    int tick_ms_=0, last_hb_ms_=0, last_tel_ms_=0, last_bat_ms_=0;
    bool heartbeat_enabled_=true, cmd_no_response_=false;
    uint8_t mission_ack_type_=MAV_MISSION_ACCEPTED;
    uint8_t mission_ack_sys_=0, mission_ack_comp_=0;
    bool mission_ack_source_override_=false;
    bool mission_ack_suppressed_=false;
    std::map<std::string,ParamDef> params_;
    bool param_streaming_=false; int param_idx_=0;
    std::vector<std::pair<std::string,ParamDef>> param_cache_;
    int param_drop_=0;
    std::map<std::string,std::string> ftp_files_;
    std::string ftp_open_path_;
    uint8_t ftp_session_=0;
    std::vector<mavlink_mission_item_int_t> stored_mission_;
    std::map<uint8_t, std::vector<mavlink_mission_item_int_t>> mission_by_type_;
    int mission_recv_=0, mission_total_=0;
    uint8_t next_cmd_result_=0;

    // mission type 0 uses stored_mission_; fence/rally live in mission_by_type_.
    std::vector<mavlink_mission_item_int_t>& mission_store(uint8_t mission_type) {
        if (mission_type == MAV_MISSION_TYPE_MISSION) return stored_mission_;
        return mission_by_type_[mission_type];
    }

private:

    void send(mavlink_message_t& m) {
        if (comm_lost_) return;  // 通信丢失时不出站
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len=mavlink_msg_to_send_buffer(buf,&m);
        link_.inject_incoming(buf,len);
    }

    // --- Telemetry ---
    void send_heartbeat() {
        mavlink_message_t m;
        uint8_t t=(config_.vehicle==Copter)?MAV_TYPE_QUADROTOR:(config_.vehicle==Plane)?MAV_TYPE_FIXED_WING:MAV_TYPE_GROUND_ROVER;
        uint8_t a=(config_.firmware==ArduPilot)?MAV_AUTOPILOT_ARDUPILOTMEGA:MAV_AUTOPILOT_PX4;
        mavlink_msg_heartbeat_pack(config_.system_id,config_.component_id,&m,t,a,MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,0,MAV_STATE_STANDBY);
        send(m);
    }
    void send_gps() {
        mavlink_message_t m;
        // gps_raw_int_pack: sys,comp,msg, time_usec,fix_type,lat,lon,alt,eph,epv,vel,cog,sat,alt_ellip,h_acc,v_acc,vel_acc,hdg_acc,yaw
        mavlink_msg_gps_raw_int_pack(config_.system_id,config_.component_id,&m,
            (uint64_t)tick_ms_*1000, telem.fix_type,
            telem.lat_deg7, telem.lon_deg7, telem.alt_mm,
            65535, 65535, 0, 0, telem.satellites,
            0, 0, 0, 0, 0, 0);
        send(m);
    }
    void send_att() {
        mavlink_message_t m;
        mavlink_msg_attitude_pack(config_.system_id,config_.component_id,&m,(uint32_t)tick_ms_,
            telem.roll,telem.pitch,telem.yaw,telem.roll*0.1f,telem.pitch*0.1f,telem.yaw*0.05f);
        send(m);
    }
    void send_gpos() {
        mavlink_message_t m;
        mavlink_msg_global_position_int_pack(config_.system_id,config_.component_id,&m,
            (uint32_t)tick_ms_,telem.lat_deg7,telem.lon_deg7,telem.alt_mm,telem.alt_mm,
            telem.vx,telem.vy,telem.vz,telem.heading);
        send(m);
    }
    void send_vfr() {
        mavlink_message_t m;
        // vfr_hud: airspeed,groundspeed,heading(int16),throttle,alt,climb
        mavlink_msg_vfr_hud_pack(config_.system_id,config_.component_id,&m,
            telem.airspeed,telem.groundspeed,(int16_t)telem.heading,telem.throttle,
            telem.alt_mm/1000.0f,telem.climb_m_s);
        send(m);
    }
    void send_batt() {
        mavlink_message_t m;
        uint16_t v[10]; v[0]=(uint16_t)(telem.voltage_v*1000); for(int i=1;i<10;i++) v[i]=UINT16_MAX;
        // battery_status: sys,comp,msg, id,batt_func,type,temp,voltages,current_batt,cur_cons,energy_cons,remaining,time_rem,charge_state,volt_ext,mode,fault
        uint16_t vext[4]={0,0,0,0};
        mavlink_msg_battery_status_pack(config_.system_id,config_.component_id,&m,
            0, MAV_BATTERY_FUNCTION_ALL, MAV_BATTERY_TYPE_LIPO, 2500, v,
            0, 0, 0, telem.battery_remaining, 0, 0, vext, 0, 0);
        send(m);
    }
    void send_sys() {
        mavlink_message_t m;
        // sys_status: sensors_present,sensors_enabled,sensors_health,load,voltage,current,remaining,drop_rate,errors_comm,err1,err2,err3,err4,ext_present,ext_enabled,ext_health
        mavlink_msg_sys_status_pack(config_.system_id,config_.component_id,&m,
            0,0,0,0, (uint16_t)(telem.voltage_v*1000), 0, telem.battery_remaining,
            0,0,0,0,0,0, 0,0,0);
        send(m);
    }

    // --- Param protocol ---
    void send_pv(const std::string& n,float v,uint8_t t,uint16_t idx) {
        if(param_drop_>0){param_drop_--;return;}
        mavlink_message_t m; char nb[16]={};
        memcpy(nb,n.c_str(),n.size()<16?n.size():15);
        mavlink_msg_param_value_pack(config_.system_id,config_.component_id,&m,nb,v,t,(uint16_t)params_.size(),idx);
        send(m);
    }
    void stream_params() {
        for(int i=0;i<5&&param_idx_<(int)param_cache_.size();param_idx_++,i++)
            send_pv(param_cache_[param_idx_].first,param_cache_[param_idx_].second.value,param_cache_[param_idx_].second.type,(uint16_t)param_idx_);
        if(param_idx_>=(int)param_cache_.size()) param_streaming_=false;
    }

    // --- Message handler ---
    void handle_message(const mavlink_message_t& msg) {
        switch(msg.msgid) {
        case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
            if (param_list_no_response_) break;  // 不响应参数列表请求
            param_cache_.clear(); for(auto&[n,d]:params_) param_cache_.push_back({n,d});
            param_idx_=0; param_streaming_=true; break;
        case MAVLINK_MSG_ID_PARAM_REQUEST_READ: {
            mavlink_param_request_read_t r; mavlink_msg_param_request_read_decode(&msg,&r);
            char nb[17]={}; memcpy(nb,r.param_id,16); std::string pn(nb);
            if (!pn.empty()) received_param_reads.push_back(pn);
            auto it=params_.find(pn); if(it!=params_.end()) send_pv(pn,it->second.value,it->second.type,(uint16_t)std::distance(params_.begin(),it));
            break; }
        case MAVLINK_MSG_ID_PARAM_SET: {
            mavlink_param_set_t ps; mavlink_msg_param_set_decode(&msg,&ps);
            char nb[17]={}; memcpy(nb,ps.param_id,16); std::string pn(nb);
            received_params_set.push_back(pn);
            auto it=params_.find(pn); if(it!=params_.end()){it->second.value=ps.param_value; send_pv(pn,ps.param_value,it->second.type,(uint16_t)std::distance(params_.begin(),it));}
            else{params_[pn]={ps.param_value,9}; send_pv(pn,ps.param_value,9,(uint16_t)(params_.size()-1));}
            break; }
        case MAVLINK_MSG_ID_MISSION_REQUEST_LIST: {
            // mission_count: target_sys,target_comp,msg, target_sys,target_comp,count,mission_type,opaque_id
            mavlink_mission_request_list_t req;
            mavlink_msg_mission_request_list_decode(&msg,&req);
            mavlink_message_t m; mavlink_msg_mission_count_pack(config_.system_id,config_.component_id,&m,
                0, MAV_COMP_ID_AUTOPILOT1, (uint16_t)mission_store(req.mission_type).size(), req.mission_type, 0);
            send(m); break; }
        case MAVLINK_MSG_ID_MISSION_REQUEST_INT: {
            // 故障注入：首次任务请求不响应（测试任务超时重发）
            if(mission_first_drop_ && !mission_dropped_){ mission_dropped_=true; break; }
            mavlink_mission_request_int_t r; mavlink_msg_mission_request_int_decode(&msg,&r);
            auto& store = mission_store(r.mission_type);
            if(r.seq<store.size()){
                mavlink_message_t m;
                if (mission_download_use_compat_item_) {
                    const auto& src = store[r.seq];
                    mavlink_msg_mission_item_pack(
                        config_.system_id, config_.component_id, &m,
                        0, MAV_COMP_ID_AUTOPILOT1,
                        src.seq, src.frame, src.command,
                        src.current, src.autocontinue,
                        src.param1, src.param2, src.param3, src.param4,
                        static_cast<float>(static_cast<double>(src.x) / 1e7),
                        static_cast<float>(static_cast<double>(src.y) / 1e7),
                        src.z, src.mission_type);
                } else {
                    auto item = store[r.seq];
                    item.mission_type = r.mission_type;
                    mavlink_msg_mission_item_int_encode(config_.system_id,config_.component_id,&m,&item);
                }
                send(m);
            }
            break; }
        case MAVLINK_MSG_ID_MISSION_ITEM_INT: {
            if (mission_use_compat_request_) break;
            mavlink_mission_item_int_t it; mavlink_msg_mission_item_int_decode(&msg,&it);
            auto& store = mission_store(it.mission_type);
            mission_recv_++; if(it.seq>=(int)store.size()) store.resize(it.seq+1);
            mission_items_received = mission_recv_;
            store[it.seq]=it;
            if(mission_recv_>=mission_total_){
                // mission_ack: target_sys,target_comp,msg, target_sys,target_comp,type,mission_type,opaque_id
                send_mission_ack(it.mission_type);
            } else {
                // mission_request_int: target_sys,target_comp,msg, target_sys,target_comp,seq,mission_type
                mavlink_message_t r; mavlink_msg_mission_request_int_pack(config_.system_id,config_.component_id,&r,
                    0, MAV_COMP_ID_AUTOPILOT1, (uint16_t)mission_recv_, it.mission_type);
                send(r);
            }
            break; }
        case MAVLINK_MSG_ID_MISSION_ITEM: {
            mavlink_mission_item_t it; mavlink_msg_mission_item_decode(&msg,&it);
            auto& store = mission_store(it.mission_type);
            mission_recv_++; if(it.seq>=(int)store.size()) store.resize(it.seq+1);
            mission_items_received = mission_recv_;
            auto& stored = store[it.seq];
            memset(&stored, 0, sizeof(stored));
            stored.seq = it.seq;
            stored.frame = it.frame;
            stored.command = it.command;
            stored.current = it.current;
            stored.autocontinue = it.autocontinue;
            stored.param1 = it.param1;
            stored.param2 = it.param2;
            stored.param3 = it.param3;
            stored.param4 = it.param4;
            stored.x = (int32_t)(it.x * 1e7f);
            stored.y = (int32_t)(it.y * 1e7f);
            stored.z = it.z;
            stored.mission_type = it.mission_type;
            if(mission_recv_>=mission_total_){
                send_mission_ack(it.mission_type);
            } else {
                mavlink_message_t r;
                if (mission_use_compat_request_) {
                    mavlink_msg_mission_request_pack(config_.system_id,config_.component_id,&r,
                        0, MAV_COMP_ID_AUTOPILOT1, (uint16_t)mission_recv_, it.mission_type);
                } else {
                    mavlink_msg_mission_request_int_pack(config_.system_id,config_.component_id,&r,
                        0, MAV_COMP_ID_AUTOPILOT1, (uint16_t)mission_recv_, it.mission_type);
                }
                send(r);
            }
            break; }
        case MAVLINK_MSG_ID_MISSION_COUNT: {
            mavlink_mission_count_t mc; mavlink_msg_mission_count_decode(&msg,&mc);
            auto& store = mission_store(mc.mission_type);
            mission_total_=mc.count; mission_recv_=0; store.clear(); store.resize(mission_total_);
            if(mission_total_>0){mavlink_message_t r;
                if (mission_use_compat_request_) {
                    mavlink_msg_mission_request_pack(config_.system_id,config_.component_id,&r,
                        0, MAV_COMP_ID_AUTOPILOT1, 0, mc.mission_type);
                } else {
                    mavlink_msg_mission_request_int_pack(config_.system_id,config_.component_id,&r,
                        0, MAV_COMP_ID_AUTOPILOT1, 0, mc.mission_type);
                }
                send(r);}
            break; }
        case MAVLINK_MSG_ID_MISSION_CLEAR_ALL: {
            mavlink_mission_clear_all_t ca; mavlink_msg_mission_clear_all_decode(&msg,&ca);
            mission_clear_received=true; mission_store(ca.mission_type).clear();
            send_mission_ack(ca.mission_type); break; }
        case MAVLINK_MSG_ID_MISSION_SET_CURRENT: {
            mission_set_current_received=true; mavlink_mission_set_current_t sc; mavlink_msg_mission_set_current_decode(&msg,&sc); mission_set_current_seq=sc.seq;
            break; }
        case MAVLINK_MSG_ID_COMMAND_LONG: {
            mavlink_command_long_t c; mavlink_msg_command_long_decode(&msg,&c);
            if (c.target_system != config_.system_id ||
                (c.target_component != config_.component_id && c.target_component != MAV_COMP_ID_ALL)) {
                break;
            }
            received_cmds.push_back({c.command,c.param1,c.param2,c.param3,c.param4,(int32_t)(c.param5*1e7),(int32_t)(c.param6*1e7),(int32_t)c.param7,static_cast<uint8_t>(MAV_FRAME_GLOBAL)});
            if(c.command==MAV_CMD_PREFLIGHT_CALIBRATION){cal_start_received=true; last_cal_param1=(uint8_t)c.param1;}
            // 故障注入：前 N 次命令不响应（测试命令重试）
            if(cmd_drop_count_>0 && cmd_dropped_so_far_<cmd_drop_count_){
                cmd_dropped_so_far_++;
                break;
            }
            if(!cmd_no_response_){
                mavlink_message_t a; mavlink_msg_command_ack_pack(config_.system_id,config_.component_id,&a,
                    c.command, next_cmd_result_, 0, 0, 0, 0);
                send(a); next_cmd_result_=0;
            }
            break; }
        case MAVLINK_MSG_ID_COMMAND_INT: {
            mavlink_command_int_t c; mavlink_msg_command_int_decode(&msg,&c);
            received_cmds.push_back({c.command,c.param1,c.param2,c.param3,c.param4,c.x,c.y,static_cast<int32_t>(c.z),c.frame});
            if(!cmd_no_response_){
                mavlink_message_t a; mavlink_msg_command_ack_pack(config_.system_id,config_.component_id,&a,
                    c.command, next_cmd_result_, 0, 0, 0, 0);
                send(a); next_cmd_result_=0;
            }
            break; }
        case MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL:
            handle_ftp(msg);
            break;
        default: break;
        }
    }

    // --- MAVLink FTP responder ---
    // 只实现读取系统文件所需的最小子集：ResetSessions / OpenFileRO /
    // ReadFile / TerminateSession。足够覆盖 @SYS/uarts.txt 之类的下载路径。
    static constexpr uint8_t kFtpOpTerminate = 1;
    static constexpr uint8_t kFtpOpReset     = 2;
    static constexpr uint8_t kFtpOpOpenRead  = 4;
    static constexpr uint8_t kFtpOpRead      = 5;
    static constexpr uint8_t kFtpRspAck      = 128;
    static constexpr uint8_t kFtpRspNak      = 129;
    static constexpr size_t  kFtpPayload     = 251;
    static constexpr size_t  kFtpHeader      = 12;
    static constexpr size_t  kFtpMaxChunk    = 239;

    static uint32_t ftp_u32(const uint8_t* p) {
        return static_cast<uint32_t>(p[0])
             | (static_cast<uint32_t>(p[1]) << 8)
             | (static_cast<uint32_t>(p[2]) << 16)
             | (static_cast<uint32_t>(p[3]) << 24);
    }

    void send_ftp(uint16_t seq, uint8_t session, uint8_t response_opcode,
                  uint8_t request_opcode, uint32_t offset,
                  const std::vector<uint8_t>& data) {
        mavlink_file_transfer_protocol_t ftp{};
        ftp.target_system = 255;
        uint8_t* p = ftp.payload;
        // FTP responses echo the request sequence + 1; the client matches on it.
        const uint16_t response_seq = static_cast<uint16_t>(seq + 1);
        p[0] = static_cast<uint8_t>(response_seq & 0xff);
        p[1] = static_cast<uint8_t>((response_seq >> 8) & 0xff);
        p[2] = session;
        p[3] = response_opcode;
        p[4] = static_cast<uint8_t>(data.size());
        p[5] = request_opcode;
        p[8]  = static_cast<uint8_t>(offset & 0xff);
        p[9]  = static_cast<uint8_t>((offset >> 8) & 0xff);
        p[10] = static_cast<uint8_t>((offset >> 16) & 0xff);
        p[11] = static_cast<uint8_t>((offset >> 24) & 0xff);
        if (!data.empty()) std::memcpy(p + kFtpHeader, data.data(), data.size());
        mavlink_message_t m;
        mavlink_msg_file_transfer_protocol_pack(
            config_.system_id, config_.component_id, &m, 0, 0, 0, p);
        send(m);
    }

    void handle_ftp(const mavlink_message_t& msg) {
        mavlink_file_transfer_protocol_t req{};
        mavlink_msg_file_transfer_protocol_decode(&msg, &req);
        const uint8_t* p = req.payload;
        const uint16_t seq = static_cast<uint16_t>(p[0] | (p[1] << 8));
        const uint8_t session = p[2];
        const uint8_t opcode = p[3];
        const uint8_t request_size = p[4];
        const uint32_t offset = ftp_u32(p + 8);

        switch (opcode) {
            case kFtpOpReset:
                ftp_session_ = static_cast<uint8_t>(ftp_session_ + 1);
                send_ftp(seq, ftp_session_, kFtpRspAck, opcode, 0, {});
                break;
            case kFtpOpTerminate:
                send_ftp(seq, session, kFtpRspAck, opcode, 0, {});
                break;
            case kFtpOpOpenRead: {
                const std::string path(reinterpret_cast<const char*>(p + kFtpHeader),
                                       request_size);
                auto it = ftp_files_.find(path);
                if (it == ftp_files_.end()) {
                    // MAV_FTP_ERR_FILE_NOT_FOUND = 10
                    send_ftp(seq, session, kFtpRspNak, opcode, 0, {10});
                    break;
                }
                ftp_open_path_ = path;
                ftp_session_ = static_cast<uint8_t>(ftp_session_ + 1);
                const uint32_t size = static_cast<uint32_t>(it->second.size());
                std::vector<uint8_t> length{
                    static_cast<uint8_t>(size & 0xff),
                    static_cast<uint8_t>((size >> 8) & 0xff),
                    static_cast<uint8_t>((size >> 16) & 0xff),
                    static_cast<uint8_t>((size >> 24) & 0xff)};
                send_ftp(seq, ftp_session_, kFtpRspAck, opcode, 0, length);
                break;
            }
            case kFtpOpRead: {
                auto it = ftp_files_.find(ftp_open_path_);
                if (it == ftp_files_.end()) {
                    send_ftp(seq, session, kFtpRspNak, opcode, 0, {1});
                    break;
                }
                const std::string& content = it->second;
                if (offset >= content.size()) {
                    // MAV_FTP_ERR_EOF = 6
                    send_ftp(seq, session, kFtpRspNak, opcode, 0, {6});
                    break;
                }
                const size_t count = std::min<size_t>(
                    std::min<size_t>(request_size, kFtpMaxChunk),
                    content.size() - offset);
                std::vector<uint8_t> chunk(
                    content.begin() + offset, content.begin() + offset + count);
                send_ftp(seq, session, kFtpRspAck, opcode, offset, chunk);
                break;
            }
            default:
                // MAV_FTP_ERR_UNKNOWN_COMMAND = 7
                send_ftp(seq, session, kFtpRspNak, opcode, 0, {7});
                break;
        }
    }
};

#endif // MOCK_FLIGHT_CONTROLLER_H
