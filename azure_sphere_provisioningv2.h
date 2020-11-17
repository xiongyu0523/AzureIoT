/// \file azure_sphere_provisioningv2.h
/// \brief This file contains functionality related to provisioning an
/// Azure Sphere device to an Azure IoT hub using an Azure Device Provisioning
/// Service instance, and the device's client auth certificate.

#pragma once
#include <azure_sphere_provisioning.h>

#ifdef __cplusplus
extern "C" {
#endif


/// <summary>
///     V2 of Provisions the Azure Sphere device using the provisioning service
///     specified by <paramref name="idScope"/> and creates an IoT hub
///     connection handle. Support both global dps endpoint and china dps endpoint
/// </summary>
/// <param name="idScope">
///     The Azure IoT Device Provisioning Service scope ID for this device. 
/// </param>
/// <param name="timeout">
///     Time to wait for provisioning (in milliseconds) before timing out.
///     In the event of a timeout, the result field of the return value will be set
///     to AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR, and the prov_device_error
///     will be PROV_DEVICE_RESULT_TIMEOUT.
/// </param>
/// <param name="handle">
///     (output) On success, the IoT Hub connection handle. On failure, this will be set to
///     NULL
/// </param>
/// <returns>
///     Returns an <see cref="AZURE_SPHERE_PROV_RETURN_VALUE" />. On success, result
///     will be AZURE_SPHERE_PROV_RESULT_OK.
///     On failure, result will be set to an error code indicating the reason for failure.
///     If result is AZURE_SPHERE_PROV_RESULT_PROV_DEVICE_ERROR, prov_device_error will be set to
///     a PROV_DEVICE_RESULT value indicating the reason.
///     If result is AZURE_SPHERE_PROV_RESULT_IOTHUB_CLIENT_ERROR, iothub_client_error will be
///     set to an IOTHUB_CLIENT_RESULT value indicating the reason.
/// </returns>
AZURE_SPHERE_PROV_RETURN_VALUE IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioningV2(
    const char *idScope, unsigned int timeout, IOTHUB_DEVICE_CLIENT_LL_HANDLE *handle);

#ifdef __cplusplus
}
#endif
