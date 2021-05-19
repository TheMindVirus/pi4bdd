#include "BDD.hxx"

//====================[Variables from PiX-iES]======================

PULONG    mbox_handle = NULL;
PMDL      mbox_middle = NULL;
BALIGN    mbox_packet = NULL;

ADDRESS   dev_framebuffer1 = 0x00000000;
VALUE     dev_pitchspace1 = 0x00000000;
VALUE     dev_width1 = 0x00000000;
VALUE     dev_height1 = 0x00000000;

ADDRESS   dev_framebuffer2 = 0x00000000;
VALUE     dev_pitchspace2 = 0x00000000;
VALUE     dev_width2 = 0x00000000;
VALUE     dev_height2 = 0x00000000;

ELEMENT   t1 = 0;
ELEMENT   t2 = 0;
ELEMENT   t3 = 0;

//=====================[MMIO port from PiX-iES]=====================

void mbox_mmio_setup()
{
    debug("[CALL]: void mbox_mmio_setup");
    PHYSICAL_ADDRESS base = {0};
    PHYSICAL_ADDRESS lowest = {0};
    PHYSICAL_ADDRESS highest = {0};

    // Map Mailbox Handle
    base.QuadPart = MBOX_BASE;
    mbox_handle = (PULONG)MmMapIoSpace(base, MBOX_SIZE, MmNonCached);
    if (!mbox_handle) { debug("[WARN]: Failed to Memory-Map Mailbox"); return; }

    // Map Mailbox Middle
    lowest.QuadPart = 0x0;
    highest.QuadPart = MBOX_MAX;
    base.QuadPart = 0x0;
    mbox_middle = MmAllocatePagesForMdlEx(lowest, highest, base, MBOX_MTU, MmNonCached, MM_ALLOCATE_REQUIRE_CONTIGUOUS_CHUNKS);
    if (!mbox_middle) { debug("[WARN]: Failed to Allocate MailboxMDL"); MmUnmapIoSpace(mbox_handle, MBOX_SIZE); return; }

    // Map Mailbox Packet
    highest.QuadPart = MBOX_MAX;
    mbox_packet = (PULONG)MmAllocateContiguousMemory(MAX_SIZE, highest);
    if (!mbox_packet) { debug("[WARN]: Failed to Memory-Map Mailbox Packet"); MmFreePagesFromMdl(mbox_middle); MmUnmapIoSpace(mbox_handle, MBOX_SIZE); return; }
}

void mbox_mmio_cleanup()
{
    debug("[CALL]: void mbox_mmio_cleanup"); //Same sequence as Map but in reverse
    if (mbox_packet)
    {
        MmFreeContiguousMemory(mbox_packet);
        mbox_packet = NULL;
    }
    if (mbox_middle)
    {
        MmFreePagesFromMdl(mbox_middle);
        mbox_middle = NULL;
    }
    if (mbox_handle)
    {
        MmUnmapIoSpace(mbox_handle, MBOX_SIZE);
        mbox_handle = NULL;
    }
}

//==================[Implementation from PiX-iES]===================

VALUE mbox_setup(BYTE channel)
{
    debug("[CALL]: VALUE mbox_setup");
    if ((!mbox_handle) || (!mbox_middle) || (!mbox_packet)) { debug("[MBOX]: Lock-up in Sector 7G"); return MBOX_FAILURE; }

    VALUE checked = 0;
    VALUE mail = (((VALUE)MmGetPhysicalAddress(mbox_packet).QuadPart) & ~0xF) | (channel & 0xF); //0xF reserved for 4-bit channel //<--Needs to be a physical address so the VC knows where it is
    debug("[MBOX]: physical = 0x%016llX", MmGetPhysicalAddress(mbox_packet).QuadPart);
    debug("[MBOX]: mail = 0x%016llX", mail);

    t1 = 0;
    t2 = 0;
    t3 = 0;

    for (int i = 0; i <= MBOX_CONFIG; ++i)
    {
        mmio_write(mbox_handle, i, 0x0);
    }
    mmio_write(mbox_handle, MBOX_CONFIG, 0x400);
    mmio_write(mbox_handle, MBOX_READ, 0x0);

    for (size_t i = 0; i < (MBOX_SIZE / 4); ++i)
    {
        debug("[MBOX]: mbox_handle[%d] | Address: 0x%016llX | Value: 0x%016llX",
            i, MmGetPhysicalAddress(mbox_handle + i), mmio_read(mbox_handle, i));
    }

    while ((mbox_peek() & MBOX_FULL) != 0)
    {
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        ++t1; if (t1 > MBOX_RETRIES) { return MBOX_FAILURE; }
    }

    mbox_write(mail);

    while (1)
    {
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        while ((mbox_peek() & MBOX_EMPTY) != 0)
        {
            KeStallExecutionProcessor(MBOX_TIMEOUT);
            ++t2; if (t2 > MBOX_RETRIES) { return MBOX_FAILURE; }
        }

        checked = mbox_read();
        if (mail == checked) { return MBOX_SUCCESS; }

        ++t3; if (t3 > MBOX_RETRIES) { return MBOX_FAILURE; }
    }
}

VALUE mbox_get_num_displays()
{
    debug("[CALL]: void mbox_get_num_displays");
    VALUE i = 1;
    VALUE a = 0;
    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x00040013;   //MBOX_TAG_GET_DISPLAY_NUM
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
a=i;mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS == mbox_setup(8)) { return mbox_packet[a]; }
    else { debug("[WARN]: Mailbox Transaction Error"); return 0; }
}

void mbox_set_display(VALUE display)
{
    debug("[CALL]: void mbox_set_display");
    VALUE i = 1;
    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x00048013;   //MBOX_TAG_SET_DISPLAY_NUM
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
    mbox_packet[i++] = display;      //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS != mbox_setup(8)) { debug("[WARN]: Mailbox Transaction Error"); }
}

VALUE mbox_get_vsync()
{
    debug("[CALL]: VALUE mbox_get_vsync");
    VALUE i = 1;
    VALUE a = 0;
    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x0004000e;   //MBOX_TAG_GET_VSYNC
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
a=i;mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS == mbox_setup(8)) { return mbox_packet[a]; }
    else { debug("[WARN]: Mailbox Transaction Error"); return 0; }
}

void mbox_set_vsync(VALUE enabled)
{
    debug("[CALL]: void mbox_set_vsync");
    VALUE i = 1;

    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x0004800e;   //MBOX_TAG_SET_VSYNC
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
    mbox_packet[i++] = enabled;      //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS != mbox_setup(8)) { debug("[WARN]: Mailbox Transaction Error"); }
}

void mbox_get_display_info()
{
    debug("[CALL]: void mbox_get_display_info");
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
    mbox_packet[i++] = dev_width1;   //Value // <-- TEMPORARY SOLUTION
    mbox_packet[i++] = dev_height1;  //Value // <-- TEMPORARY SOLUTION

    mbox_packet[i++] = 0x00048004;   //MBOX_TAG_SET_FB_VGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = dev_width1;   //Value // <-- TEMPORARY SOLUTION
    mbox_packet[i++] = dev_height1;  //Value // <-- TEMPORARY SOLUTION

    mbox_packet[i++] = 0x00040001;   //MBOX_TAG_ALLOC_FB
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
a=i;mbox_packet[i++] = 32;           //Value //Alignment
    mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0x00040008;   //MBOX_TAG_GET_FB_LINELENGTH
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
b=i;mbox_packet[i++] = 0;            //Value

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
    mbox_packet[i++] = dev_width2;   //Value // <-- TEMPORARY SOLUTION
    mbox_packet[i++] = dev_height2;  //Value // <-- TEMPORARY SOLUTION

    mbox_packet[i++] = 0x00048004;   //MBOX_TAG_SET_FB_VGEOM
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
    mbox_packet[i++] = dev_width2;   //Value // <-- TEMPORARY SOLUTION
    mbox_packet[i++] = dev_height2;  //Value // <-- TEMPORARY SOLUTION

    mbox_packet[i++] = 0x00040001;   //MBOX_TAG_ALLOC_FB
    mbox_packet[i++] = 8;            //Data Length (bytes)
    mbox_packet[i++] = 8;            //Data Length (again)
c=i;mbox_packet[i++] = 32;           //Value //Alignment
    mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0x00040008;   //MBOX_TAG_GET_FB_LINELENGTH
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
d=i;mbox_packet[i++] = 0;            //Value

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