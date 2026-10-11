# SITL 任务场景证据

- 时间：2026-10-11 03:48:00 CST
- 目标：/Users/fansicheng/Projects/ardupilot/build/sitl/bin/arducopter --model quad -I0 --serial0 udpclient:127.0.0.1:14550
- 驱动：build_check/test_sitl_mission_scenario
- 退出码：0
- 复算：tools/sitl-mission-scenario.sh --vehicle arducopter --model quad --port 14550 --evidence docs/sitl-scenario-evidence.md

```text
  [PASS] connect request accepted
  [PASS] link reaches connected state (heartbeat received)
  [PASS] vehicle system id is known
  [PASS] mission document created
  [PASS] three waypoints added to the document
  [PASS] mission document validates
  [PASS] upload accepted (transfer started)
  [PASS] flight controller accepted the mission (MISSION_ACCEPTED)
  [PASS] download started
  [PASS] read-back matches the document (1 m tolerance)
  [PASS] link reports disconnected after disconnect
  [PASS] reconnect request accepted
  [PASS] link recovers: connected again after reconnect
  [PASS] download after reconnect started
  [PASS] flight controller served the mission again after reconnect
  [PASS] vehicle still holds the uploaded waypoints after the link drop
  [PASS] mission document rebuilt from the vehicle after reconnect
  [PASS] rebuilt document carries the three enabled waypoints
  [PASS] rebuilt document keeps the uploaded coordinates
  [PASS] recovered document matches the vehicle read-back
=== SITL scenario: 20 passed, 0 failed ===
[EVIDENCE] endpoint=127.0.0.1:14550
[EVIDENCE] connect_state=connected
[EVIDENCE] system_id=1
[EVIDENCE] vehicle_type=2
[EVIDENCE] flight_mode=Stabilize
[EVIDENCE] firmware={"autopilot_type":3,"vehicle_type":2,"system_id":1,"flight_mode":"Stabilize"}
[EVIDENCE] firmware_is_ardupilot=true
[EVIDENCE] doc_revision=4
[EVIDENCE] doc_bytes=853
[EVIDENCE] upload_accepted=true
[EVIDENCE] upload_state=idle
[EVIDENCE] upload_result=0
[EVIDENCE] upload_result_text=飞控接受任务
[EVIDENCE] upload_retry_count=0
[EVIDENCE] verify_matches=true
[EVIDENCE] verify_json={"success":true,"matches":true,"difference_count":0,"expected_count":3,"vehicle_count":3,"differences":[],"normalization_count":3,"normalizations":[{"index":0,"field":"param2","expected":"2.000000","actual":"0.000000","reason":"该机型不存储此字段（由机型专属参数决定）"},{"index":1,"field":"param2","expected":"2.000000","actual":"0.000000","reason":"该机型不存储此字段（由机型专属参数决定）"},{"index":2,"field":"param2","expected":"2.000000","actual":"0.000000","reason":"该机型不存储此字段（由机型专属参数决定）"}],"home_sync_count":0,"home_sync":[]}
[EVIDENCE] readback_plan_bytes=751
[EVIDENCE] disconnect_state=disconnected
[EVIDENCE] reconnect_state=connected
[EVIDENCE] reconnect_download_state=idle
[EVIDENCE] reconnect_download_result=0
[EVIDENCE] reconnect_readback_json={"valid":true,"count":3,"current_index":0,"items":[{"seq":0,"frame":3,"frame_name":"GLOBAL_REL_ALT","command":16,"command_name":"NAV_WAYPOINT","current":0,"autocontinue":1,"param1":0,"param2":0,"param3":0,"param4":0,"x":-353632608,"y":1491652352,"z":60,"lat":-35.363261,"lon":149.16524},{"seq":1,"frame":3,"frame_name":"GLOBAL_REL_ALT","command":16,"command_name":"NAV_WAYPOINT","current":0,"autocontinue":1,"param1":0,"param2":0,"param3":0,"param4":0,"x":-353628000,"y":1491660032,"z":70,"lat":-35.3628,"lon":149.166},{"seq":2,"frame":3,"frame_name":"GLOBAL_REL_ALT","command":16,"command_name":"NAV_WAYPOINT","current":0,"autocontinue":1,"param1":0,"param2":0,"param3":0,"param4":0,"x":-353620000,"y":1491670016,"z":80,"lat":-35.362,"lon":149.167}]}
[EVIDENCE] reconnect_readback_count=3
[EVIDENCE] reconnect_document_waypoints=3
[EVIDENCE] reconnect_document_revision=1
[EVIDENCE] reconnect_verify_matches=true
[EVIDENCE] reconnect_verify_json={"success":true,"matches":true,"difference_count":0,"expected_count":3,"vehicle_count":3,"differences":[],"normalization_count":0,"normalizations":[],"home_sync_count":0,"home_sync":[]}
[EVIDENCE] checks_passed=20
[EVIDENCE] checks_failed=0
```
