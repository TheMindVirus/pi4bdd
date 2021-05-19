#include "mbox.hxx"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS DriverEntry
(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    debug("[CALL]: DriverEntry");
    NTSTATUS status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG DriverConfig = { 0 };

    WDF_DRIVER_CONFIG_INIT(&DriverConfig, DriverDeviceAdd);
    DriverConfig.EvtDriverUnload = DriverUnload;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &DriverConfig, WDF_NO_HANDLE);
    if (NT_ERROR(status)) { debug("[WARN]: WdfDriverCreate Failed (0x%08lX)", status); return status; }
    return STATUS_SUCCESS;
}

NTSTATUS DriverDeviceAdd
(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    debug("[CALL]: DriverDeviceAdd");
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING symlink = { 0 };
    WDF_PNPPOWER_EVENT_CALLBACKS configPnP = { 0 };
    WDF_IO_QUEUE_CONFIG configIO = { 0 };
    WDFDEVICE device = { 0 };

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&configPnP);
    configPnP.EvtDevicePrepareHardware = DevicePrepareHardware;
    configPnP.EvtDeviceReleaseHardware = DeviceReleaseHardware;
    configPnP.EvtDeviceD0Entry = DevicePowerUp;
    configPnP.EvtDeviceD0Exit = DevicePowerDown;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &configPnP);

    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (NT_ERROR(status)) { debug("[WARN]: WdfDeviceCreate Failed (0x%08lX)", status); return status; }

    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(Driver);
}

void DriverUnload
(
    WDFDRIVER Driver
)
{
    debug("[CALL]: DriverUnload");
    UNREFERENCED_PARAMETER(Driver);
}

NTSTATUS DevicePrepareHardware
(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
)
{
    debug("[CALL]: DevicePrepareHardware");
    main();
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
}

NTSTATUS DeviceReleaseHardware
(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated
)
{
    debug("[CALL]: DeviceReleaseHardware");
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
}

NTSTATUS DevicePowerUp
(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
{
    debug("[CALL]: DevicePowerUp");
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);
}

NTSTATUS DevicePowerDown
(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
)
{
    debug("[CALL]: DevicePowerDown");
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);
}