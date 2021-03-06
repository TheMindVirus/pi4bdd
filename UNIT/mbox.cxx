#include "mbox.hxx"

//==================================================================

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    debug("[CALL]: DriverEntry");
    NTSTATUS status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG DriverConfig;

    WDF_DRIVER_CONFIG_INIT(&DriverConfig, DriverDeviceAdd);
    DriverConfig.EvtDriverUnload = DriverUnload;
    
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &DriverConfig, WDF_NO_HANDLE);
    if (NT_ERROR(status)) { debug("[WARN]: WdfDriverCreate: 0x%016llX", status); }
    return status;
}

NTSTATUS DriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    debug("[CALL]: DriverDeviceAdd");
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnPevents;
    WDFDEVICE Device;

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnPevents);
    PnPevents.EvtDevicePrepareHardware = PrepareHardware;
    PnPevents.EvtDeviceReleaseHardware = ReleaseHardware;
    PnPevents.EvtDeviceD0Entry = PowerUp;
    PnPevents.EvtDeviceD0Exit = PowerDown;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnPevents);

    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &Device);
    if (NT_ERROR(status)) { debug("[WARN]: WdfDeviceCreate: 0x%016llX", status); }
    return status;
}

void DriverUnload(WDFDRIVER Driver)
{
    debug("[CALL]: DriverUnload");
    UNREFERENCED_PARAMETER(Driver);
}

NTSTATUS PrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
{
    debug("[CALL]: PrepareHardware");
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    mbox_mmio_setup();
    mbox_get_display_info();
    return STATUS_SUCCESS;
}

NTSTATUS ReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
    debug("[CALL]: ReleaseHardware");
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    mbox_mmio_cleanup();
    return STATUS_SUCCESS;
}

NTSTATUS PowerUp(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    debug("[CALL]: PowerUp");
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);
    return STATUS_SUCCESS;
}

NTSTATUS PowerDown(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState)
{
    debug("[CALL]: PowerDown");
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);
    return STATUS_SUCCESS;
}

//==================================================================

void mbox_mmio_setup()
{
    debug("[CALL]: mbox_mmio_setup");
    PHYSICAL_ADDRESS base;
    PHYSICAL_ADDRESS max;

    base.QuadPart = MBOX_BASE;
    mbox_base = (PULONG)MmMapIoSpace(base, MBOX_SIZE, MmNonCached);
    if (!mbox_base) { debug("[WARN]: Failed to Memory-Map Mailbox"); }
    
    max.QuadPart = PERI_MASK;
    mbox_packet = (PULONG)MmAllocateContiguousMemory(MAX_SIZE, max);
    if (!mbox_packet) { debug("[WARN]: Failed to Memory-Map Mailbox Packet"); }
}

void mbox_mmio_cleanup()
{
    debug("[CALL]: mbox_mmio_cleanup");
    if (mbox_base)
    {
        MmUnmapIoSpace(mbox_base, MBOX_SIZE);
        mbox_base = NULL;
    }
    if (mbox_packet)
    {
        MmFreeContiguousMemory(mbox_packet);
        mbox_packet = NULL;
    }
}

//==================================================================

VALUE mbox_setup(BYTE channel)
{
    debug("[CALL]: mbox_setup");
    if ((!mbox_base) || (!mbox_packet)) { debug("[MBOX]: Error 1"); return MBOX_FAILURE; }

    VALUE checked = 0;
    VALUE mail = ((((ADDRESS)MmGetPhysicalAddress(mbox_packet).QuadPart) &~ 0xF) | (channel & 0xF)); //0xF reserved for 4-bit channel //<--Needs to be a physical address so the VC knows where it is
    debug("[MBOX]: physical = 0x%016llX", MmGetPhysicalAddress(mbox_packet).QuadPart);
    debug("[MBOX]: mail = 0x%016llX", mail);

    KSPIN_LOCK lock = NULL;
    KIRQL prevState = NULL;
    KeInitializeSpinLock(&lock);
    KeAcquireSpinLock(&lock, &prevState);

    mmio_write(mbox_base, MBOX_READ,   0x00000000);
    mmio_write(mbox_base, 1,           0x00000000);
    mmio_write(mbox_base, 2,           0x00000000);
    mmio_write(mbox_base, 3,           0x00000000);
    mmio_write(mbox_base, MBOX_POLL,   0x00000000);
    mmio_write(mbox_base, MBOX_SENDER, 0x00000000);
    //mmio_write(mbox_base, MBOX_STATUS, 0x00000000);
    //mmio_write(mbox_base, MBOX_CONFIG, 0x00000400); // <-- may be all that's required
    //mmio_write(mbox_base, MBOX_WRITE,  0x00000000); // <-- may be all that's required
    KeStallExecutionProcessor(100000);

    for (size_t i = 0; i < (MBOX_SIZE / 4); ++i)
    {
        debug("[MBOX]: mbox_base[%d] | Address: 0x%016llX | Value: 0x%016llX",
            i, MmGetPhysicalAddress(mbox_base + i), mmio_read(mbox_base, i));
    }

    VALUE timeout = 0;
    while ((mbox_peek() & MBOX_FULL) != 0)
    {
        KeStallExecutionProcessor(100000);
        ++timeout;
        if (timeout > 100) { debug("[MBOX]: Error 2"); goto failure; }
    }
    mbox_write(mail);

    VALUE timeout1 = 0;
    VALUE timeout2 = 0;
    while (1)
    {
        while ((mbox_peek() & MBOX_EMPTY) != 0)
        {
            KeStallExecutionProcessor(100000);
            ++timeout2;
            if (timeout2 > 100) { debug("[MBOX]: Error 3"); goto failure; }
        }

        checked = mbox_read();
        if (mail == checked) { goto success; }

        KeStallExecutionProcessor(100000);
        ++timeout1;
        if (timeout1 > 100) { debug("[MBOX]: Error 4"); goto failure; }
    }

success:
    KeReleaseSpinLock(&lock, prevState);
    return MBOX_SUCCESS;
failure:
    KeReleaseSpinLock(&lock, prevState);
    return MBOX_FAILURE;
}

void mbox_set_display(VALUE display)
{
    debug("[CALL]: mbox_set_display");
    VALUE i = 1;

    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x00048013;   //MBOX_TAG_SET_DISPLAY_NUM
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
    mbox_packet[i++] = display;      //Display Number

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS != mbox_setup(8)) { debug("[WARN]: Mailbox Transaction Error"); }
}

void mbox_get_display_info()
{
    debug("[CALL]: mbox_get_display_info");
    VALUE i = 1;
    VALUE a = 0;
    VALUE b = 0;
    VALUE c = 0;
    VALUE d = 0;

    mbox_set_display(0);
    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request Header

    mbox_packet[i++] = 0x00048003;   //MBOX_TAG_SET_FB_PGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = 1920;         //Width
    mbox_packet[i++] = 1080;         //Height

    mbox_packet[i++] = 0x00048004;   //MBOX_TAG_SET_FB_VGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = 1920;         //Width
    mbox_packet[i++] = 1080;         //Height

    mbox_packet[i++] = 0x00040001;   //MBOX_TAG_ALLOC_FB
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
a=i;mbox_packet[i++] = 32;           //Alignment
    mbox_packet[i++] = 0;            //Size

    mbox_packet[i++] = 0x00040008;   //MBOX_TAG_GET_FB_LINELENGTH
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
b=i;mbox_packet[i++] = 0;            //Pitch

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS == mbox_setup(8))
    {
        dev_framebuffer1 = mbox_packet[a] & 0x3FFFFFFF; //Translate from VC to ARM address
        dev_pitchspace1 = mbox_packet[b];
        debug("[MBOX]: dev_framebuffer1 = 0x%016llX", dev_framebuffer1);
        debug("[MBOX]: dev_pitchspace1 = 0x%016llX", dev_pitchspace1);
    }
    else { debug("[WARN]: Mailbox Transaction Error"); }

    i = 1;
    mbox_set_display(1);
    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request Header

    mbox_packet[i++] = 0x00048003;   //MBOX_TAG_SET_FB_PGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = 1920;         //Width
    mbox_packet[i++] = 1080;         //Height

    mbox_packet[i++] = 0x00048004;   //MBOX_TAG_SET_FB_VGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = 1920;         //Width
    mbox_packet[i++] = 1080;         //Height

    mbox_packet[i++] = 0x00040001;   //MBOX_TAG_ALLOC_FB
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
c=i;mbox_packet[i++] = 32;           //Alignment
    mbox_packet[i++] = 0;            //Size

    mbox_packet[i++] = 0x00040008;   //MBOX_TAG_GET_FB_LINELENGTH
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
d=i;mbox_packet[i++] = 0;            //Pitch

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS == mbox_setup(8))
    {
        dev_framebuffer2 = mbox_packet[c] & 0x3FFFFFFF; //Translate from VC to ARM address
        dev_pitchspace2 = mbox_packet[d];
        debug("[MBOX]: dev_framebuffer2 = 0x%016llX", dev_framebuffer2);
        debug("[MBOX]: dev_pitchspace2 = 0x%016llX", dev_pitchspace2);
    }
    else { debug("[WARN]: Mailbox Transaction Error"); }

    for (size_t j = 0; j < i; ++j) { debug("[MBOX]: mbox_packet[%d] = 0x%016llX", j, mbox_packet[j]); }
}

//==================================================================