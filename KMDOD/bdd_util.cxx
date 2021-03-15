#include "BDD.hxx"

//
// EDID validation
//

BOOLEAN IsEdidHeaderValid(_In_reads_bytes_(EDID_V1_BLOCK_SIZE) const BYTE* pEdid)
{
    debug("[CALL]: BOOLEAN IsEdidHeaderValid");
    static const UCHAR EDID1Header[8] = {0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0};
    return memcmp(pEdid, EDID1Header, sizeof(EDID1Header)) == 0;
}

BOOLEAN IsEdidChecksumValid(_In_reads_bytes_(EDID_V1_BLOCK_SIZE) const BYTE* pEdid)
{
    debug("[CALL]: BOOLEAN IsEdidChecksumValid");
    BYTE CheckSum = 0;
    for (const BYTE* pEdidStart = pEdid; pEdidStart < (pEdid + EDID_V1_BLOCK_SIZE); ++pEdidStart)
    {
        CheckSum += *pEdidStart;
    }
    return CheckSum == 0;
}

//
// Frame buffer map/unmap ///!!!NOTE
//

NTSTATUS MapFrameBuffer
(
    _In_                       PHYSICAL_ADDRESS    PhysicalAddress,
    _In_                       ULONG               Length,
    _Outptr_result_bytebuffer_(Length) VOID**      VirtualAddress
)
{
    debug("[CALL]: NTSTATUS MapFrameBuffer | FrameBuffer: 0x%08llX", PhysicalAddress);
    // Check for parameters
    if ((PhysicalAddress.QuadPart == (ULONGLONG)0) ||
        (Length == 0) ||
        (VirtualAddress == NULL))
    {
        debug("[WARN]: One of PhysicalAddress.QuadPart (0x%016llX), Length (0x%016llX), VirtualAddress (0x%016llX) is NULL or 0",
                        PhysicalAddress.QuadPart, Length, VirtualAddress);
        return STATUS_INVALID_PARAMETER;
    }

    *VirtualAddress = MmMapIoSpaceEx(PhysicalAddress, Length, PAGE_READWRITE | PAGE_WRITECOMBINE);
    if (*VirtualAddress == NULL)
    {
        // The underlying call to MmMapIoSpace failed. This may be because, MmWriteCombined
        // isn't supported, so try again with MmNonCached
        *VirtualAddress = MmMapIoSpaceEx(PhysicalAddress,
                                       Length,
                                       PAGE_READWRITE | PAGE_NOCACHE);
        if (*VirtualAddress == NULL)
        {
            debug("[WARN]: MmMapIoSpace returned a NULL buffer when trying to allocate 0x%016llX bytes", Length);
            return STATUS_NO_MEMORY;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS UnmapFrameBuffer
(
    _In_reads_bytes_(Length) VOID* VirtualAddress,
    _In_                ULONG Length
)
{
    debug("[CALL]: NTSTATUS UnmapFrameBuffer");
    // Check for parameters
    if ((VirtualAddress == NULL) && (Length == 0))
    {
        // Allow this function to be called when there's no work to do, and treat as successful
        return STATUS_SUCCESS;
    }
    else if ((VirtualAddress == NULL) || (Length == 0))
    {
        debug("[WARN]: Only one of Length (0x%016llX), VirtualAddress (0x%016llX) is NULL or 0", Length, VirtualAddress);
        return STATUS_INVALID_PARAMETER;
    }
    MmUnmapIoSpace(VirtualAddress, Length);
    return STATUS_SUCCESS;
}

//
// DEBUG
//

void DebugLogToFile(LPSTR format, ...)
{
    //NTSTATUS status = STATUS_SUCCESS;

    char buffer[255] = "";
    va_list va;
    va_start(va, format);
    vsprintf(buffer, format, va);
    va_end(va);
    strcat(buffer, "\n");

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, buffer));
    /*
    HANDLE file;
    UNICODE_STRING path;
    WCHAR pathbuffer[1024];
    IO_STATUS_BLOCK result;
    OBJECT_ATTRIBUTES attributes;
    
    wcscpy(pathbuffer, DEBUG_FILEROOT);
    wcscat(pathbuffer, DEBUG_FILENAME);
    RtlInitUnicodeString(&path, pathbuffer);
    InitializeObjectAttributes(&attributes, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    status = ZwCreateFile(&file, FILE_APPEND_DATA, &attributes, &result, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
        (_DebugOverwrite) ? FILE_OVERWRITE_IF : FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(status)) { return; }
    _DebugOverwrite = FALSE;

    status = ZwWriteFile(file, NULL, NULL, NULL, &result, buffer, (ULONG)strlen(buffer), NULL, NULL);
    for (ULONG i = 0; i < 10; ++i)
    {
        if (result.Status != STATUS_PENDING) { break; }
        KeStallExecutionProcessor(100);
    }
    ZwClose(file);
    */
}
