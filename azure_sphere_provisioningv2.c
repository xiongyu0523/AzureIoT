/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include <applibs/log.h>
#include <applibs/networking.h>
#include <applibs/application.h>

// Azure IoT SDK
#include <iothub_client_core_common.h>
#include <iothub_device_client_ll.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>
#include <iothub.h>
#include <iothub_security_factory.h>
#include <shared_util_options.h>
#include <prov_device_ll_client.h>
#include <prov_security_factory.h>
#include <prov_transport_mqtt_client.h>

#include "azure_sphere_provisioningv2.h"

#define CHINA_DPS_ENDPOINT          "global.azure-devices-provisioning.cn"
#define GLOBAL_DPS_ENDPOINT         "global.azure-devices-provisioning.net"
#define CHINA_DPS_IDSCOPE_PREFIX    "0cn"
#define GLOBAL_DPS_IDSCOPE_PREFIX   "0ne"
#define CHINA_IOTHUB_SUFFIX         ".cn"
#define GLOBAL_IOTHUB_SUFFIX        ".net"

#define PROV_CHECK_LOOP             100

typedef struct _dps_callback_context {
    bool registration_complete;
    bool is_china_iothub;
    char* iothub_uri;
    PROV_DEVICE_RESULT error_code;
} DpsCbContext;

static const char digiCertGlobalRootCA[] =
/* DigiCert Global Root CA */
"-----BEGIN CERTIFICATE-----\r\n"
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\r\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\r\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\r\n"
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\r\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\r\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\r\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\r\n"
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\r\n"
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\r\n"
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\r\n"
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\r\n"
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\r\n"
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\r\n"
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\r\n"
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\r\n"
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\r\n"
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\r\n"
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\r\n"
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\r\n"
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\r\n"
"-----END CERTIFICATE-----\r\n";

static void msleep(uint32_t ms);
static void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri,
    const char* not_used, void* user_context);
AZURE_SPHERE_PROV_RETURN_VALUE IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioningV2(
    const char* idScope, unsigned int timeout, IOTHUB_DEVICE_CLIENT_LL_HANDLE* handle);

static void msleep(uint32_t ms)
{
    struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000) };
    while ((-1 == nanosleep(&ts, &ts)) && (EINTR == errno));
}

static void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri,
    const char* not_used, void* user_context)
{
    DpsCbContext* pctx = (DpsCbContext*)user_context;
    if (register_result == PROV_DEVICE_RESULT_OK) {
        Log_Debug("INFO: Registration Information received from service: %s\n", iothub_uri);
        pctx->iothub_uri = malloc(strlen(iothub_uri) + 1);
        (void)strcpy(pctx->iothub_uri, iothub_uri);

        if (strstr(pctx->iothub_uri, CHINA_IOTHUB_SUFFIX) != NULL) {
            pctx->is_china_iothub = true;
        }

        pctx->error_code = register_result;
        pctx->registration_complete = true;
    } else {
        pctx->error_code = register_result;
        pctx->registration_complete = true;
    }
}

AZURE_SPHERE_PROV_RETURN_VALUE IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioningV2(
    const char* idScope, unsigned int timeout, IOTHUB_DEVICE_CLIENT_LL_HANDLE* handle)
{
    AZURE_SPHERE_PROV_RETURN_VALUE ret;
    const char* dps_endpoint;
    int device_id = 1;
    PROV_DEVICE_LL_HANDLE prov_handle;
    bool isChinaDps = false;

    if (strncmp(idScope, CHINA_DPS_IDSCOPE_PREFIX, strlen(CHINA_DPS_IDSCOPE_PREFIX)) == 0) {
        dps_endpoint = CHINA_DPS_ENDPOINT;
        isChinaDps = true;
    } else if (strncmp(idScope, GLOBAL_DPS_IDSCOPE_PREFIX, strlen(GLOBAL_DPS_IDSCOPE_PREFIX)) == 0) {
        dps_endpoint = GLOBAL_DPS_ENDPOINT;
    } else {
        ret.result = AZURE_SPHERE_PROV_RESULT_INVALID_PARAM;
        return ret;
    }

    if ((idScope == NULL) || (handle == NULL)) {
        ret.result = AZURE_SPHERE_PROV_RESULT_INVALID_PARAM;
        return ret;
    }

    bool is_network_ready = false;
    if (Networking_IsNetworkingReady(&is_network_ready) < 0) {
        Log_Debug("ERROR: Networking_IsNetworkingReady() failed\n");
        ret.result = AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR;
        return ret;
    }

    if (!is_network_ready) {
        Log_Debug("WARN: Network must be ready before provisioning\n");
        ret.result = AZURE_SPHERE_PROV_RESULT_NETWORK_NOT_READY;
        return ret;
    }

    bool is_daa_ready = false;
    if (Application_IsDeviceAuthReady(&is_daa_ready) < 0) {
        Log_Debug("ERROR: Application_IsDeviceAuthReady() failed\n");
        ret.result = AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR;
        return ret;
    }

    if (!is_daa_ready) {
        Log_Debug("WARN: DAA must be passed before provisioning\n");
        ret.result = AZURE_SPHERE_PROV_RESULT_DEVICEAUTH_NOT_READY;
        return ret;
    }

    if (prov_dev_security_init(SECURE_DEVICE_TYPE_X509) != 0) {
        Log_Debug("ERROR: prov_dev_security_init() failed\n");
        ret.result = AZURE_SPHERE_PROV_RESULT_GENERIC_ERROR;
        return ret;
    }

    DpsCbContext ctx;
    ctx.registration_complete = false;
    ctx.is_china_iothub = false;
    ctx.iothub_uri = NULL;
    ctx.error_code = PROV_DEVICE_RESULT_OK;

    if ((prov_handle = Prov_Device_LL_Create(dps_endpoint, idScope, Prov_Device_MQTT_Protocol)) == NULL) {
        Log_Debug("ERROR: Prov_Device_LL_Create() failed\n");

        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
        ret.prov_device_error = PROV_DEVICE_RESULT_ERROR;
        return ret;
    }

    if ((ret.prov_device_error = Prov_Device_LL_SetOption(prov_handle, "SetDeviceId", &device_id)) != PROV_DEVICE_RESULT_OK) {
        Log_Debug("ERROR: Prov_Device_LL_SetOption() failed\n");

        Prov_Device_LL_Destroy(prov_handle);
        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
        return ret;
    }

    if (isChinaDps) {
        if ((ret.prov_device_error = Prov_Device_LL_SetOption(prov_handle, OPTION_TRUSTED_CERT, digiCertGlobalRootCA)) != PROV_DEVICE_RESULT_OK) {
            Log_Debug("ERROR: Prov_Device_LL_SetOption() failed\n");

            Prov_Device_LL_Destroy(prov_handle);
            prov_dev_security_deinit();
            ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
            return ret;
        }
    }

    if ((ret.prov_device_error = Prov_Device_LL_Register_Device(prov_handle, register_device_callback, &ctx, NULL, NULL)) != PROV_DEVICE_RESULT_OK) {
        Log_Debug("ERROR: Prov_Device_LL_Register_Device() failed\n");

        Prov_Device_LL_Destroy(prov_handle);
        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
        return ret;
    }

    unsigned int num_of_loops = timeout / PROV_CHECK_LOOP;
    do {
        Prov_Device_LL_DoWork(prov_handle);
        msleep(PROV_CHECK_LOOP);
        --num_of_loops;
    } while ((!ctx.registration_complete) && (num_of_loops > 0));

    Prov_Device_LL_Destroy(prov_handle);

    if (!ctx.registration_complete) {
        Log_Debug("ERROR: Provisioning timeout\n");

        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
        ret.prov_device_error = PROV_DEVICE_RESULT_TIMEOUT;
        return ret;

    } else {
        if (ctx.error_code != PROV_DEVICE_RESULT_OK) {
            prov_dev_security_deinit();
            ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
            ret.prov_device_error = ctx.error_code;
            return ret;
        } 
    }

    *handle = IoTHubDeviceClient_LL_CreateWithAzureSphereFromDeviceAuth(ctx.iothub_uri, MQTT_Protocol);
    if (*handle == NULL) {
        Log_Debug("ERROR: IoTHubDeviceClient_LL_CreateWithAzureSphereFromDeviceAuth() failed\n");

        free(ctx.iothub_uri);
        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR;
        ret.prov_device_error = PROV_DEVICE_RESULT_DEV_AUTH_ERROR;
        return ret;
    }

    free(ctx.iothub_uri);

    if ((ret.iothub_client_error = IoTHubDeviceClient_LL_SetOption(*handle, "SetDeviceId", &device_id)) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: IoTHubDeviceClient_LL_SetOption() failed\n");

        prov_dev_security_deinit();
        ret.result = AZURE_SPHERE_PROV_RESULT_IOTHUB_CLIENT_ERROR;
        return ret;
    }

    if (ctx.is_china_iothub) {
        if ((ret.iothub_client_error = IoTHubDeviceClient_LL_SetOption(*handle, OPTION_TRUSTED_CERT, digiCertGlobalRootCA)) != IOTHUB_CLIENT_OK) {
            Log_Debug("ERROR: IoTHubDeviceClient_LL_SetOption() failed\n");

            prov_dev_security_deinit();
            ret.result = AZURE_SPHERE_PROV_RESULT_IOTHUB_CLIENT_ERROR;
            return ret;
        }
    }

    prov_dev_security_deinit();
    ret.result = AZURE_SPHERE_PROV_RESULT_OK;
    return ret;
}

