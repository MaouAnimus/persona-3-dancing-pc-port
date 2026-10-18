/* VR, Camera, and HMD stubs for 2D mode compatibility.
 * When HMD/Camera report not connected, Persona 3: Dancing in Moonlight gracefully
 * disables its optional PS VR features and executes in standard 2D TV display mode. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORBIS_OK 0
#define SCE_HMD_ERROR_NO_HEADSET           ((int32_t)0x80540001)
#define SCE_CAMERA_ERROR_NOT_CONNECTED     ((int32_t)0x802E0001)
#define SCE_COMMON_DIALOG_STATUS_NONE      0
#define SCE_COMMON_DIALOG_STATUS_RUNNING   1
#define SCE_COMMON_DIALOG_STATUS_FINISHED  2

static ABI int32_t vr_no_headset(void) { return SCE_HMD_ERROR_NO_HEADSET; }
static ABI int32_t vr_ok(void) { return ORBIS_OK; }

/* Camera: report disconnected / unattached */
static ABI int32_t camera_is_attached(int32_t index) { (void)index; return 0; }
static ABI int32_t camera_open(int32_t user, int32_t type, int32_t index, const void *param) {
    (void)user; (void)type; (void)index; (void)param;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}
static ABI int32_t camera_close(int32_t handle) { (void)handle; return ORBIS_OK; }
static ABI int32_t camera_start(int32_t handle, const void *param) { (void)handle; (void)param; return SCE_CAMERA_ERROR_NOT_CONNECTED; }
static ABI int32_t camera_stop(int32_t handle) { (void)handle; return ORBIS_OK; }
static ABI int32_t camera_get_frame_data(int32_t handle, void *frame) { (void)handle; (void)frame; return SCE_CAMERA_ERROR_NOT_CONNECTED; }
static ABI int32_t camera_set_config(int32_t handle, const void *config) { (void)handle; (void)config; return SCE_CAMERA_ERROR_NOT_CONNECTED; }
static ABI int32_t camera_set_video_sync(int32_t handle, const void *sync) { (void)handle; (void)sync; return SCE_CAMERA_ERROR_NOT_CONNECTED; }

/* HMD setup dialog: immediately reports finished or not running */
static ABI int32_t hmd_dialog_init(void) { return ORBIS_OK; }
static ABI int32_t hmd_dialog_open(const void *param) { (void)param; return ORBIS_OK; }
static ABI int32_t hmd_dialog_status(void) { return SCE_COMMON_DIALOG_STATUS_FINISHED; }
static ABI int32_t hmd_dialog_result(void *result) { (void)result; return ORBIS_OK; }
static ABI int32_t hmd_dialog_term(void) { return ORBIS_OK; }

static const RuntimeExport vr_exports[] = {
    /* libSceCamera */
    {"sceCameraIsAttached", camera_is_attached},
    {"p6n3Npi3YY4#libSceCamera", camera_is_attached},
    {"sceCameraOpen", camera_open},
    {"BHn83xrF92E#libSceCamera", camera_open},
    {"sceCameraClose", camera_close},
    {"OMS9LlcrvBo#libSceCamera", camera_close},
    {"sceCameraStart", camera_start},
    {"9EpRYMy7rHU#libSceCamera", camera_start},
    {"sceCameraStop", camera_stop},
    {"2G2C0nmd++M#libSceCamera", camera_stop},
    {"sceCameraGetFrameData", camera_get_frame_data},
    {"mxgMmR+1Kr0#libSceCamera", camera_get_frame_data},
    {"sceCameraSetConfig", camera_set_config},
    {"VQ+5kAqsE2Q#libSceCamera", camera_set_config},
    {"sceCameraSetVideoSync", camera_set_video_sync},
    {"wpeyFwJ+UEI#libSceCamera", camera_set_video_sync},

    /* libSceHmd */
    {"sceHmdOpen", vr_no_headset},
    {"d2g5Ij7EUzo#libSceHmd", vr_no_headset},
    {"sceHmdInitialize315", vr_no_headset},
    {"s-J66ar9g50#libSceHmd", vr_no_headset},
    {"sceHmdClose", vr_ok},
    {"6biw1XHTSqQ#libSceHmd", vr_ok},
    {"sceHmdTerminate", vr_ok},
    {"z-RMILqP6tE#libSceHmd", vr_ok},
    {"sceHmdGetDeviceInformation", vr_no_headset},
    {"thDt9upZlp8#libSceHmd", vr_no_headset},
    {"sceHmdGetDeviceInformationByHandle", vr_no_headset},
    {"1pxQfif1rkE#libSceHmd", vr_no_headset},
    {"sceHmdGetFieldOfView", vr_no_headset},
    {"NPQwYFqi0bs#libSceHmd", vr_no_headset},
    {"sceHmdGet2DEyeOffset", vr_no_headset},
    {"BWY-qKM5hxE#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionInitialize", vr_no_headset},
    {"OuygGEWkins#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionFinalize", vr_ok},
    {"ZrV5YIqD09I#libSceHmd", vr_ok},
    {"sceHmdReprojectionQueryGarlicBuffAlign", vr_no_headset},
    {"TkcANcGM0s8#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionQueryGarlicBuffSize", vr_no_headset},
    {"z0KtN1vqF2E#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionQueryOnionBuffAlign", vr_no_headset},
    {"IWybWbR-xvA#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionQueryOnionBuffSize", vr_no_headset},
    {"kLUAkN6a1e8#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionSetDisplayBuffers", vr_no_headset},
    {"E+dPfjeQLHI#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionSetUserEventStart", vr_no_headset},
    {"7as0CjXW1B8#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionStartWithOverlay", vr_no_headset},
    {"kcldQ7zLYQQ#libSceHmd", vr_no_headset},
    {"sceHmdReprojectionStop", vr_ok},
    {"vzMEkwBQciM#libSceHmd", vr_ok},
    {"sceHmdReprojectionUnsetDisplayBuffers", vr_ok},
    {"iGNNpDDjcwo#libSceHmd", vr_ok},

    /* libSceHmdSetupDialog */
    {"sceHmdSetupDialogInitialize", hmd_dialog_init},
    {"NB1Y2kA2jCY#libSceHmdSetupDialog", hmd_dialog_init},
    {"sceHmdSetupDialogOpen", hmd_dialog_open},
    {"NNgiV4T+akU#libSceHmdSetupDialog", hmd_dialog_open},
    {"sceHmdSetupDialogUpdateStatus", hmd_dialog_status},
    {"Ud7j3+RDIBg#libSceHmdSetupDialog", hmd_dialog_status},
    {"sceHmdSetupDialogGetResult", hmd_dialog_result},
    {"6lVRHMV5LY0#libSceHmdSetupDialog", hmd_dialog_result},
    {"sceHmdSetupDialogTerminate", hmd_dialog_term},
    {"+z4OJmFreZc#libSceHmdSetupDialog", hmd_dialog_term},

    /* libSceVrTracker */
    {"sceVrTrackerInit", vr_no_headset},
    {"QkRl7pART9M#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerTerm", vr_ok},
    {"IBv4P3q1pQ0#libSceVrTracker", vr_ok},
    {"sceVrTrackerGetResult", vr_no_headset},
    {"76OBvrrQXUc#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerGetTime", vr_no_headset},
    {"XoeWzXlrnMw#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerGpuSubmit", vr_ok},
    {"TVegDMLaBB8#libSceVrTracker", vr_ok},
    {"sceVrTrackerGpuWaitAndCpuProcess", vr_ok},
    {"ARhgpXvwoR0#libSceVrTracker", vr_ok},
    {"sceVrTrackerQueryMemory", vr_no_headset},
    {"K7yhYrsIBPc#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerRecalibrate", vr_no_headset},
    {"EUCaQtXXYNI#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerRegisterDevice", vr_no_headset},
    {"sIh8GwcevaQ#libSceVrTracker", vr_no_headset},
    {"sceVrTrackerUnregisterDevice", vr_ok},
    {"Q8skQqEwn5c#libSceVrTracker", vr_ok},
    {"sceVrTrackerUpdateMotionSensorData", vr_no_headset},
    {"9fvHMUbsom4#libSceVrTracker", vr_no_headset},
};

uintptr_t runtime_vr_resolve(const char *name) {
    return RUNTIME_LOOKUP(vr_exports, name);
}
