#include "BDD.hxx"

// Display-Only Devices can only return display modes of D3DDDIFMT_A8R8G8B8.
// Color conversion takes place if the app's fullscreen backbuffer has different format.
// Full display drivers can add more if the hardware supports them.
D3DDDIFORMAT gBddPixelFormats[] = { D3DDDIFMT_A8R8G8B8 };

// TODO: Need to also check pinned modes and the path parameters, not just topology
NTSTATUS BASIC_DISPLAY_DRIVER::IsSupportedVidPn(_Inout_ DXGKARG_ISSUPPORTEDVIDPN* pIsSupportedVidPn)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::IsSupportedVidPn");
    if (!(pIsSupportedVidPn != NULL)) { debug("[ASRT]: (pIsSupportedVidPn = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::IsSupportedVidPn"); return STATUS_ABANDONED; }

    if (pIsSupportedVidPn->hDesiredVidPn == 0)
    {
        // A null desired VidPn is supported
        pIsSupportedVidPn->IsVidPnSupported = TRUE;
        return STATUS_SUCCESS;
    }

    // Default to not supported, until shown it is supported
    pIsSupportedVidPn->IsVidPnSupported = FALSE;

    CONST DXGK_VIDPN_INTERFACE* pVidPnInterface;
    NTSTATUS Status = m_DxgkInterface.DxgkCbQueryVidPnInterface(pIsSupportedVidPn->hDesiredVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkCbQueryVidPnInterface failed with Status = 0x%08lX, hDesiredVidPn = %p", Status, pIsSupportedVidPn->hDesiredVidPn);
        return Status;
    }

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    CONST DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(pIsSupportedVidPn->hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnGetTopology failed with Status = 0x%08lX, hDesiredVidPn = %p", Status, pIsSupportedVidPn->hDesiredVidPn);
        return Status;
    }

    // For every source in this topology, make sure they don't have more paths than there are targets
    for (D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId = 0; SourceId < MAX_VIEWS; ++SourceId)
    {
        SIZE_T NumPathsFromSource = 0;
        Status = pVidPnTopologyInterface->pfnGetNumPathsFromSource(hVidPnTopology, SourceId, &NumPathsFromSource);
        if (Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
        {
            continue;
        }
        else if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnGetNumPathsFromSource failed with Status = 0x%08lX. hVidPnTopology = %p, SourceId = 0x%08lX",
                  Status, hVidPnTopology, SourceId);
            return Status;
        }
        else if (NumPathsFromSource > MAX_CHILDREN)
        {
            // This VidPn is not supported, which has already been set as the default
            return STATUS_SUCCESS;
        }
    }

    // All sources succeeded so this VidPn is supported
    pIsSupportedVidPn->IsVidPnSupported = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::RecommendFunctionalVidPn(_In_ CONST DXGKARG_RECOMMENDFUNCTIONALVIDPN* CONST pRecommendFunctionalVidPn)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::RecommendFunctionalVidPn");
    if (!(pRecommendFunctionalVidPn == NULL)) { debug("[ASRT]: (pRecommendFunctionalVidPn != NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::RecommendFunctionalVidPn"); return STATUS_ABANDONED; }
    return STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN;
}

NTSTATUS BASIC_DISPLAY_DRIVER::RecommendVidPnTopology(_In_ CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY* CONST pRecommendVidPnTopology)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::RecommendVidPnTopology");
    if (!(pRecommendVidPnTopology == NULL)) { debug("[ASRT]: (pRecommendVidPnTopology != NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::RecommendVidPnTopology"); return STATUS_ABANDONED; }
    return STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN;
}

NTSTATUS BASIC_DISPLAY_DRIVER::RecommendMonitorModes(_In_ CONST DXGKARG_RECOMMENDMONITORMODES* CONST pRecommendMonitorModes)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::RecommendMonitorModes");
    // This is always called to recommend modes for the monitor. The sample driver doesn't provide EDID for a monitor, so 
    // the OS prefills the list with default monitor modes. Since the required mode might not be in the list, it should 
    // be provided as a recommended mode.
    return AddSingleMonitorMode(pRecommendMonitorModes);
}

// Tell DMM about all the modes, etc. that are supported
NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality(_In_ CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST pEnumCofuncModality)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality");
    if (!(pEnumCofuncModality != NULL)) { debug("[ASRT]: (pEnumCofuncModality = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }

    D3DKMDT_HVIDPNTOPOLOGY                   hVidPnTopology = 0;
    D3DKMDT_HVIDPNSOURCEMODESET              hVidPnSourceModeSet = 0;
    D3DKMDT_HVIDPNTARGETMODESET              hVidPnTargetModeSet = 0;
    CONST DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    CONST DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface = NULL;
    CONST DXGK_VIDPNSOURCEMODESET_INTERFACE* pVidPnSourceModeSetInterface = NULL;
    CONST DXGK_VIDPNTARGETMODESET_INTERFACE* pVidPnTargetModeSetInterface = NULL;
    CONST D3DKMDT_VIDPN_PRESENT_PATH* pVidPnPresentPath = NULL;
    CONST D3DKMDT_VIDPN_PRESENT_PATH* pVidPnPresentPathTemp = NULL; // Used for AcquireNextPathInfo
    CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnPinnedSourceModeInfo = NULL;
    CONST D3DKMDT_VIDPN_TARGET_MODE* pVidPnPinnedTargetModeInfo = NULL;

    // Get the VidPn Interface so we can get the 'Source Mode Set', 'Target Mode Set' and 'VidPn Topology' interfaces
    NTSTATUS Status = m_DxgkInterface.DxgkCbQueryVidPnInterface(pEnumCofuncModality->hConstrainingVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkCbQueryVidPnInterface failed with Status = 0x%08lX, hFunctionalVidPn = %p", Status, pEnumCofuncModality->hConstrainingVidPn);
        return Status;
    }

    // Get the VidPn Topology interface so we can enumerate all paths
    Status = pVidPnInterface->pfnGetTopology(pEnumCofuncModality->hConstrainingVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnGetTopology failed with Status = 0x%08lX, hFunctionalVidPn = %p", Status, pEnumCofuncModality->hConstrainingVidPn);
        return Status;
    }

    // Get the first path before we start looping through them
    Status = pVidPnTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pVidPnPresentPath);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnAcquireFirstPathInfo failed with Status = 0x%08lX, hVidPnTopology = %p", Status, hVidPnTopology);
        return Status;
    }

    // Loop through all available paths.
    while (Status != STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
    {
        // Get the Source Mode Set interface so the pinned mode can be retrieved
        Status = pVidPnInterface->pfnAcquireSourceModeSet(pEnumCofuncModality->hConstrainingVidPn,
            pVidPnPresentPath->VidPnSourceId,
            &hVidPnSourceModeSet,
            &pVidPnSourceModeSetInterface);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquireSourceModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, SourceId = 0x%08lX",
                Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnSourceId);
            break;
        }

        // Get the pinned mode, needed when VidPnSource isn't pivot, and when VidPnTarget isn't pivot
        Status = pVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnSourceModeSet, &pVidPnPinnedSourceModeInfo);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquirePinnedModeInfo failed with Status = 0x%08lX, hVidPnSourceModeSet = %p", Status, hVidPnSourceModeSet);
            break;
        }

        // SOURCE MODES: If this source mode isn't the pivot point, do work on the source mode set
        if (!((pEnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNSOURCE) &&
            (pEnumCofuncModality->EnumPivot.VidPnSourceId == pVidPnPresentPath->VidPnSourceId)))
        {
            // If there's no pinned source add possible modes (otherwise they've already been added)
            if (pVidPnPinnedSourceModeInfo == NULL)
            {
                // Release the acquired source mode set, since going to create a new one to put all modes in
                Status = pVidPnInterface->pfnReleaseSourceModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnSourceModeSet);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnReleaseSourceModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, hVidPnSourceModeSet = %p",
                        Status, pEnumCofuncModality->hConstrainingVidPn, hVidPnSourceModeSet);
                    break;
                }
                hVidPnSourceModeSet = 0; // Successfully released it

                // Create a new source mode set which will be added to the constraining VidPn with all the possible modes
                Status = pVidPnInterface->pfnCreateNewSourceModeSet(pEnumCofuncModality->hConstrainingVidPn,
                    pVidPnPresentPath->VidPnSourceId,
                    &hVidPnSourceModeSet,
                    &pVidPnSourceModeSetInterface);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnCreateNewSourceModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, SourceId = 0x%08lX",
                        Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnSourceId);
                    break;
                }

                // Add the appropriate modes to the source mode set
                {
                    Status = AddSingleSourceMode(pVidPnSourceModeSetInterface, hVidPnSourceModeSet, pVidPnPresentPath->VidPnSourceId);
                }

                if (!NT_SUCCESS(Status))
                {
                    break;
                }

                // Give DMM back the source modes just populated
                Status = pVidPnInterface->pfnAssignSourceModeSet(pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnSourceId, hVidPnSourceModeSet);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnAssignSourceModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, SourceId = 0x%08lX, hVidPnSourceModeSet = %p",
                        Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnSourceId, hVidPnSourceModeSet);
                    break;
                }
                hVidPnSourceModeSet = 0; // Successfully assigned it (equivalent to releasing it)
            }
        }// End: SOURCE MODES

        // TARGET MODES: If this target mode isn't the pivot point, do work on the target mode set
        if (!((pEnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNTARGET) &&
            (pEnumCofuncModality->EnumPivot.VidPnTargetId == pVidPnPresentPath->VidPnTargetId)))
        {
            // Get the Target Mode Set interface so modes can be added if necessary
            Status = pVidPnInterface->pfnAcquireTargetModeSet(pEnumCofuncModality->hConstrainingVidPn,
                pVidPnPresentPath->VidPnTargetId,
                &hVidPnTargetModeSet,
                &pVidPnTargetModeSetInterface);
            if (!NT_SUCCESS(Status))
            {
                debug("[WARN]: pfnAcquireTargetModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, TargetId = 0x%08lX",
                    Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnTargetId);
                break;
            }

            Status = pVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnTargetModeSet, &pVidPnPinnedTargetModeInfo);
            if (!NT_SUCCESS(Status))
            {
                debug("[WARN]: pfnAcquirePinnedModeInfo failed with Status = 0x%08lX, hVidPnTargetModeSet = %p", Status, hVidPnTargetModeSet);
                break;
            }

            // If there's no pinned target add possible modes (otherwise they've already been added)
            if (pVidPnPinnedTargetModeInfo == NULL)
            {
                // Release the acquired target mode set, since going to create a new one to put all modes in
                Status = pVidPnInterface->pfnReleaseTargetModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnTargetModeSet);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnReleaseTargetModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, hVidPnTargetModeSet = %p",
                        Status, pEnumCofuncModality->hConstrainingVidPn, hVidPnTargetModeSet);
                    break;
                }
                hVidPnTargetModeSet = 0; // Successfully released it

                // Create a new target mode set which will be added to the constraining VidPn with all the possible modes
                Status = pVidPnInterface->pfnCreateNewTargetModeSet(pEnumCofuncModality->hConstrainingVidPn,
                    pVidPnPresentPath->VidPnTargetId,
                    &hVidPnTargetModeSet,
                    &pVidPnTargetModeSetInterface);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnCreateNewTargetModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, TargetId = 0x%08lX",
                        Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnTargetId);
                    break;
                }

                Status = AddSingleTargetMode(pVidPnTargetModeSetInterface, hVidPnTargetModeSet, pVidPnPinnedSourceModeInfo, pVidPnPresentPath->VidPnSourceId);

                if (!NT_SUCCESS(Status))
                {
                    break;
                }

                // Give DMM back the source modes just populated
                Status = pVidPnInterface->pfnAssignTargetModeSet(pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnTargetId, hVidPnTargetModeSet);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnAssignTargetModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, TargetId = 0x%08lX, hVidPnTargetModeSet = %p",
                        Status, pEnumCofuncModality->hConstrainingVidPn, pVidPnPresentPath->VidPnTargetId, hVidPnTargetModeSet);
                    break;
                }
                hVidPnTargetModeSet = 0; // Successfully assigned it (equivalent to releasing it)
            }
            else
            {
                // Release the pinned target as there's no other work to do
                Status = pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hVidPnTargetModeSet, pVidPnPinnedTargetModeInfo);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnReleaseModeInfo failed with Status = 0x%08lX, hVidPnTargetModeSet = %p, pVidPnPinnedTargetModeInfo = %p",
                        Status, hVidPnTargetModeSet, pVidPnPinnedTargetModeInfo);
                    break;
                }
                pVidPnPinnedTargetModeInfo = NULL; // Successfully released it

                // Release the acquired target mode set, since it is no longer needed
                Status = pVidPnInterface->pfnReleaseTargetModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnTargetModeSet);
                if (!NT_SUCCESS(Status))
                {
                    debug("[WARN]: pfnReleaseTargetModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, hVidPnTargetModeSet = %p",
                        Status, pEnumCofuncModality->hConstrainingVidPn, hVidPnTargetModeSet);
                    break;
                }
                hVidPnTargetModeSet = 0; // Successfully released it
            }
        }// End: TARGET MODES

        // Nothing else needs the pinned source mode so release it
        if (pVidPnPinnedSourceModeInfo != NULL)
        {
            Status = pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hVidPnSourceModeSet, pVidPnPinnedSourceModeInfo);
            if (!NT_SUCCESS(Status))
            {
                debug("[WARN]: pfnReleaseModeInfo failed with Status = 0x%08lX, hVidPnSourceModeSet = %p, pVidPnPinnedSourceModeInfo = %p",
                    Status, hVidPnSourceModeSet, pVidPnPinnedSourceModeInfo);
                break;
            }
            pVidPnPinnedSourceModeInfo = NULL; // Successfully released it
        }

        // With the pinned source mode now released, if the source mode set hasn't been released, release that as well
        if (hVidPnSourceModeSet != 0)
        {
            Status = pVidPnInterface->pfnReleaseSourceModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnSourceModeSet);
            if (!NT_SUCCESS(Status))
            {
                debug("[WARN]: pfnReleaseSourceModeSet failed with Status = 0x%08lX, hConstrainingVidPn = %p, hVidPnSourceModeSet = %p",
                    Status, pEnumCofuncModality->hConstrainingVidPn, hVidPnSourceModeSet);
                break;
            }
            hVidPnSourceModeSet = 0; // Successfully released it
        }

        // If modifying support fields, need to modify a local version of a path structure since the retrieved one is const
        D3DKMDT_VIDPN_PRESENT_PATH LocalVidPnPresentPath = *pVidPnPresentPath;
        BOOLEAN SupportFieldsModified = FALSE;

        // SCALING: If this path's scaling isn't the pivot point, do work on the scaling support
        if (!((pEnumCofuncModality->EnumPivotType == D3DKMDT_EPT_SCALING) &&
            (pEnumCofuncModality->EnumPivot.VidPnSourceId == pVidPnPresentPath->VidPnSourceId) &&
            (pEnumCofuncModality->EnumPivot.VidPnTargetId == pVidPnPresentPath->VidPnTargetId)))
        {
            // If the scaling is unpinned, then modify the scaling support field
            if (pVidPnPresentPath->ContentTransformation.Scaling == D3DKMDT_VPPS_UNPINNED)
            {
                // Identity and centered scaling are supported, but not any stretch modes
                RtlZeroMemory(&(LocalVidPnPresentPath.ContentTransformation.ScalingSupport), sizeof(D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT));
                LocalVidPnPresentPath.ContentTransformation.ScalingSupport.Identity = 1;
                LocalVidPnPresentPath.ContentTransformation.ScalingSupport.Centered = 1;
                SupportFieldsModified = TRUE;
            }
        } // End: SCALING

        // ROTATION: If this path's rotation isn't the pivot point, do work on the rotation support
        if (!((pEnumCofuncModality->EnumPivotType != D3DKMDT_EPT_ROTATION) &&
            (pEnumCofuncModality->EnumPivot.VidPnSourceId == pVidPnPresentPath->VidPnSourceId) &&
            (pEnumCofuncModality->EnumPivot.VidPnTargetId == pVidPnPresentPath->VidPnTargetId)))
        {
            // If the rotation is unpinned, then modify the rotation support field
            if (pVidPnPresentPath->ContentTransformation.Rotation == D3DKMDT_VPPR_UNPINNED)
            {
                LocalVidPnPresentPath.ContentTransformation.RotationSupport.Identity = 1;
                // Sample supports only Rotate90
                LocalVidPnPresentPath.ContentTransformation.RotationSupport.Rotate90 = 1;
                LocalVidPnPresentPath.ContentTransformation.RotationSupport.Rotate180 = 0;
                LocalVidPnPresentPath.ContentTransformation.RotationSupport.Rotate270 = 0;

                // Since clone is not supported, should not support path-independent rotations
                LocalVidPnPresentPath.ContentTransformation.RotationSupport.Offset0 = 1;

                SupportFieldsModified = TRUE;
            }
        } // End: ROTATION

        if (SupportFieldsModified)
        {
            // The correct path will be found by this function and the appropriate fields updated
            Status = pVidPnTopologyInterface->pfnUpdatePathSupportInfo(hVidPnTopology, &LocalVidPnPresentPath);
            if (!NT_SUCCESS(Status))
            {
                debug("[WARN]: pfnUpdatePathSupportInfo failed with Status = 0x%08lX, hVidPnTopology = %p", Status, hVidPnTopology);
                break;
            }
        }

        // Get the next path...
        // (NOTE: This is the value of Status that will return STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET when it's time to quit the loop)
        pVidPnPresentPathTemp = pVidPnPresentPath;
        Status = pVidPnTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pVidPnPresentPathTemp, &pVidPnPresentPath);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquireNextPathInfo failed with Status = 0x%08lX, hVidPnTopology = %p, pVidPnPresentPathTemp = %p", Status, hVidPnTopology, pVidPnPresentPathTemp);
            break;
        }

        // ...and release the last path
        NTSTATUS TempStatus = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathTemp);
        if (!NT_SUCCESS(TempStatus))
        {
            debug("[WARN]: pfnReleasePathInfo failed with Status = 0x%08lX, hVidPnTopology = %p, pVidPnPresentPathTemp = %p", TempStatus, hVidPnTopology, pVidPnPresentPathTemp);
            Status = TempStatus;
            break;
        }
        pVidPnPresentPathTemp = NULL; // Successfully released it
    }// End: while loop for paths in topology

    // If quit the while loop normally, set the return value to success
    if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
    {
        Status = STATUS_SUCCESS;
    }

    // Release any resources hanging around because the loop was quit early.
    // Since in normal execution everything should be released by this point, TempStatus is initialized to a bogus error to be used as an
    //  assertion that if anything had to be released now (TempStatus changing) Status isn't successful.
    NTSTATUS TempStatus = STATUS_NOT_FOUND;

    if ((pVidPnSourceModeSetInterface != NULL) &&
        (pVidPnPinnedSourceModeInfo != NULL))
    {
        TempStatus = pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hVidPnSourceModeSet, pVidPnPinnedSourceModeInfo);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: ((pVidPnSourceModeSetInterface) || (pVidPnPinnedSourceModeInfo)) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if ((pVidPnTargetModeSetInterface != NULL) &&
        (pVidPnPinnedTargetModeInfo != NULL))
    {
        TempStatus = pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hVidPnTargetModeSet, pVidPnPinnedTargetModeInfo);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: ((pVidPnTargetModeSetInterface) || (pVidPnPinnedTargetModeInfo)) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if (pVidPnPresentPath != NULL)
    {
        TempStatus = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPath);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: (pVidPnPresentPath) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if (pVidPnPresentPathTemp != NULL)
    {
        TempStatus = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathTemp);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: pVidPnPresentPathTemp) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if (hVidPnSourceModeSet != 0)
    {
        TempStatus = pVidPnInterface->pfnReleaseSourceModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnSourceModeSet);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: (hVidPnSourceModeSet = 0) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if (hVidPnTargetModeSet != 0)
    {
        TempStatus = pVidPnInterface->pfnReleaseTargetModeSet(pEnumCofuncModality->hConstrainingVidPn, hVidPnTargetModeSet);
        if (!(NT_SUCCESS(TempStatus))) { debug("[ASRT]: (hVidPnTargetModeSet = 0) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    }

    if (!((TempStatus == STATUS_NOT_FOUND) || (Status != STATUS_SUCCESS))) { debug("[ASRT]: ((TempStatus != STATUS_NOT_FOUND) || (Status != STATUS_SUCCESS)) | NTSTATUS BASIC_DISPLAY_DRIVER::EnumVidPnCofuncModality"); return STATUS_ABANDONED; }
    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::SetVidPnSourceVisibility(_In_ CONST DXGKARG_SETVIDPNSOURCEVISIBILITY* pSetVidPnSourceVisibility)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SetVidPnSourceVisibility");
    if (!(pSetVidPnSourceVisibility != NULL)) { debug("[ASRT]: (pSetVidPnSourceVisibility = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::SetVidPnSourceVisibility"); return STATUS_ABANDONED; }
    if (!((pSetVidPnSourceVisibility->VidPnSourceId < MAX_VIEWS) ||
               (pSetVidPnSourceVisibility->VidPnSourceId == D3DDDI_ID_ALL)))
        { debug("[ASRT]: ((pSetVidPnSourceVisibility->VidPnSourceId < MAX_VIEWS) || (pSetVidPnSourceVisibility->VidPnSourceId == D3DDDI_ID_ALL)) | NTSTATUS BASIC_DISPLAY_DRIVER::SetVidPnSourceVisibility"); return STATUS_ABANDONED; }
    UINT StartVidPnSourceId = (pSetVidPnSourceVisibility->VidPnSourceId == D3DDDI_ID_ALL) ? 0 : pSetVidPnSourceVisibility->VidPnSourceId;
    UINT MaxVidPnSourceId = (pSetVidPnSourceVisibility->VidPnSourceId == D3DDDI_ID_ALL) ? MAX_VIEWS : pSetVidPnSourceVisibility->VidPnSourceId + 1;

    for (UINT SourceId = StartVidPnSourceId; SourceId < MaxVidPnSourceId; ++SourceId)
    {
        if (pSetVidPnSourceVisibility->Visible)
        {
            m_CurrentModes[SourceId].Flags.FullscreenPresent = TRUE;
        }
        else
        {
            BlackOutScreen(SourceId);
        }

        // Store current visibility so it can be dealt with during Present call
        m_CurrentModes[SourceId].Flags.SourceNotVisible = !(pSetVidPnSourceVisibility->Visible);
    }
    return STATUS_SUCCESS;
}

// NOTE: The value of pCommitVidPn->MonitorConnectivityChecks is ignored, since BDD is unable to recognize whether a monitor is connected or not
// The value of pCommitVidPn->hPrimaryAllocation is also ignored, since BDD is a display only driver and does not deal with allocations
NTSTATUS BASIC_DISPLAY_DRIVER::CommitVidPn(_In_ CONST DXGKARG_COMMITVIDPN* CONST pCommitVidPn)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::CommitVidPn");
    if (!(pCommitVidPn != NULL)) { debug("[ASRT]: (pCommitVidPn = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::CommitVidPn"); return STATUS_ABANDONED; }
    if (!(pCommitVidPn->AffectedVidPnSourceId < MAX_VIEWS)) { debug("[ASRT]: (pCommitVidPn->AffectedVidPnSourceId >= MAX_VIEWS) | NTSTATUS BASIC_DISPLAY_DRIVER::CommitVidPn"); return STATUS_ABANDONED; }

    NTSTATUS                                 Status;
    SIZE_T                                   NumPaths = 0;
    D3DKMDT_HVIDPNTOPOLOGY                   hVidPnTopology = 0;
    D3DKMDT_HVIDPNSOURCEMODESET              hVidPnSourceModeSet = 0;
    CONST DXGK_VIDPN_INTERFACE*              pVidPnInterface = NULL;
    CONST DXGK_VIDPNTOPOLOGY_INTERFACE*      pVidPnTopologyInterface = NULL;
    CONST DXGK_VIDPNSOURCEMODESET_INTERFACE* pVidPnSourceModeSetInterface = NULL;
    CONST D3DKMDT_VIDPN_PRESENT_PATH*        pVidPnPresentPath = NULL;
    CONST D3DKMDT_VIDPN_SOURCE_MODE*         pPinnedVidPnSourceModeInfo = NULL;

    // Check this CommitVidPn is for the mode change notification when monitor is in power off state.
    if (pCommitVidPn->Flags.PathPoweredOff)
    {
        // Ignore the commitVidPn call for the mode change notification when monitor is in power off state.
        Status = STATUS_SUCCESS;
        goto CommitVidPnExit;
    }

    // Get the VidPn Interface so we can get the 'Source Mode Set' and 'VidPn Topology' interfaces
    Status = m_DxgkInterface.DxgkCbQueryVidPnInterface(pCommitVidPn->hFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: DxgkCbQueryVidPnInterface failed with Status = 0x%08lX, hFunctionalVidPn = %p", Status, pCommitVidPn->hFunctionalVidPn);
        goto CommitVidPnExit;
    }

    // Get the VidPn Topology interface so can enumerate paths from source
    Status = pVidPnInterface->pfnGetTopology(pCommitVidPn->hFunctionalVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnGetTopology failed with Status = 0x%08lX, hFunctionalVidPn = %p", Status, pCommitVidPn->hFunctionalVidPn);
        goto CommitVidPnExit;
    }

    // Find out the number of paths now, if it's 0 don't bother with source mode set and pinned mode, just clear current and then quit
    Status = pVidPnTopologyInterface->pfnGetNumPaths(hVidPnTopology, &NumPaths);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnGetNumPaths failed with Status = 0x%08lX, hVidPnTopology = %p", Status, hVidPnTopology);
        goto CommitVidPnExit;
    }

    if (NumPaths != 0)
    {
        // Get the Source Mode Set interface so we can get the pinned mode
        Status = pVidPnInterface->pfnAcquireSourceModeSet(pCommitVidPn->hFunctionalVidPn,
                                                          pCommitVidPn->AffectedVidPnSourceId,
                                                          &hVidPnSourceModeSet,
                                                          &pVidPnSourceModeSetInterface);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquireSourceModeSet failed with Status = 0x%08lX, hFunctionalVidPn = %p, SourceId = 0x%08lX",
                Status, pCommitVidPn->hFunctionalVidPn, pCommitVidPn->AffectedVidPnSourceId);
            goto CommitVidPnExit;
        }

        // Get the mode that is being pinned
        Status = pVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquirePinnedModeInfo failed with Status = 0x%08lX, hFunctionalVidPn = %p", Status, pCommitVidPn->hFunctionalVidPn);
            goto CommitVidPnExit;
        }
    }
    else
    {
        // This will cause the successful quit below
        pPinnedVidPnSourceModeInfo = NULL;
    }

    if (m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].FrameBuffer.Ptr &&
        !m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].Flags.DoNotMapOrUnmap)
    {
        Status = UnmapFrameBuffer(m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].FrameBuffer.Ptr,
                                  m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].DispInfo.Pitch * m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].DispInfo.Height);
        m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].FrameBuffer.Ptr = NULL;
        m_CurrentModes[pCommitVidPn->AffectedVidPnSourceId].Flags.FrameBufferIsActive = FALSE;

        if (!NT_SUCCESS(Status))
        {
            goto CommitVidPnExit;
        }
    }

    if (pPinnedVidPnSourceModeInfo == NULL)
    {
        // There is no mode to pin on this source, any old paths here have already been cleared
        Status = STATUS_SUCCESS;
        goto CommitVidPnExit;
    }

    Status = IsVidPnSourceModeFieldsValid(pPinnedVidPnSourceModeInfo);
    if (!NT_SUCCESS(Status))
    {
        goto CommitVidPnExit;
    }

    // Get the number of paths from this source so we can loop through all paths
    SIZE_T NumPathsFromSource = 0;
    Status = pVidPnTopologyInterface->pfnGetNumPathsFromSource(hVidPnTopology, pCommitVidPn->AffectedVidPnSourceId, &NumPathsFromSource);
    if (!NT_SUCCESS(Status))
    {
        debug("[WARN]: pfnGetNumPathsFromSource failed with Status = 0x%08lX, hVidPnTopology = %p", Status, hVidPnTopology);
        goto CommitVidPnExit;
    }

    // Loop through all paths to set this mode
    for (SIZE_T PathIndex = 0; PathIndex < NumPathsFromSource; ++PathIndex)
    {
        // Get the target id for this path
        D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId = D3DDDI_ID_UNINITIALIZED;
        Status = pVidPnTopologyInterface->pfnEnumPathTargetsFromSource(hVidPnTopology, pCommitVidPn->AffectedVidPnSourceId, PathIndex, &TargetId);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnEnumPathTargetsFromSource failed with Status = 0x%08lX, hVidPnTopology = %p, SourceId = 0x%08lX, PathIndex = 0x%016llX",
                  Status, hVidPnTopology, pCommitVidPn->AffectedVidPnSourceId, PathIndex);
            goto CommitVidPnExit;
        }

        // Get the actual path info
        Status = pVidPnTopologyInterface->pfnAcquirePathInfo(hVidPnTopology, pCommitVidPn->AffectedVidPnSourceId, TargetId, &pVidPnPresentPath);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnAcquirePathInfo failed with Status = 0x%08lX, hVidPnTopology = %p, SourceId = 0x%08lX, TargetId = 0x%08lX",
                  Status, hVidPnTopology, pCommitVidPn->AffectedVidPnSourceId, TargetId);
            goto CommitVidPnExit;
        }

        Status = IsVidPnPathFieldsValid(pVidPnPresentPath);
        if (!NT_SUCCESS(Status))
        {
            goto CommitVidPnExit;
        }

        Status = SetSourceModeAndPath(pPinnedVidPnSourceModeInfo, pVidPnPresentPath);
        if (!NT_SUCCESS(Status))
        {
            goto CommitVidPnExit;
        }

        Status = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPath);
        if (!NT_SUCCESS(Status))
        {
            debug("[WARN]: pfnReleasePathInfo failed with Status = 0x%08lX, hVidPnTopoogy = %p, pVidPnPresentPath = %p",
                            Status, hVidPnTopology, pVidPnPresentPath);
            goto CommitVidPnExit;
        }
        pVidPnPresentPath = NULL; // Successfully released it
    }

CommitVidPnExit:

    NTSTATUS TempStatus;

    if ((pVidPnSourceModeSetInterface != NULL) &&
        (hVidPnSourceModeSet != 0) &&
        (pPinnedVidPnSourceModeInfo != NULL))
    {
        TempStatus = pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        NT_ASSERT(NT_SUCCESS(TempStatus));
    }

    if ((pVidPnInterface != NULL) &&
        (pCommitVidPn->hFunctionalVidPn != 0) &&
        (hVidPnSourceModeSet != 0))
    {
        TempStatus = pVidPnInterface->pfnReleaseSourceModeSet(pCommitVidPn->hFunctionalVidPn, hVidPnSourceModeSet);
        NT_ASSERT(NT_SUCCESS(TempStatus));
    }

    if ((pVidPnTopologyInterface != NULL) &&
        (hVidPnTopology != 0) &&
        (pVidPnPresentPath != NULL))
    {
        TempStatus = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPath);
        NT_ASSERT(NT_SUCCESS(TempStatus));
    }

    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::UpdateActiveVidPnPresentPath(_In_ CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* CONST pUpdateActiveVidPnPresentPath)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::UpdateActiveVidPnPresentPath");
    if (!(pUpdateActiveVidPnPresentPath != NULL)) { debug("[ASRT]: (pUpdateActiveVidPnPresentPath = NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::UpdateActiveVidPnPresentPath"); return STATUS_ABANDONED; }
    NTSTATUS Status = IsVidPnPathFieldsValid(&(pUpdateActiveVidPnPresentPath->VidPnPresentPathInfo));
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }
    // Mark the next present as fullscreen to make sure the full rotation comes through
    m_CurrentModes[pUpdateActiveVidPnPresentPath->VidPnPresentPathInfo.VidPnSourceId].Flags.FullscreenPresent = TRUE;
    m_CurrentModes[pUpdateActiveVidPnPresentPath->VidPnPresentPathInfo.VidPnSourceId].Rotation = pUpdateActiveVidPnPresentPath->VidPnPresentPathInfo.ContentTransformation.Rotation;
    return STATUS_SUCCESS;
}

//
// Private BDD DMM functions
//

NTSTATUS BASIC_DISPLAY_DRIVER::SetSourceModeAndPath(CONST D3DKMDT_VIDPN_SOURCE_MODE* pSourceMode,
                                                    CONST D3DKMDT_VIDPN_PRESENT_PATH* pPath)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::SetSourceModeAndPath");
    CURRENT_BDD_MODE* pCurrentBddMode = &m_CurrentModes[pPath->VidPnSourceId];
    NTSTATUS Status = STATUS_SUCCESS;
    pCurrentBddMode->Scaling = pPath->ContentTransformation.Scaling;
    pCurrentBddMode->SrcModeWidth = pSourceMode->Format.Graphics.PrimSurfSize.cx;
    pCurrentBddMode->SrcModeHeight = pSourceMode->Format.Graphics.PrimSurfSize.cy;
    pCurrentBddMode->Rotation = pPath->ContentTransformation.Rotation;

    if (!pCurrentBddMode->Flags.DoNotMapOrUnmap)
    {
        // Map the new frame buffer
        if (!(pCurrentBddMode->FrameBuffer.Ptr == NULL)) { debug("[ASRT]: (pCurrentBddMode->FrameBuffer.Ptr != NULL) | NTSTATUS BASIC_DISPLAY_DRIVER::SetSourceModeAndPath"); return STATUS_ABANDONED; }
        Status = MapFrameBuffer(pCurrentBddMode->DispInfo.PhysicAddress,
                                pCurrentBddMode->DispInfo.Pitch * pCurrentBddMode->DispInfo.Height,
                                &(pCurrentBddMode->FrameBuffer.Ptr));
    }
    if (NT_SUCCESS(Status))
    {

        pCurrentBddMode->Flags.FrameBufferIsActive = TRUE;
        BlackOutScreen(pPath->VidPnSourceId);
        // Mark that the next present should be fullscreen so the screen doesn't go from black to actual pixels one dirty rect at a time.
        pCurrentBddMode->Flags.FullscreenPresent = TRUE;
    }
    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::IsVidPnPathFieldsValid(CONST D3DKMDT_VIDPN_PRESENT_PATH* pPath) const
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::IsVidPnPathFieldsValid");

    if (pPath->VidPnSourceId >= MAX_VIEWS)
    {
        debug("[WARN]: VidPnSourceId is 0x%08lX is too high (MAX_VIEWS is 0x%08lX)", pPath->VidPnSourceId, MAX_VIEWS);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE;
    }
    else if (pPath->VidPnTargetId >= MAX_CHILDREN)
    {
        debug("[WARN]: VidPnTargetId is 0x%08lX is too high (MAX_CHILDREN is 0x%08lX)", pPath->VidPnTargetId, MAX_CHILDREN);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_TARGET;
    }
    else if (pPath->GammaRamp.Type != D3DDDI_GAMMARAMP_DEFAULT)
    {
        debug("[WARN]: pPath contains a gamma ramp (0x%08lX)", pPath->GammaRamp.Type);
        return STATUS_GRAPHICS_GAMMA_RAMP_NOT_SUPPORTED;
    }
    else if ((pPath->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY) &&
             (pPath->ContentTransformation.Scaling != D3DKMDT_VPPS_CENTERED) &&
             (pPath->ContentTransformation.Scaling != D3DKMDT_VPPS_NOTSPECIFIED) &&
             (pPath->ContentTransformation.Scaling != D3DKMDT_VPPS_UNINITIALIZED))
    {
        debug("[WARN]: pPath contains a non-identity scaling (0x%08lX)", pPath->ContentTransformation.Scaling);
        return STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED;
    }
    else if ((pPath->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY) &&
             (pPath->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE90) &&
             (pPath->ContentTransformation.Rotation != D3DKMDT_VPPR_NOTSPECIFIED) &&
             (pPath->ContentTransformation.Rotation != D3DKMDT_VPPR_UNINITIALIZED))
    {
        debug("[WARN]: pPath contains a not-supported rotation (0x%08lX)", pPath->ContentTransformation.Rotation);
        return STATUS_GRAPHICS_VIDPN_MODALITY_NOT_SUPPORTED;
    }
    else if ((pPath->VidPnTargetColorBasis != D3DKMDT_CB_SCRGB) &&
             (pPath->VidPnTargetColorBasis != D3DKMDT_CB_UNINITIALIZED))
    {
        debug("[WARN]: pPath has a non-linear RGB color basis (0x%08lX)", pPath->VidPnTargetColorBasis);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE;
    }
    else
    {
        return STATUS_SUCCESS;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::IsVidPnSourceModeFieldsValid(CONST D3DKMDT_VIDPN_SOURCE_MODE* pSourceMode) const
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::IsVidPnSourceModeFieldsValid");

    if (pSourceMode->Type != D3DKMDT_RMT_GRAPHICS)
    {
        debug("[WARN]: pSourceMode is a non-graphics mode (0x%08lX)", pSourceMode->Type);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE;
    }
    else if ((pSourceMode->Format.Graphics.ColorBasis != D3DKMDT_CB_SCRGB) &&
             (pSourceMode->Format.Graphics.ColorBasis != D3DKMDT_CB_UNINITIALIZED))
    {
        debug("[WARN]: pSourceMode has a non-linear RGB color basis (0x%08lX)", pSourceMode->Format.Graphics.ColorBasis);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE;
    }
    else if (pSourceMode->Format.Graphics.PixelValueAccessMode != D3DKMDT_PVAM_DIRECT)
    {
        debug("[WARN]: pSourceMode has a palettized access mode (0x%08lX)", pSourceMode->Format.Graphics.PixelValueAccessMode);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE;
    }
    else
    {
        for (UINT PelFmtIdx = 0; PelFmtIdx < ARRAYSIZE(gBddPixelFormats); ++PelFmtIdx)
        {
            if (pSourceMode->Format.Graphics.PixelFormat == gBddPixelFormats[PelFmtIdx])
            {
                return STATUS_SUCCESS;
            }
        }
        debug("[WARN]: pSourceMode has an unknown pixel format (0x%08lX)", pSourceMode->Format.Graphics.PixelFormat);
        return STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE;
    }
}

// Add more mode from the table.
struct SampleSourceMode
{
    UINT ModeWidth;
    UINT ModeHeight;
};

// The driver will advertise all modes that fit within the actual required mode (see AddSingleSourceMode below)
const static SampleSourceMode C_SampleSourceMode[] = {{800,600},{1024,768},{1152,864},{1280,800},{1280,1024},{1400,1050},{1600,1200},{1680,1050},{1920,1200}}; // <-- UTTER RUBBISH
const static UINT C_SampleSourceModeMax = sizeof(C_SampleSourceMode)/sizeof(C_SampleSourceMode[0]);

NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleSourceMode(_In_ CONST DXGK_VIDPNSOURCEMODESET_INTERFACE* pVidPnSourceModeSetInterface,
                                                   D3DKMDT_HVIDPNSOURCEMODESET hVidPnSourceModeSet,
                                                   D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleSourceMode");

    // There is only one source format supported by display-only drivers, but more can be added in a 
    // full WDDM driver if the hardware supports them
    for (UINT PelFmtIdx = 0; PelFmtIdx < ARRAYSIZE(gBddPixelFormats); ++PelFmtIdx)
    {
        // Create new mode info that will be populated
        D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo = NULL;
        NTSTATUS Status = pVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hVidPnSourceModeSet, &pVidPnSourceModeInfo);
        if (!NT_SUCCESS(Status))
        {
            // If failed to create a new mode info, mode doesn't need to be released since it was never created
            debug("[WARN]: pfnCreateNewModeInfo failed with Status = 0x%08lX, hVidPnSourceModeSet = %p", Status, hVidPnSourceModeSet);
            return Status;
        }

        // Populate mode info with values from current mode and hard-coded values
        // Always report 32 bpp format, this will be color converted during the present if the mode is < 32bpp
        debug("[INFO]: SourceId: %u", SourceId);
        pVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
        pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = m_CurrentModes[SourceId].DispInfo.Width;
        pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = m_CurrentModes[SourceId].DispInfo.Height;
        pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
        debug("[INFO]: PrimWidth: %u | PrimHeight: %u", pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx, pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy);
        debug("[INFO]: VisibleWidth: %u | VisibleHeight: %u", pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx, pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy);
        pVidPnSourceModeInfo->Format.Graphics.Stride = m_CurrentModes[SourceId].DispInfo.Pitch;
        debug("[INFO]: Pitch: %u (0x%04X)", m_CurrentModes[SourceId].DispInfo.Pitch, m_CurrentModes[SourceId].DispInfo.Pitch);
        debug("[INFO]: Stride: %u (0x%04X)", pVidPnSourceModeInfo->Format.Graphics.Stride, pVidPnSourceModeInfo->Format.Graphics.Stride);
        pVidPnSourceModeInfo->Format.Graphics.PixelFormat = gBddPixelFormats[PelFmtIdx]; // <-- NONSENSE.
        pVidPnSourceModeInfo->Format.Graphics.ColorBasis = D3DKMDT_CB_SCRGB;
        pVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;

        // Add the mode to the source mode set
        Status = pVidPnSourceModeSetInterface->pfnAddMode(hVidPnSourceModeSet, pVidPnSourceModeInfo);
        if (!NT_SUCCESS(Status))
        {
            // If adding the mode failed, release the mode, if this doesn't work there is nothing that can be done, some memory will get leaked
            NTSTATUS TempStatus = pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hVidPnSourceModeSet, pVidPnSourceModeInfo);
            UNREFERENCED_PARAMETER(TempStatus);
            NT_ASSERT(NT_SUCCESS(TempStatus));

            if (Status != STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
            {
                debug("[WARN]: pfnAddMode1 failed with Status = 0x%08lX, hVidPnSourceModeSet = %p, pVidPnSourceModeInfo = %p", Status, hVidPnSourceModeSet, pVidPnSourceModeInfo);
                return Status;
            }
        }
    }

    // Add all predefined modes that fit within the bounds of the required (POST) mode
    for (UINT ModeIndex = 0; ModeIndex < C_SampleSourceModeMax; ++ModeIndex)
    {
        // There is only one source format supported by display-only drivers, but more can be added in a 
        // full WDDM driver if the hardware supports them
        for (UINT PelFmtIdx = 0; PelFmtIdx < ARRAYSIZE(gBddPixelFormats); ++PelFmtIdx)
        {
            // Create new mode info that will be populated
            D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo = NULL;
            NTSTATUS Status = pVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hVidPnSourceModeSet, &pVidPnSourceModeInfo);
            if (!NT_SUCCESS(Status))
            {
                // If failed to create a new mode info, continuing to the next mode and trying again isn't going to be at all helpful, so return
                // Also, mode doesn't need to be released since it was never created
                debug("[WARN]: pfnCreateNewModeInfo failed with Status = 0x%08lX, hVidPnSourceModeSet = %p", Status, hVidPnSourceModeSet);
                return Status;
            }

            // Populate mode info with values from mode at ModeIndex and hard-coded values
            // Always report 32 bpp format, this will be color converted during the present if the mode at ModeIndex was < 32bpp
            pVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
            pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = C_SampleSourceMode[ModeIndex].ModeWidth; // <-- GIBBERISH
            pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = C_SampleSourceMode[ModeIndex].ModeHeight; // <-- GIBBERISH
            pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize; // <-- GIBBERISH
            pVidPnSourceModeInfo->Format.Graphics.Stride = 4*C_SampleSourceMode[ModeIndex].ModeWidth; // <-- GIBBERISH
            pVidPnSourceModeInfo->Format.Graphics.PixelFormat = gBddPixelFormats[PelFmtIdx]; // <-- GIBBERISH
            pVidPnSourceModeInfo->Format.Graphics.ColorBasis = D3DKMDT_CB_SCRGB;
            pVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;

            // Add the mode to the source mode set
            Status = pVidPnSourceModeSetInterface->pfnAddMode(hVidPnSourceModeSet, pVidPnSourceModeInfo);
            if (!NT_SUCCESS(Status))
            {
                if (Status != STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
                {
                    debug("[WARN]: pfnAddMode2 failed with Status = 0x%08lX, hVidPnSourceModeSet = %p, pVidPnSourceModeInfo = %p", Status, hVidPnSourceModeSet, pVidPnSourceModeInfo);
                }

                // If adding the mode failed, release the mode, if this doesn't work there is nothing that can be done, some memory will get leaked, continue to next mode anyway
                Status = pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hVidPnSourceModeSet, pVidPnSourceModeInfo);
                if (!(NT_SUCCESS(Status))) { debug("[ASRT]: (pVidPnSourceModeInfo) | NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleSourceMode"); return STATUS_ABANDONED; }
            }
        }
    }
    return STATUS_SUCCESS;
}

// Add the current mode information (acquired from the POST frame buffer) as the target mode.
NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleTargetMode(_In_ CONST DXGK_VIDPNTARGETMODESET_INTERFACE* pVidPnTargetModeSetInterface,
                                                   D3DKMDT_HVIDPNTARGETMODESET hVidPnTargetModeSet,
                                                   _In_opt_ CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnPinnedSourceModeInfo,
                                                   D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleTargetMode");
    D3DKMDT_VIDPN_TARGET_MODE* pVidPnTargetModeInfo = NULL;
    NTSTATUS Status = pVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hVidPnTargetModeSet, &pVidPnTargetModeInfo);
    if (!NT_SUCCESS(Status))
    {
        // If failed to create a new mode info, mode doesn't need to be released since it was never created
        debug("[WARN]: pfnCreateNewModeInfo failed with Status = 0x%08lX, hVidPnTargetModeSet = %p", Status, hVidPnTargetModeSet);
        return Status;
    }

    pVidPnTargetModeInfo->VideoSignalInfo.VideoStandard = D3DKMDT_VSS_OTHER;
    UNREFERENCED_PARAMETER(pVidPnPinnedSourceModeInfo);
    UNREFERENCED_PARAMETER(SourceId);
    pVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cx = m_CurrentModes[SourceId].DispInfo.Width;
    pVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy = m_CurrentModes[SourceId].DispInfo.Height;
    pVidPnTargetModeInfo->VideoSignalInfo.ActiveSize = pVidPnTargetModeInfo->VideoSignalInfo.TotalSize;
    pVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pVidPnTargetModeInfo->VideoSignalInfo.PixelRate = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pVidPnTargetModeInfo->VideoSignalInfo.ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;
    // We add this as PREFERRED since it is the only supported target
    pVidPnTargetModeInfo->Preference = D3DKMDT_MP_PREFERRED;

    Status = pVidPnTargetModeSetInterface->pfnAddMode(hVidPnTargetModeSet, pVidPnTargetModeInfo);
    if (!NT_SUCCESS(Status))
    {
        if (Status != STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
        {
            debug("[WARN]: pfnAddMode3 failed with Status = 0x%08lX, hVidPnTargetModeSet = %p, pVidPnTargetModeInfo = %p", Status, hVidPnTargetModeSet, pVidPnTargetModeInfo);
        }
        else
        {
            Status = STATUS_SUCCESS;
        }

        // If adding the mode failed, release the mode, if this doesn't work there is nothing that can be done, some memory will get leaked
        NTSTATUS TempStatus = pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hVidPnTargetModeSet, pVidPnTargetModeInfo);
        UNREFERENCED_PARAMETER(TempStatus);
        NT_ASSERT(NT_SUCCESS(TempStatus));
        return Status;
    }
    else
    {
        // If AddMode succeeded with something other than STATUS_SUCCESS treat it as such anyway when propagating up
        return STATUS_SUCCESS;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleMonitorMode(_In_ CONST DXGKARG_RECOMMENDMONITORMODES* CONST pRecommendMonitorModes)
{
    debug("[CALL]: NTSTATUS BASIC_DISPLAY_DRIVER::AddSingleMonitorMode");
    D3DKMDT_MONITOR_SOURCE_MODE* pMonitorSourceMode = NULL;
    NTSTATUS Status = pRecommendMonitorModes->pMonitorSourceModeSetInterface->pfnCreateNewModeInfo(pRecommendMonitorModes->hMonitorSourceModeSet, &pMonitorSourceMode);
    if (!NT_SUCCESS(Status))
    {
        // If failed to create a new mode info, mode doesn't need to be released since it was never created
        debug("[WARN]: pfnCreateNewModeInfo failed with Status = 0x%08lX, hMonitorSourceModeSet = %p", Status, pRecommendMonitorModes->hMonitorSourceModeSet);
        return Status;
    }

    D3DDDI_VIDEO_PRESENT_SOURCE_ID CorrespondingSourceId = FindSourceForTarget(pRecommendMonitorModes->VideoPresentTargetId, TRUE);

    // Since we don't know the real monitor timing information, just use the current display mode (from the POST device) with unknown frequencies
    pMonitorSourceMode->VideoSignalInfo.VideoStandard = D3DKMDT_VSS_OTHER;
    pMonitorSourceMode->VideoSignalInfo.TotalSize.cx = m_CurrentModes[CorrespondingSourceId].DispInfo.Width;
    pMonitorSourceMode->VideoSignalInfo.TotalSize.cy = m_CurrentModes[CorrespondingSourceId].DispInfo.Height;
    pMonitorSourceMode->VideoSignalInfo.ActiveSize = pMonitorSourceMode->VideoSignalInfo.TotalSize;
    pMonitorSourceMode->VideoSignalInfo.VSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pMonitorSourceMode->VideoSignalInfo.VSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pMonitorSourceMode->VideoSignalInfo.HSyncFreq.Numerator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pMonitorSourceMode->VideoSignalInfo.HSyncFreq.Denominator = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pMonitorSourceMode->VideoSignalInfo.PixelRate = D3DKMDT_FREQUENCY_NOTSPECIFIED;
    pMonitorSourceMode->VideoSignalInfo.ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;

    // We set the preference to PREFERRED since this is the only supported mode
    pMonitorSourceMode->Origin = D3DKMDT_MCO_DRIVER;
    pMonitorSourceMode->Preference = D3DKMDT_MP_PREFERRED;
    pMonitorSourceMode->ColorBasis = D3DKMDT_CB_SRGB;
    pMonitorSourceMode->ColorCoeffDynamicRanges.FirstChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.SecondChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.ThirdChannel = 8;
    pMonitorSourceMode->ColorCoeffDynamicRanges.FourthChannel = 8;

    Status = pRecommendMonitorModes->pMonitorSourceModeSetInterface->pfnAddMode(pRecommendMonitorModes->hMonitorSourceModeSet, pMonitorSourceMode);
    if (!NT_SUCCESS(Status))
    {
        if (Status != STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
        {
            debug("[WARN]: pfnAddMode4 failed with Status = 0x%08lX, hMonitorSourceModeSet = %p, pMonitorSourceMode = %p",
                            Status, pRecommendMonitorModes->hMonitorSourceModeSet, pMonitorSourceMode);
        }
        else
        {
            Status = STATUS_SUCCESS;
        }

        // If adding the mode failed, release the mode, if this doesn't work there is nothing that can be done, some memory will get leaked
        NTSTATUS TempStatus = pRecommendMonitorModes->pMonitorSourceModeSetInterface->pfnReleaseModeInfo(pRecommendMonitorModes->hMonitorSourceModeSet, pMonitorSourceMode);
        UNREFERENCED_PARAMETER(TempStatus);
        NT_ASSERT(NT_SUCCESS(TempStatus));
        return Status;
    }
    else
    {
        // If AddMode succeeded with something other than STATUS_SUCCESS treat it as such anyway when propagating up
        return STATUS_SUCCESS;
    }
}