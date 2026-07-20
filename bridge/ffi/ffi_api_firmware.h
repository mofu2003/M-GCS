#ifndef MGCORE_FFI_API_FIRMWARE_H
#define MGCORE_FFI_API_FIRMWARE_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_vehicle_request_firmware_version(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_firmware(mg_core_t* core);
MGCORE_API char* mg_firmware_get_type(mg_core_t* core);
MGCORE_API char* mg_firmware_request_version(mg_core_t* core);
MGCORE_API char* mg_firmware_get_version(mg_core_t* core);
MGCORE_API char* mg_firmware_flash(mg_core_t* core, const char* firmware_path);
MGCORE_API char* mg_firmware_flash_status(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_FIRMWARE_H */
