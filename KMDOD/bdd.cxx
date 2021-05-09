#include "BDD.hxx"

BASIC_DISPLAY_DRIVER::BASIC_DISPLAY_DRIVER(_In_ DEVICE_OBJECT* pPhysicalDeviceObject) : m_pPhysicalDevice(pPhysicalDeviceObject),
                                                                                        m_MonitorPowerState(PowerDeviceD0),
                                                                                        m_AdapterPowerState(PowerDeviceD0)
{
    debug("[CALL]: BASIC_DISPLAY_DRIVER::BASIC_DISPLAY_DRIVER");
    *((UINT*)&m_Flags) = 0;
    m_Flags._LastFlag = TRUE;
    RtlZeroMemory(&m_DxgkInterface, sizeof(m_DxgkInterface));
    RtlZeroMemory(&m_StartInfo, sizeof(m_StartInfo));
    RtlZeroMemory(m_CurrentModes, sizeof(m_CurrentModes));
    RtlZeroMemory(&m_DeviceInfo, sizeof(m_DeviceInfo));
    for (UINT i=0;i<MAX_VIEWS;i++)
    {
        m_HardwareBlt[i].Initialize(this,i);
    }
}

BASIC_DISPLAY_DRIVER::~BASIC_DISPLAY_DRIVER()
{
    debug("[CALL]: BASIC_DISPLAY_DRIVER::~BASIC_DISPLAY_DRIVER");
    CleanUp();
}

NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice
(
    _In_  DXGK_START_INFO*   pDxgkStartInfo,
    _In_  DXGKRNL_INTERFACE* pDxgkInterface,
    _Out_ ULONG*             pNumberOfViews,
    _Out_ ULONG*             pNumberOfChildren
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice");
    if (!(pDxgkStartInfo != NULL)) { debug("[ASRT]: (pDxgkStartInfo = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice"); return STATUS_ABANDONED; }
    if (!(pDxgkInterface != NULL)) { debug("[ASRT]: (pDxgkInterface = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice"); return STATUS_ABANDONED; }
    if (!(pNumberOfViews != NULL)) { debug("[ASRT]: (pNumberOfViews = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice"); return STATUS_ABANDONED; }
    if (!(pNumberOfChildren != NULL)) { debug("[ASRT]: (pNumberOfChildren = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice"); return STATUS_ABANDONED; }
    RtlCopyMemory(&m_StartInfo, pDxgkStartInfo, sizeof(m_StartInfo));
    RtlCopyMemory(&m_DxgkInterface, pDxgkInterface, sizeof(m_DxgkInterface));
    RtlZeroMemory(m_CurrentModes, sizeof(m_CurrentModes));
    m_CurrentModes[0].DispInfo.TargetId = D3DDDI_ID_UNINITIALIZED; // <-- ???

    // Get device information from OS.
    RegisterHWInfo();

    mbox_mmio_setup();
    dev_width2 =  m_CurrentModes[0].DispInfo.Width; //1920; // <-- TEMPORARY SOLUTION
    dev_height2 = m_CurrentModes[0].DispInfo.Height; //1080; // <-- TEMPORARY SOLUTION
    mbox_get_display_info();
    
    mbox_set_display(1);
    BOOLEAN b = (BOOLEAN)mbox_get_vsync();
    debug("[MBOX]: mbox_get_vsync() = %s", b ? "true" : "false");
    b = false;
    mbox_set_vsync(true);
    b = (BOOLEAN)mbox_get_vsync();
    debug("[MBOX]: mbox_set_vsync() = %s", b ? "true" : "false");
    mbox_set_display(0);

    debug("[MBOX]: dev_pitchspace2 = 0x%016llX", dev_pitchspace2);
    m_CurrentModes[0].DispInfo.PhysicAddress.QuadPart = dev_framebuffer2;
    m_CurrentModes[0].DispInfo.Pitch = dev_pitchspace2; // 0x1E00 reported by the mailbox may cause STATUS_GRAPHICS_INVALID_STRIDE (0xC01E033C)
    m_CurrentModes[0].DispInfo.ColorFormat = D3DDDIFMT_A8R8G8B8;

    // NTSTATUS Status = m_DxgkInterface.DxgkCbGetDeviceInformation(m_DxgkInterface.DeviceHandle, &m_DeviceInfo);
    // if (!NT_SUCCESS(Status))
    // {
    //     debug("[WARN]: DxgkCbGetDeviceInformation failed with status 0x%016llX", Status);
    //     return Status;
    // }
    // 
    // Ignore return value, since it's not the end of the world if we failed to write these values to the registry
    // RegisterHWInfo();
    // TODO: Uncomment the line below after updating the TODOs in the function CheckHardware
    // Status = CheckHardware();
    // if (!NT_SUCCESS(Status))
    // {
    //    debug("[WARN]: CheckHardware failed with status 0x%016llX", Status);
    //    return Status;
    // }
    // This sample driver only uses the frame buffer of the POST device. DxgkCbAcquirePostDisplayOwnership
    // gives you the frame buffer address and ensures that no one else is drawing to it. Be sure to give it back!
    ///!!!NOTE: The 0'th device is not a POST device, it is in Mode 5!!! This is an invalid sample driver!!! >:(

    //debug("[INFO]: HighestPhysicalAddress: 0x%016llX", m_DeviceInfo.HighestPhysicalAddress);
    //debug("[INFO]: AgpApertureBase: 0x%016llX", m_DeviceInfo.AgpApertureBase);
    //debug("[INFO]: AgpApertureSize: 0x%016llX", m_DeviceInfo.AgpApertureSize);
    
    // Status = m_DxgkInterface.DxgkCbAcquirePostDisplayOwnership(m_DxgkInterface.DeviceHandle, &(m_CurrentModes[0].DispInfo));
    // if (!NT_SUCCESS(Status) || m_CurrentModes[0].DispInfo.Width == 0)
    // {
    //     // The most likely cause of failure is that the driver is simply not running on a POST device, or we are running
    //     // after a pre-WDDM 1.2 driver. Since we can't draw anything, we should fail to start.
    //     debug("[WARN]: DxgkCbAcquirePostDisplayOwnership failed with status 0x%016llX (width = 0, not running on a BIOS POST device?)", Status);
    //     return STATUS_UNSUCCESSFUL;
    // }

    m_Flags.DriverStarted = TRUE;
    *pNumberOfViews = MAX_VIEWS;
    *pNumberOfChildren = MAX_CHILDREN;
   return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::StopDevice(VOID)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::StopDevice");
    CleanUp();
    mbox_mmio_cleanup();
    m_Flags.DriverStarted = FALSE;
    return STATUS_SUCCESS;
}

VOID BASIC_DISPLAY_DRIVER::CleanUp()
{
    debug("[CALL]: VOID BASIC_DISPLAY_DRIVER::CleanUp");
    for (UINT Source = 0; Source < MAX_VIEWS; ++Source)
    {
        if (m_CurrentModes[Source].FrameBuffer.Ptr)
        {
            UnmapFrameBuffer(m_CurrentModes[Source].FrameBuffer.Ptr, m_CurrentModes[Source].DispInfo.Height * m_CurrentModes[Source].DispInfo.Pitch);
            m_CurrentModes[Source].FrameBuffer.Ptr = NULL;
            m_CurrentModes[Source].Flags.FrameBufferIsActive = FALSE;
        }
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::DispatchIoRequest
(
    _In_  ULONG                 VidPnSourceId,
    _In_  VIDEO_REQUEST_PACKET* pVideoRequestPacket
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::DispatchIoRequest");
    if (!(pVideoRequestPacket != NULL)) { debug("[ASRT]: (pVideoRequestPacket = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::DispatchIoRequest"); return STATUS_ABANDONED; }
    if (!(VidPnSourceId < MAX_VIEWS)) { debug("[ASRT]: (VidPnSourceId >= MAX_VIEWS) | NTSTATUS BASIC_DISPLAY_DRIVER::DispatchIoRequest"); return STATUS_ABANDONED; }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState
(
    _In_  ULONG              HardwareUid,
    _In_  DEVICE_POWER_STATE DevicePowerState,
    _In_  POWER_ACTION       ActionType
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState");
    UNREFERENCED_PARAMETER(ActionType);
    if (!((HardwareUid < MAX_CHILDREN) || (HardwareUid == DISPLAY_ADAPTER_HW_ID)))
        { debug("[ASRT]: ((HardwareUid >= MAX_CHILDREN) || (HardwareUid != DISPLAY_ADAPTER_HW_ID)) | NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState"); return STATUS_ABANDONED; }
    if (HardwareUid == DISPLAY_ADAPTER_HW_ID)
    {
        if (DevicePowerState == PowerDeviceD0)
        {
            // When returning from D3 the device visibility defined to be off for all targets
            if (m_AdapterPowerState == PowerDeviceD3)
            {
                DXGKARG_SETVIDPNSOURCEVISIBILITY Visibility;
                Visibility.VidPnSourceId = D3DDDI_ID_ALL;
                Visibility.Visible = FALSE;
                SetVidPnSourceVisibility(&Visibility);
            }
        }
        // Store new adapter power state
        m_AdapterPowerState = DevicePowerState;
        // There is nothing to do to specifically power up/down the display adapter
        return STATUS_SUCCESS;
    }
    else
    {
        // TODO: This is where the specified monitor should be powered up/down
        NOTHING;
        return STATUS_SUCCESS;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildRelations
(
    _Out_writes_bytes_(ChildRelationsSize) DXGK_CHILD_DESCRIPTOR* pChildRelations,
    _In_                                   ULONG                  ChildRelationsSize
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildRelations");
    if (!(pChildRelations != NULL)) { debug("[ASRT]: (pChildRelations = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildRelations"); return STATUS_ABANDONED; }
    // The last DXGK_CHILD_DESCRIPTOR in the array of pChildRelations must remain zeroed out, so we subtract this from the count
    ULONG ChildRelationsCount = (ChildRelationsSize / sizeof(DXGK_CHILD_DESCRIPTOR)) - 1;
    if (!(ChildRelationsCount <= MAX_CHILDREN)) { debug("[ASRT]: (ChildRelationsCount <= MAX_CHILDREN) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildRelations"); return STATUS_ABANDONED; }
    for (UINT ChildIndex = 0; ChildIndex < ChildRelationsCount; ++ChildIndex)
    {
        pChildRelations[ChildIndex].ChildDeviceType = TypeVideoOutput;
        pChildRelations[ChildIndex].ChildCapabilities.HpdAwareness = HpdAwarenessInterruptible;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = m_CurrentModes[0].Flags.IsInternal ? D3DKMDT_VOT_INTERNAL : D3DKMDT_VOT_OTHER;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_NONE;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        // TODO: Replace 0 with the actual ACPI ID of the child device, if available
        pChildRelations[ChildIndex].AcpiUid = 0;
        pChildRelations[ChildIndex].ChildUid = ChildIndex;
    }
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildStatus
(
    _Inout_ DXGK_CHILD_STATUS* pChildStatus,
    _In_    BOOLEAN            NonDestructiveOnly
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildStatus");
    UNREFERENCED_PARAMETER(NonDestructiveOnly);
    if (!(pChildStatus != NULL)) { debug("[ASRT]: (pChildStatus != NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildStatus"); return STATUS_ABANDONED; }
    if (!(pChildStatus->ChildUid < MAX_CHILDREN)) { debug("[ASRT]: (pChildStatus->ChildUid < MAX_CHILDREN) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildStatus"); return STATUS_ABANDONED; }
    switch (pChildStatus->Type)
    {
        case StatusConnection:
        {
            // HpdAwarenessInterruptible was reported since HpdAwarenessNone is deprecated.
            // However, BDD has no knowledge of HotPlug events, so just always return connected.
            pChildStatus->HotPlug.Connected = IsDriverActive();
            return STATUS_SUCCESS;
        }
        case StatusRotation:
        {
            // D3DKMDT_MOA_NONE was reported, so this should never be called
            debug("[WARN]: Child status being queried for StatusRotation even though D3DKMDT_MOA_NONE was reported");
            return STATUS_INVALID_PARAMETER;
        }
        default:
        {
            debug("[WARN]: Unknown pChildStatus->Type (0x%016llX) requested.", pChildStatus->Type);
            return STATUS_NOT_SUPPORTED;
        }
    }
}

// EDID retrieval
NTSTATUS BASIC_DISPLAY_DRIVER::QueryDeviceDescriptor
(
    _In_    ULONG                   ChildUid,
    _Inout_ DXGK_DEVICE_DESCRIPTOR* pDeviceDescriptor
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::QueryDeviceDescriptor");
    if (!(pDeviceDescriptor != NULL)) { debug("[ASRT]: (pDeviceDescriptor = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryDeviceDescriptor"); return STATUS_ABANDONED; }
    if (!(ChildUid < MAX_CHILDREN)) { debug("[ASRT]: (ChildUid >= MAX_CHILDREN) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryDeviceDescriptor"); return STATUS_ABANDONED; }
    // If we haven't successfully retrieved an EDID yet (invalid ones are ok, so long as it was retrieved)
    if (!m_Flags.EDID_Attempted)
    {
        GetEdid(ChildUid);
    }
    if (!m_Flags.EDID_Retrieved || !m_Flags.EDID_ValidHeader || !m_Flags.EDID_ValidChecksum)
    {
        // Report no EDID if a valid one wasn't retrieved
        return STATUS_GRAPHICS_CHILD_DESCRIPTOR_NOT_SUPPORTED;
    }
    else if (pDeviceDescriptor->DescriptorOffset == 0)
    {
        // Only the base block is supported
        RtlCopyMemory(pDeviceDescriptor->DescriptorBuffer,
                      m_EDIDs[ChildUid],
                      min(pDeviceDescriptor->DescriptorLength, EDID_V1_BLOCK_SIZE));
        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryAdapterInfo(_In_ CONST DXGKARG_QUERYADAPTERINFO* pQueryAdapterInfo)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::QueryAdapterInfo");
    if (!(pQueryAdapterInfo != NULL)) { debug("[ASRT]: (pQueryAdapterInfo = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryAdapterInfo"); return STATUS_ABANDONED; }
    switch (pQueryAdapterInfo->Type)
    {
        case DXGKQAITYPE_WDDMDEVICECAPS:
        {
            DXGK_WDDMDEVICECAPS* pWDDMDeviceCaps = (DXGK_WDDMDEVICECAPS*)pQueryAdapterInfo->pOutputData;
            RtlZeroMemory(pWDDMDeviceCaps, sizeof(DXGK_WDDMDEVICECAPS));
            pWDDMDeviceCaps->WDDMVersion = DXGKDDI_WDDMv1_2; //more utter nonsense: https://docs.microsoft.com/en-us/windows-hardware/drivers/display/wddm-2-1-features#driver-versioning
            return STATUS_SUCCESS; //Please see StampInf Driver Version Information in Project Settings (9.21.0000.0000)
        }
        break;
        case DXGKQAITYPE_DISPLAYID_DESCRIPTOR:
        {
            //DXGK_QUERYINTEGRATEDDISPLAYOUT* pQueryIntegratedDisplayOut = (DXGK_QUERYINTEGRATEDDISPLAYOUT*)pQueryAdapterInfo->pOutputData;
            //RtlZeroMemory(pQueryIntegratedDisplayOut, sizeof(DXGK_QUERYINTEGRATEDDISPLAYOUT));
            debug("[WARN]: DXGKQAITYPE_DISPLAYID_DESCRIPTOR Not Supported", pQueryAdapterInfo->Type);
            //pQueryIntegratedDisplayOut->Colorimetry = DXGK_COLORIMETRY::;
            return STATUS_ABANDONED;
        }
        break;
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        case DXGKQAITYPE_DRIVERCAPS:
        {
            if (pQueryAdapterInfo->OutputDataSize < sizeof(DXGK_DRIVERCAPS))
            {
                debug("[WARN]: pQueryAdapterInfo->OutputDataSize (0x%016llX) is smaller than sizeof(DXGK_DRIVERCAPS) (0x%016llX)", pQueryAdapterInfo->OutputDataSize, sizeof(DXGK_DRIVERCAPS));
                return STATUS_BUFFER_TOO_SMALL;
            }
            DXGK_DRIVERCAPS* pDriverCaps = (DXGK_DRIVERCAPS*)pQueryAdapterInfo->pOutputData;
            // Nearly all fields must be initialized to zero, so zero out to start and then change those that are non-zero.
            // Fields are zero since BDD is Display-Only and therefore does not support any of the render related fields.
            // It also doesn't support hardware interrupts, gamma ramps, etc.
            RtlZeroMemory(pDriverCaps, sizeof(DXGK_DRIVERCAPS));
            pDriverCaps->WDDMVersion = DXGKDDI_WDDMv1_2;
            pDriverCaps->HighestAcceptableAddress.QuadPart = -1;
            pDriverCaps->SupportNonVGA = TRUE;
            pDriverCaps->SupportSmoothRotation = TRUE;
            return STATUS_SUCCESS;
        }
        case DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION:
        {
            DXGK_DISPLAY_DRIVERCAPS_EXTENSION* pDriverDisplayCaps;
            if (pQueryAdapterInfo->OutputDataSize < sizeof(*pDriverDisplayCaps))
            {
                debug("[WARN]: pQueryAdapterInfo->OutputDataSize (0x%016llX) is smaller than sizeof(DXGK_DISPLAY_DRIVERCAPS_EXTENSION) (0x%016llX)",
                               pQueryAdapterInfo->OutputDataSize,
                               sizeof(DXGK_DISPLAY_DRIVERCAPS_EXTENSION));
                return STATUS_INVALID_PARAMETER;
            }
            pDriverDisplayCaps = (DXGK_DISPLAY_DRIVERCAPS_EXTENSION*)pQueryAdapterInfo->pOutputData;
            // Reset all caps values
            RtlZeroMemory(pDriverDisplayCaps, pQueryAdapterInfo->OutputDataSize);
            // We claim to support virtual display mode.
            pDriverDisplayCaps->VirtualModeSupport = 1;
            return STATUS_SUCCESS;
        }
        default:
        {
            // BDD does not need to support any other adapter information types
            debug("[WARN]: Unknown QueryAdapterInfo Type (0x%016llX) requested", pQueryAdapterInfo->Type);
            return STATUS_NOT_SUPPORTED;
        }
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::CheckHardware()
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::CheckHardware");
    NTSTATUS Status;
    ULONG VendorID;
    ULONG DeviceID;
// TODO: If developing a driver for PCI based hardware, then use the second method to retrieve Vendor/Device IDs.
// If developing for non-PCI based hardware (i.e. ACPI based hardware), use the first method to retrieve the IDs.
#if 1 // ACPI-based device
    // Get the Vendor & Device IDs on non-PCI system
    ACPI_EVAL_INPUT_BUFFER_COMPLEX AcpiInputBuffer = {0};
    AcpiInputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
    AcpiInputBuffer.MethodNameAsUlong = ACPI_METHOD_HARDWARE_ID;
    AcpiInputBuffer.Size = 0;
    AcpiInputBuffer.ArgumentCount = 0;
    BYTE OutputBuffer[sizeof(ACPI_EVAL_OUTPUT_BUFFER) + 0x10];
    RtlZeroMemory(OutputBuffer, sizeof(OutputBuffer));
    ACPI_EVAL_OUTPUT_BUFFER* pAcpiOutputBuffer = reinterpret_cast<ACPI_EVAL_OUTPUT_BUFFER*>(&OutputBuffer);
    Status = m_DxgkInterface.DxgkCbEvalAcpiMethod(m_DxgkInterface.DeviceHandle,
                                                  DISPLAY_ADAPTER_HW_ID,
                                                  &AcpiInputBuffer,
                                                  sizeof(AcpiInputBuffer),
                                                  pAcpiOutputBuffer,
                                                  sizeof(OutputBuffer));
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkCbReadDeviceSpace failed to get hardware IDs with status 0x%016llX", Status);
        return Status;
    }
    VendorID = ((ULONG*)(pAcpiOutputBuffer->Argument[0].Data))[0];
    DeviceID = ((ULONG*)(pAcpiOutputBuffer->Argument[0].Data))[1];
#else // PCI-based device
    // Get the Vendor & Device IDs on PCI system
    PCI_COMMON_HEADER Header = {0};
    ULONG BytesRead;
    Status = m_DxgkInterface.DxgkCbReadDeviceSpace(m_DxgkInterface.DeviceHandle,
                                                   DXGK_WHICHSPACE_CONFIG,
                                                   &Header,
                                                   0,
                                                   sizeof(Header),
                                                   &BytesRead);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkCbReadDeviceSpace failed with status 0x%016llX", Status);
        return Status;
    }
    VendorID = Header.VendorID;
    DeviceID = Header.DeviceID;
#endif
    // TODO: Replace 0x1414 with your Vendor ID
    if (VendorID == 0x1414)
    {
        switch (DeviceID)
        {
            // TODO: Replace the case statements below with the Device IDs supported by this driver
            case 0x0000:
            case 0xFFFF: return STATUS_SUCCESS;
        }
    }
    return STATUS_GRAPHICS_DRIVER_MISMATCH;
}

// Even though Sample Basic Display Driver does not support hardware cursors, and reports such
// in QueryAdapterInfo. This function can still be called to set the pointer to not visible
NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerPosition(_In_ CONST DXGKARG_SETPOINTERPOSITION* pSetPointerPosition)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerPosition");
    if (!(pSetPointerPosition != NULL)) { debug("[ASRT]: (pSetPointerPosition = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerPosition"); return STATUS_ABANDONED; }
    if (!(pSetPointerPosition->VidPnSourceId < MAX_VIEWS)) { debug("[ASRT]: (pSetPointerPosition->VidPnSourceId >= MAX_VIEWS) | NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerPosition"); return STATUS_ABANDONED; }
    if (!(pSetPointerPosition->Flags.Visible)) { return STATUS_SUCCESS; }
    else
    {
        debug("[WARN]: SetPointerPosition should never be called to set the pointer to visible since BDD doesn't support hardware cursors.");
        return STATUS_UNSUCCESSFUL;
    }
}

// Basic Sample Display Driver does not support hardware cursors, and reports such
// in QueryAdapterInfo. Therefore this function should never be called.
NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerShape(_In_ CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerShape");
    if (!(pSetPointerShape != NULL)) { debug("[ASRT]: (pSetPointerShape = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerShape"); return STATUS_ABANDONED; }
    debug("[WARN]: SetPointerShape should never be called since BDD doesn't support hardware cursors.");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS BASIC_DISPLAY_DRIVER::PresentDisplayOnly(_In_ CONST DXGKARG_PRESENT_DISPLAYONLY* pPresentDisplayOnly)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::PresentDisplayOnly");
    if (!(pPresentDisplayOnly != NULL)) { debug("[ASRT]: (pPresentDisplayOnly = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::PresentDisplayOnly"); return STATUS_ABANDONED; }
    if (!(pPresentDisplayOnly->VidPnSourceId < MAX_VIEWS)) { debug("[ASRT]: (pPresentDisplayOnly->VidPnSourceId >= MAX_VIEWS) | NTSTATUS BASIC_DISPLAY_DRIVER::PresentDisplayOnly"); return STATUS_ABANDONED; }
    if (pPresentDisplayOnly->BytesPerPixel < MIN_BYTES_PER_PIXEL_REPORTED)
    {
        // Only >=32bpp modes are reported, therefore this Present should never pass anything less than 4 bytes per pixel
        debug("[WARN]: pPresentDisplayOnly->BytesPerPixel is 0x%016llX, which is lower than the allowed.", pPresentDisplayOnly->BytesPerPixel);
        return STATUS_INVALID_PARAMETER;
    }
    // If it is in monitor off state or source is not supposed to be visible, don't present anything to the screen
    if ((m_MonitorPowerState > PowerDeviceD0) ||
        (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Flags.SourceNotVisible))
    {
        return STATUS_SUCCESS;
    }
    // Present is only valid if the target is actively connected to this source
    if (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Flags.FrameBufferIsActive)
    {

        // If actual pixels are coming through, will need to completely zero out physical address next time in BlackOutScreen
        m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].ZeroedOutStart.QuadPart = 0;
        m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].ZeroedOutEnd.QuadPart = 0;

        D3DKMDT_VIDPN_PRESENT_PATH_ROTATION RotationNeededByFb = pPresentDisplayOnly->Flags.Rotate ?
                                                                 m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Rotation :
                                                                 D3DKMDT_VPPR_IDENTITY;
            BYTE* pDst = (BYTE*)m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].FrameBuffer.Ptr;
            UINT DstBitPerPixel = BPPFromPixelFormat(m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].DispInfo.ColorFormat);
            if (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Scaling == D3DKMDT_VPPS_CENTERED)
            {
                UINT CenterShift = (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].DispInfo.Height -
                    m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].SrcModeHeight)*m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].DispInfo.Pitch;
                CenterShift += (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].DispInfo.Width -
                    m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].SrcModeWidth)*DstBitPerPixel/8;
                pDst += (int)CenterShift/2;
            }
            return m_HardwareBlt[pPresentDisplayOnly->VidPnSourceId].ExecutePresentDisplayOnly(pDst,
                                                                    DstBitPerPixel,
                                                                    (BYTE*)pPresentDisplayOnly->pSource,
                                                                    pPresentDisplayOnly->BytesPerPixel,
                                                                    pPresentDisplayOnly->Pitch,
                                                                    pPresentDisplayOnly->NumMoves,
                                                                    pPresentDisplayOnly->pMoves,
                                                                    pPresentDisplayOnly->NumDirtyRects,
                                                                    pPresentDisplayOnly->pDirtyRect,
                                                                    RotationNeededByFb);
    }
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::StopDeviceAndReleasePostDisplayOwnership
(
    _In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _Out_ DXGK_DISPLAY_INFORMATION*      pDisplayInfo
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::StopDeviceAndReleasePostDisplayOwnership");
    if (!(TargetId < MAX_CHILDREN)) { debug("[ASRT]: (TargetId >= MAX_CHILDREN) | NTSTATUS BASIC_DISPLAY_DRIVER::StopDeviceAndReleasePostDisplayOwnership"); pDisplayInfo = NULL; return STATUS_ABANDONED; }
    D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId = FindSourceForTarget(TargetId, TRUE);
    // In case BDD is the next driver to run, the monitor should not be off, since
    // this could cause the BIOS to hang when the EDID is retrieved on Start.
    if (m_MonitorPowerState > PowerDeviceD0)
    {
        SetPowerState(TargetId, PowerDeviceD0, PowerActionNone);
    }
    // The driver has to black out the display and ensure it is visible when releasing ownership
    BlackOutScreen(SourceId);
    *pDisplayInfo = m_CurrentModes[SourceId].DispInfo;
    return StopDevice();
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability(_Inout_ DXGKARG_QUERYVIDPNHWCAPABILITY* pVidPnHWCaps)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability");
    if (!(pVidPnHWCaps != NULL)) { debug("[ASRT]: (pVidPnHWCaps = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability"); return STATUS_ABANDONED; }
    if (!(pVidPnHWCaps->SourceId < MAX_VIEWS)) { debug("[ASRT]: (pVidPnHWCaps->SourceId >= MAX_VIEWS) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability"); return STATUS_ABANDONED; }
    if (!(pVidPnHWCaps->TargetId < MAX_CHILDREN)) { debug("[ASRT]: (pVidPnHWCaps->TargetId >= MAX_CHILDREN) | NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability"); return STATUS_ABANDONED; }
    pVidPnHWCaps->VidPnHWCaps.DriverRotation             = 1; // BDD does rotation in software
    pVidPnHWCaps->VidPnHWCaps.DriverScaling              = 0; // BDD does not support scaling
    pVidPnHWCaps->VidPnHWCaps.DriverCloning              = 0; // BDD does not support clone
    pVidPnHWCaps->VidPnHWCaps.DriverColorConvert         = 1; // BDD does color conversions in software
    pVidPnHWCaps->VidPnHWCaps.DriverLinkedAdapaterOutput = 0; // BDD does not support linked adapters
    pVidPnHWCaps->VidPnHWCaps.DriverRemoteDisplay        = 0; // BDD does not support remote displays
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::GetEdid(D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::GetEdid");
    if (!(!m_Flags.EDID_Attempted)) { debug("[ASRT]: (m_Flags.EDID_Attempted) | NTSTATUS BASIC_DISPLAY_DRIVER::GetEdid"); return STATUS_ABANDONED; }
    NTSTATUS Status = STATUS_SUCCESS;
    RtlZeroMemory(m_EDIDs[TargetId], sizeof(m_EDIDs[TargetId]));
    m_Flags.EDID_Attempted = TRUE;
    return Status;
}

VOID BASIC_DISPLAY_DRIVER::BlackOutScreen(D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId)
{
    debug("[CALL]: VOID BASIC_DISPLAY_DRIVER::BlackOutScreen");
    UINT ScreenHeight = m_CurrentModes[SourceId].DispInfo.Height;
    UINT ScreenPitch = m_CurrentModes[SourceId].DispInfo.Pitch;
    PHYSICAL_ADDRESS NewPhysAddrStart = m_CurrentModes[SourceId].DispInfo.PhysicAddress;
    PHYSICAL_ADDRESS NewPhysAddrEnd;
    NewPhysAddrEnd.QuadPart = NewPhysAddrStart.QuadPart + ((LONGLONG)ScreenHeight * ScreenPitch);
    if (m_CurrentModes[SourceId].Flags.FrameBufferIsActive)
    {
        BYTE* MappedAddr = reinterpret_cast<BYTE*>(m_CurrentModes[SourceId].FrameBuffer.Ptr);
        // Zero any memory at the start that hasn't been zeroed recently
        if (NewPhysAddrStart.QuadPart < m_CurrentModes[SourceId].ZeroedOutStart.QuadPart)
        {
            if (NewPhysAddrEnd.QuadPart < m_CurrentModes[SourceId].ZeroedOutStart.QuadPart)
            {
                // No overlap
                RtlZeroMemory(MappedAddr, ((LONGLONG)ScreenHeight * ScreenPitch));
            }
            else
            {
                RtlZeroMemory(MappedAddr, (UINT)(m_CurrentModes[SourceId].ZeroedOutStart.QuadPart - NewPhysAddrStart.QuadPart));
            }
        }
        // Zero any memory at the end that hasn't been zeroed recently
        if (NewPhysAddrEnd.QuadPart > m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart)
        {
            if (NewPhysAddrStart.QuadPart > m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart)
            {
                // No overlap
                // NOTE: When actual pixels were the most recent thing drawn, ZeroedOutStart & ZeroedOutEnd will both be 0
                // and this is the path that will be used to black out the current screen.
                RtlZeroMemory(MappedAddr, ((LONGLONG)ScreenHeight * ScreenPitch));
            }
            else
            {
                RtlZeroMemory(MappedAddr, (UINT)(NewPhysAddrEnd.QuadPart - m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart));
            }
        }
    }
    m_CurrentModes[SourceId].ZeroedOutStart.QuadPart = NewPhysAddrStart.QuadPart;
    m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart = NewPhysAddrEnd.QuadPart;
}

NTSTATUS BASIC_DISPLAY_DRIVER::WriteHWInfoStr(_In_ HANDLE DevInstRegKeyHandle, _In_ PCWSTR pszwValueName, _In_ PCSTR pszValue)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::WriteHWInfoStr");
    NTSTATUS Status;
    ANSI_STRING AnsiStrValue;
    UNICODE_STRING UnicodeStrValue;
    UNICODE_STRING UnicodeStrValueName;
    // ZwSetValueKey wants the ValueName as a UNICODE_STRING
    RtlInitUnicodeString(&UnicodeStrValueName, pszwValueName);
    // REG_SZ is for WCHARs, there is no equivalent for CHARs
    // Use the ansi/unicode conversion functions to get from PSTR to PWSTR
    RtlInitAnsiString(&AnsiStrValue, pszValue);
    Status = RtlAnsiStringToUnicodeString(&UnicodeStrValue, &AnsiStrValue, TRUE);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: RtlAnsiStringToUnicodeString failed with Status: 0x%016llX", Status);
        return Status;
    }
    // Write the value to the registry
    Status = ZwSetValueKey(DevInstRegKeyHandle,
                           &UnicodeStrValueName,
                           0,
                           REG_SZ,
                           UnicodeStrValue.Buffer,
                           UnicodeStrValue.MaximumLength);
    // Free the earlier allocated unicode string
    RtlFreeUnicodeString(&UnicodeStrValue);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: ZwSetValueKey failed with Status: 0x%016llX", Status);
    }
    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::RegisterHWInfo()
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::RegisterHWInfo");
    NTSTATUS Status;
    // TODO: Replace these strings with proper information
    PCSTR StrHWInfoChipType = "Broadcom VideoCore IV";
    PCSTR StrHWInfoDacType = "PixelValve (CRTC)";
    PCSTR StrHWInfoAdapterString = "Raspberry Pi 4 Basic Display Driver";
    PCSTR StrHWInfoBiosString = "Raspberry Pi 4 Basic Display Driver";
    HANDLE DevInstRegKeyHandle;
    Status = IoOpenDeviceRegistryKey(m_pPhysicalDevice, PLUGPLAY_REGKEY_DRIVER, KEY_SET_VALUE, &DevInstRegKeyHandle);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: IoOpenDeviceRegistryKey failed for PDO: 0x%016llX, Status: 0x%016llX", m_pPhysicalDevice, Status);
        return Status;
    }
    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.ChipType", StrHWInfoChipType);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: WriteHWInfoStr for StrHWInfoChipType failed with Status: 0x%016llX", Status);
        return Status;
    }
    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.DacType", StrHWInfoDacType);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: WriteHWInfoStr for StrHWInfoDacType failed with Status: 0x%016llX", Status);
        return Status;
    }
    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.AdapterString", StrHWInfoAdapterString);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: WriteHWInfoStr for StrHWInfoAdapterString failed with Status: 0x%016llX", Status);
        return Status;
    }
    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.BiosString", StrHWInfoBiosString);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: WriteHWInfoStr for StrHWInfoBiosString failed with Status: 0x%016llX", Status);
        return Status;
    }
    // MemorySize is a ULONG, unlike the others which are all strings
    UNICODE_STRING ValueNameMemorySize;
    RtlInitUnicodeString(&ValueNameMemorySize, L"HardwareInformation.MemorySize");
    DWORD MemorySize = 0; // BDD has no access to video memory
    Status = ZwSetValueKey(DevInstRegKeyHandle,
                           &ValueNameMemorySize,
                           0,
                           REG_DWORD,
                           &MemorySize,
                           sizeof(MemorySize));
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: ZwSetValueKey for MemorySize failed with Status: 0x%016llX", Status);
        return Status;
    }
    debug("[INFO]: Current Status: 0x%016llX", Status);
    return Status;
}

D3DDDI_VIDEO_PRESENT_SOURCE_ID BASIC_DISPLAY_DRIVER::FindSourceForTarget(D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId, BOOLEAN DefaultToZero)
{
    debug("[CALL]: D3DDDI_VIDEO_PRESENT_SOURCE_ID BASIC_DISPLAY_DRIVER::FindSourceForTarget");
    UNREFERENCED_PARAMETER(TargetId);
    if (!(TargetId < MAX_CHILDREN)) { debug("[ASRT]: (TargetId >= MAX_CHILDREN) | D3DDDI_VIDEO_PRESENT_SOURCE_ID BASIC_DISPLAY_DRIVER::FindSourceForTarget"); return D3DDDI_ID_UNINITIALIZED; }
    for (UINT SourceId = 0; SourceId < MAX_VIEWS; ++SourceId)
    {
        if (m_CurrentModes[SourceId].FrameBuffer.Ptr != NULL) { return SourceId; }
    }
    return DefaultToZero ? 0 : D3DDDI_ID_UNINITIALIZED;
}

VOID BASIC_DISPLAY_DRIVER::DpcRoutine(VOID)
{
    debug("[CALL]: VOID BASIC_DISPLAY_DRIVER::DpcRoutine");
    m_DxgkInterface.DxgkCbNotifyDpc((HANDLE)m_DxgkInterface.DeviceHandle);
}

BOOLEAN BASIC_DISPLAY_DRIVER::InterruptRoutine(_In_  ULONG MessageNumber)
{
    //debug("[CALL]: BOOLEAN BASIC_DISPLAY_DRIVER::InterruptRoutine");
    UNREFERENCED_PARAMETER(MessageNumber);
    // BDD cannot handle interrupts
    return TRUE; //FALSE; ///!!!ReportPresentProgress Bug
}

VOID BASIC_DISPLAY_DRIVER::ResetDevice(VOID) { debug("[CALL]: VOID BASIC_DISPLAY_DRIVER::ResetDevice"); }

// Must be Non-Paged, as it sets up the display for a bugcheck
NTSTATUS BASIC_DISPLAY_DRIVER::SystemDisplayEnable
(
    _In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _In_  PDXGKARG_SYSTEM_DISPLAY_ENABLE_FLAGS Flags,
    _Out_ UINT* pWidth,
    _Out_ UINT* pHeight,
    _Out_ D3DDDIFORMAT* pColorFormat
)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SystemDisplayEnable");
    UNREFERENCED_PARAMETER(Flags);
    m_SystemDisplaySourceId = D3DDDI_ID_UNINITIALIZED;
    if (!((TargetId < MAX_CHILDREN) || (TargetId == D3DDDI_ID_UNINITIALIZED)))
    {
        debug("[ASRT]: ((TargetId >= MAX_CHILDREN) || (TargetId != D3DDDI_ID_UNINITIALIZED)) | NTSTATUS BASIC_DISPLAY_DRIVER::SystemDisplayEnable");
        pWidth = NULL; pHeight = NULL; pColorFormat = NULL;
        return STATUS_ABANDONED;
    }
    // Find the frame buffer for displaying the bugcheck, if it was successfully mapped
    if (TargetId == D3DDDI_ID_UNINITIALIZED)
    {
        for (UINT SourceIdx = 0; SourceIdx < MAX_VIEWS; ++SourceIdx)
        {
            if (m_CurrentModes[SourceIdx].FrameBuffer.Ptr != NULL)
            {
                m_SystemDisplaySourceId = SourceIdx;
                break;
            }
        }
    }
    else
    {
        m_SystemDisplaySourceId = FindSourceForTarget(TargetId, FALSE);
    }
    if (m_SystemDisplaySourceId == D3DDDI_ID_UNINITIALIZED)
    {
        return STATUS_UNSUCCESSFUL;
    }
    if ((m_CurrentModes[m_SystemDisplaySourceId].Rotation == D3DKMDT_VPPR_ROTATE90)
    ||  (m_CurrentModes[m_SystemDisplaySourceId].Rotation == D3DKMDT_VPPR_ROTATE270))
    {
        *pHeight = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
        *pWidth = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;
    }
    else
    {
        *pWidth = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
        *pHeight = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;
    }
    *pColorFormat = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.ColorFormat;
    return STATUS_SUCCESS;
}

// Must be Non-Paged, as it is called to display the bugcheck screen
VOID BASIC_DISPLAY_DRIVER::SystemDisplayWrite
(
    _In_reads_bytes_(SourceHeight * SourceStride) VOID* pSource,
    _In_ UINT SourceWidth,
    _In_ UINT SourceHeight,
    _In_ UINT SourceStride,
    _In_ INT PositionX,
    _In_ INT PositionY
)
{
    debug("[CALL]: VOID BASIC_DISPLAY_DRIVER::SystemDisplayWrite");
    // Rect will be Offset by PositionX/Y in the src to reset it back to 0
    RECT Rect;
    Rect.left = PositionX;
    Rect.top = PositionY;
    Rect.right =  Rect.left + SourceWidth;
    Rect.bottom = Rect.top + SourceHeight;

    // Set up destination blt info
    BLT_INFO DstBltInfo;
    DstBltInfo.pBits = m_CurrentModes[m_SystemDisplaySourceId].FrameBuffer.Ptr;
    DstBltInfo.Pitch = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Pitch;
    DstBltInfo.BitsPerPel = BPPFromPixelFormat(m_CurrentModes[m_SystemDisplaySourceId].DispInfo.ColorFormat);
    DstBltInfo.Offset.x = 0;
    DstBltInfo.Offset.y = 0;
    DstBltInfo.Rotation = m_CurrentModes[m_SystemDisplaySourceId].Rotation;
    DstBltInfo.Width = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
    DstBltInfo.Height = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;

    // Set up source blt info
    BLT_INFO SrcBltInfo;
    SrcBltInfo.pBits = pSource;
    SrcBltInfo.Pitch = SourceStride;
    SrcBltInfo.BitsPerPel = 32;

    SrcBltInfo.Offset.x = -PositionX;
    SrcBltInfo.Offset.y = -PositionY;
    SrcBltInfo.Rotation = D3DKMDT_VPPR_IDENTITY;
    SrcBltInfo.Width = SourceWidth;
    SrcBltInfo.Height = SourceHeight;

    BltBits(&DstBltInfo, &SrcBltInfo, 1, &Rect); //1 is NumRects
}