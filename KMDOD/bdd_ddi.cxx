#include "BDD.hxx"

extern "C" NTSTATUS DriverEntry
(
    _In_  DRIVER_OBJECT*  pDriverObject,
    _In_  UNICODE_STRING* pRegistryPath
)
{
    debug("[CALL]: extern \"C\" NTSTATUS DriverEntry");
    // Initialize DDI function pointers and dxgkrnl
    KMDDOD_INITIALIZATION_DATA InitialData = {0};

    InitialData.Version = DXGKDDI_INTERFACE_VERSION;

    InitialData.DxgkDdiAddDevice                    = BddDdiAddDevice;
    InitialData.DxgkDdiStartDevice                  = BddDdiStartDevice;
    InitialData.DxgkDdiStopDevice                   = BddDdiStopDevice;
    InitialData.DxgkDdiResetDevice                  = BddDdiResetDevice;
    InitialData.DxgkDdiRemoveDevice                 = BddDdiRemoveDevice;
    InitialData.DxgkDdiDispatchIoRequest            = BddDdiDispatchIoRequest;
    InitialData.DxgkDdiInterruptRoutine             = BddDdiInterruptRoutine;
    InitialData.DxgkDdiDpcRoutine                   = BddDdiDpcRoutine;
    InitialData.DxgkDdiQueryChildRelations          = BddDdiQueryChildRelations;
    InitialData.DxgkDdiQueryChildStatus             = BddDdiQueryChildStatus;
    InitialData.DxgkDdiQueryDeviceDescriptor        = BddDdiQueryDeviceDescriptor;
    InitialData.DxgkDdiSetPowerState                = BddDdiSetPowerState;
    InitialData.DxgkDdiUnload                       = BddDdiUnload;
    InitialData.DxgkDdiQueryAdapterInfo             = BddDdiQueryAdapterInfo;
    InitialData.DxgkDdiSetPointerPosition           = BddDdiSetPointerPosition;
    InitialData.DxgkDdiSetPointerShape              = BddDdiSetPointerShape;
    InitialData.DxgkDdiIsSupportedVidPn             = BddDdiIsSupportedVidPn;
    InitialData.DxgkDdiRecommendFunctionalVidPn     = BddDdiRecommendFunctionalVidPn;
    InitialData.DxgkDdiEnumVidPnCofuncModality      = BddDdiEnumVidPnCofuncModality;
    InitialData.DxgkDdiSetVidPnSourceVisibility     = BddDdiSetVidPnSourceVisibility;
    InitialData.DxgkDdiCommitVidPn                  = BddDdiCommitVidPn;
    InitialData.DxgkDdiUpdateActiveVidPnPresentPath = BddDdiUpdateActiveVidPnPresentPath;
    InitialData.DxgkDdiRecommendMonitorModes        = BddDdiRecommendMonitorModes;
    InitialData.DxgkDdiQueryVidPnHWCapability       = BddDdiQueryVidPnHWCapability;
    InitialData.DxgkDdiPresentDisplayOnly           = BddDdiPresentDisplayOnly;
    InitialData.DxgkDdiStopDeviceAndReleasePostDisplayOwnership = BddDdiStopDeviceAndReleasePostDisplayOwnership;
    InitialData.DxgkDdiSystemDisplayEnable          = BddDdiSystemDisplayEnable;
    InitialData.DxgkDdiSystemDisplayWrite           = BddDdiSystemDisplayWrite;

    NTSTATUS Status = DxgkInitializeDisplayOnlyDriver(pDriverObject, pRegistryPath, &InitialData);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkInitializeDisplayOnlyDriver failed with Status: 0x%08lX", Status);
        return Status;
    }
    return Status;
}

//
// PnP DDIs
//

VOID BddDdiUnload(VOID) { debug("[CALL]: VOID BddDdiUnload"); }

NTSTATUS BddDdiAddDevice
(
    _In_ DEVICE_OBJECT* pPhysicalDeviceObject,
    _Outptr_ PVOID*  ppDeviceContext
)
{
    debug("[CALL]: NTSTATUS BddDdiAddDevice");
    if ((pPhysicalDeviceObject == NULL) ||
        (ppDeviceContext == NULL))
    {
        debug("[WARN]: One of pPhysicalDeviceObject (0x%08lX), ppDeviceContext (0x%08lX) is NULL",
                        pPhysicalDeviceObject, ppDeviceContext);
        return STATUS_INVALID_PARAMETER;
    }
    *ppDeviceContext = NULL;
    BASIC_DISPLAY_DRIVER* pBDD = new(NonPagedPoolNx) BASIC_DISPLAY_DRIVER(pPhysicalDeviceObject);
    if (pBDD == NULL)
    {
        debug("[WARN]: pBDD failed to be allocated");
        return STATUS_NO_MEMORY;
    }
    *ppDeviceContext = pBDD;
    return STATUS_SUCCESS;
}

NTSTATUS BddDdiRemoveDevice
(
    _In_  VOID* pDeviceContext
)
{
    debug("[CALL]: NTSTATUS BddDdiRemoveDevice");
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    if (pBDD)
    {
        delete pBDD;
        pBDD = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS BddDdiStartDevice
(
    _In_  VOID*              pDeviceContext,
    _In_  DXGK_START_INFO*   pDxgkStartInfo,
    _In_  DXGKRNL_INTERFACE* pDxgkInterface,
    _Out_ ULONG*             pNumberOfViews,
    _Out_ ULONG*             pNumberOfChildren
)
{
    debug("[CALL]: NTSTATUS BddDdiStartDevice");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiStartDevice"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->StartDevice(pDxgkStartInfo, pDxgkInterface, pNumberOfViews, pNumberOfChildren);
}

NTSTATUS BddDdiStopDevice
(
    _In_  VOID* pDeviceContext
)
{
    debug("[CALL]: NTSTATUS BddDdiStopDevice");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiStopDevice"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->StopDevice();
}


NTSTATUS BddDdiDispatchIoRequest
(
    _In_  VOID*                 pDeviceContext,
    _In_  ULONG                 VidPnSourceId,
    _In_  VIDEO_REQUEST_PACKET* pVideoRequestPacket
)
{
    debug("[CALL]: NTSTATUS BddDdiDispatchIoRequest");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiDispatchIoRequest"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    if (!pBDD->IsDriverActive())
    {
        debug("BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->DispatchIoRequest(VidPnSourceId, pVideoRequestPacket);
}

NTSTATUS BddDdiSetPowerState
(
    _In_  VOID*              pDeviceContext,
    _In_  ULONG              HardwareUid,
    _In_  DEVICE_POWER_STATE DevicePowerState,
    _In_  POWER_ACTION       ActionType
)
{
    debug("[CALL]: NTSTATUS BddDdiSetPowerState");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiSetPowerState"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    if (!pBDD->IsDriverActive())
    {
        // If the driver isn't active, SetPowerState can still be called, however in BDD's case
        // this shouldn't do anything, as it could for instance be called on BDD Fallback after
        // Fallback has been stopped and BDD PnP is being started. Fallback doesn't have control
        // of the hardware in this case.
        return STATUS_SUCCESS;
    }
    return pBDD->SetPowerState(HardwareUid, DevicePowerState, ActionType);
}

NTSTATUS BddDdiQueryChildRelations
(
    _In_                             VOID*                  pDeviceContext,
    _Out_writes_bytes_(ChildRelationsSize) DXGK_CHILD_DESCRIPTOR* pChildRelations,
    _In_                             ULONG                  ChildRelationsSize
)
{
    debug("[CALL]: NTSTATUS BddDdiQueryChildRelations");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiQueryChildRelations"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->QueryChildRelations(pChildRelations, ChildRelationsSize);
}

NTSTATUS BddDdiQueryChildStatus
(
    _In_    VOID*              pDeviceContext,
    _Inout_ DXGK_CHILD_STATUS* pChildStatus,
    _In_    BOOLEAN            NonDestructiveOnly
)
{
    debug("[CALL]: NTSTATUS BddDdiQueryChildStatus");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiQueryChildStatus"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->QueryChildStatus(pChildStatus, NonDestructiveOnly);
}

NTSTATUS BddDdiQueryDeviceDescriptor
(
    _In_  VOID*                     pDeviceContext,
    _In_  ULONG                     ChildUid,
    _Inout_ DXGK_DEVICE_DESCRIPTOR* pDeviceDescriptor
)
{
    debug("[CALL]: NTSTATUS BddDdiQueryDeviceDescriptor");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS BddDdiQueryDeviceDescriptor"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    if (!pBDD->IsDriverActive())
    {
        // During stress testing of PnPStop, it is possible for BDD Fallback to get called to start then stop in quick succession.
        // The first call queues a worker thread item indicating that it now has a child device, the second queues a worker thread
        // item that it no longer has any child device. This function gets called based on the first worker thread item, but after
        // the driver has been stopped. Therefore instead of asserting like other functions, we only warn.
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->QueryDeviceDescriptor(ChildUid, pDeviceDescriptor);
}

//
// WDDM Display Only Driver DDIs
//

NTSTATUS APIENTRY BddDdiQueryAdapterInfo
(
    _In_ CONST HANDLE                    hAdapter,
    _In_ CONST DXGKARG_QUERYADAPTERINFO* pQueryAdapterInfo
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiQueryAdapterInfo");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiQueryAdapterInfo"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    return pBDD->QueryAdapterInfo(pQueryAdapterInfo);
}

NTSTATUS APIENTRY BddDdiSetPointerPosition
(
    _In_ CONST HANDLE                      hAdapter,
    _In_ CONST DXGKARG_SETPOINTERPOSITION* pSetPointerPosition
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiSetPointerPosition");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiSetPointerPosition"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->SetPointerPosition(pSetPointerPosition);
}

NTSTATUS APIENTRY BddDdiSetPointerShape
(
    _In_ CONST HANDLE                   hAdapter,
    _In_ CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiSetPointerShape");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiSetPointerShape"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->SetPointerShape(pSetPointerShape);
}

NTSTATUS APIENTRY BddDdiPresentDisplayOnly
(
    _In_ CONST HANDLE                       hAdapter,
    _In_ CONST DXGKARG_PRESENT_DISPLAYONLY* pPresentDisplayOnly
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiPresentDisplayOnly");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiPresentDisplayOnly"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->PresentDisplayOnly(pPresentDisplayOnly);
}

NTSTATUS BddDdiStopDeviceAndReleasePostDisplayOwnership
(
    _In_  VOID*                          pDeviceContext,
    _In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _Out_ PDXGK_DISPLAY_INFORMATION      DisplayInfo
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiStopDeviceAndReleasePostDisplayOwnership");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS APIENTRY BddDdiStopDeviceAndReleasePostDisplayOwnership"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->StopDeviceAndReleasePostDisplayOwnership(TargetId, DisplayInfo);
}

NTSTATUS APIENTRY BddDdiIsSupportedVidPn
(
    _In_ CONST HANDLE                 hAdapter,
    _Inout_ DXGKARG_ISSUPPORTEDVIDPN* pIsSupportedVidPn
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiIsSupportedVidPn");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiIsSupportedVidPn"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        // This path might hit because win32k/dxgport doesn't check that an adapter is active when taking the adapter lock.
        // The adapter lock is the main thing BDD Fallback relies on to not be called while it's inactive. It is still a rare
        // timing issue around PnpStart/Stop and isn't expected to have any effect on the stability of the system.
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    //if (!(pIsSupportedVidPn != NULL)) { debug("[ASRT]: (pIsSupportedVidPn = NULL) | NTSTATUS APIENTRY BddDdiIsSupportedVidPn"); return STATUS_UNSUCCESSFUL; }
    //else { debug("[INFO]: Video Present Network <<<UNQUERIABLE ID>>> supported = IDK"); }
    //debug("[INFO]: Video Present Network <<<UNQUERIABLE ID>>> supported = %s", /* pIsSupportedVidPn->hDesiredVidPn->unused, */ (pIsSupportedVidPn->IsVidPnSupported) ? "Yes" : "No");
    return pBDD->IsSupportedVidPn(pIsSupportedVidPn);
}

NTSTATUS APIENTRY BddDdiRecommendFunctionalVidPn
(
    _In_ CONST HANDLE                                  hAdapter,
    _In_ CONST DXGKARG_RECOMMENDFUNCTIONALVIDPN* CONST pRecommendFunctionalVidPn
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiRecommendFunctionalVidPn");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiRecommendFunctionalVidPn"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->RecommendFunctionalVidPn(pRecommendFunctionalVidPn);
}

NTSTATUS APIENTRY BddDdiRecommendVidPnTopology
(
    _In_ CONST HANDLE                                 hAdapter,
    _In_ CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY* CONST  pRecommendVidPnTopology
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiRecommendVidPnTopology");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiRecommendVidPnTopology"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->RecommendVidPnTopology(pRecommendVidPnTopology);
}

NTSTATUS APIENTRY BddDdiRecommendMonitorModes
(
    _In_ CONST HANDLE                                hAdapter,
    _In_ CONST DXGKARG_RECOMMENDMONITORMODES* CONST  pRecommendMonitorModes
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiRecommendMonitorModes");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiRecommendMonitorModes"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->RecommendMonitorModes(pRecommendMonitorModes);
}

NTSTATUS APIENTRY BddDdiEnumVidPnCofuncModality
(
    _In_ CONST HANDLE                                 hAdapter,
    _In_ CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST pEnumCofuncModality
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiEnumVidPnCofuncModality");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiEnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->EnumVidPnCofuncModality(pEnumCofuncModality);
}

NTSTATUS APIENTRY BddDdiSetVidPnSourceVisibility
(
    _In_ CONST HANDLE                            hAdapter,
    _In_ CONST DXGKARG_SETVIDPNSOURCEVISIBILITY* pSetVidPnSourceVisibility
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiSetVidPnSourceVisibility");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiSetVidPnSourceVisibility"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->SetVidPnSourceVisibility(pSetVidPnSourceVisibility);
}

NTSTATUS APIENTRY BddDdiCommitVidPn
(
    _In_ CONST HANDLE                     hAdapter,
    _In_ CONST DXGKARG_COMMITVIDPN* CONST pCommitVidPn
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiCommitVidPn");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiCommitVidPn"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->CommitVidPn(pCommitVidPn);
}

NTSTATUS APIENTRY BddDdiUpdateActiveVidPnPresentPath
(
    _In_ CONST HANDLE                                      hAdapter,
    _In_ CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* CONST pUpdateActiveVidPnPresentPath
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiUpdateActiveVidPnPresentPath");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiUpdateActiveVidPnPresentPath"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->UpdateActiveVidPnPresentPath(pUpdateActiveVidPnPresentPath);
}

NTSTATUS APIENTRY BddDdiQueryVidPnHWCapability
(
    _In_ CONST HANDLE                       hAdapter,
    _Inout_ DXGKARG_QUERYVIDPNHWCAPABILITY* pVidPnHWCaps
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiQueryVidPnHWCapability");
    if (!(hAdapter != NULL)) { debug("[ASRT]: (hAdapter = NULL) | NTSTATUS APIENTRY BddDdiQueryVidPnHWCapability"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(hAdapter);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return STATUS_UNSUCCESSFUL;
    }
    return pBDD->QueryVidPnHWCapability(pVidPnHWCaps);
}

VOID BddDdiDpcRoutine
(
    _In_  VOID* pDeviceContext
)
{
    debug("[CALL]: VOID BddDdiDpcRoutine");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | VOID BddDdiDpcRoutine"); return; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    if (!pBDD->IsDriverActive())
    {
        debug("[WARN]: BDD (0x%08lX) is being called when not active!", pBDD);
        return;
    }
    pBDD->DpcRoutine();
}

BOOLEAN BddDdiInterruptRoutine
(
    _In_  VOID* pDeviceContext,
    _In_  ULONG MessageNumber
)
{
    return TRUE; // As quickly as possible...
    UNREFERENCED_PARAMETER(pDeviceContext);
    UNREFERENCED_PARAMETER(MessageNumber);
    //debug("[CALL]: BOOLEAN BddDdiInterruptRoutine");
    //if (!(pDeviceContext != NULL)) { /*debug("[ASRT]: (pDeviceContext = NULL) | BOOLEAN BddDdiInterruptRoutine");*/ return FALSE; }
    //BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    //return pBDD->InterruptRoutine(MessageNumber);
}

VOID BddDdiResetDevice
(
    _In_  VOID* pDeviceContext
)
{
    debug("[CALL]: VOID BddDdiResetDevice");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | BOOLEAN BddDdiInterruptRoutine"); return; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    pBDD->ResetDevice();
}

NTSTATUS BddDdiSystemDisplayEnable
(
    _In_  VOID* pDeviceContext,
    _In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
    _In_  PDXGKARG_SYSTEM_DISPLAY_ENABLE_FLAGS Flags,
    _Out_ UINT* Width,
    _Out_ UINT* Height,
    _Out_ D3DDDIFORMAT* ColorFormat
)
{
    debug("[CALL]: NTSTATUS APIENTRY BddDdiSystemDisplayEnable");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | NTSTATUS APIENTRY BddDdiSystemDisplayEnable"); return STATUS_ABANDONED; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    return pBDD->SystemDisplayEnable(TargetId, Flags, Width, Height, ColorFormat);
}

VOID BddDdiSystemDisplayWrite
(
    _In_  VOID* pDeviceContext,
    _In_  VOID* Source,
    _In_  UINT  SourceWidth,
    _In_  UINT  SourceHeight,
    _In_  UINT  SourceStride,
    _In_  UINT  PositionX,
    _In_  UINT  PositionY
)
{
    debug("[CALL]: VOID APIENTRY BddDdiSystemDisplayWrite");
    if (!(pDeviceContext != NULL)) { debug("[ASRT]: (pDeviceContext = NULL) | VOID APIENTRY BddDdiSystemDisplayWrite"); return; }
    BASIC_DISPLAY_DRIVER* pBDD = reinterpret_cast<BASIC_DISPLAY_DRIVER*>(pDeviceContext);
    pBDD->SystemDisplayWrite(Source, SourceWidth, SourceHeight, SourceStride, PositionX, PositionY);
}