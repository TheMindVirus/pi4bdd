#include "bdd.hxx"

//====================[Variables from PiX-iES]======================

PULONG    mbox_base = NULL;
PULONG    mbox_packet = NULL;

ADDRESS   dev_framebuffer1 = NULL;
VALUE     dev_pitchspace1 = 0;
VALUE     dev_width1 = 1920;
VALUE     dev_height1 = 1080;

ADDRESS   dev_framebuffer2 = NULL;
VALUE     dev_pitchspace2 = 0;
VALUE     dev_width2 = 1920;
VALUE     dev_height2 = 1080;

//=====================[MMIO port from PiX-iES]=====================

void mbox_mmio_setup()
{
    debug("[CALL]: void mbox_mmio_setup");
    PHYSICAL_ADDRESS base = {0};
    PHYSICAL_ADDRESS max = {0};

    base.QuadPart = MBOX_BASE;
    mbox_base = (PULONG)MmMapIoSpace(base, MBOX_SIZE, MmNonCached);
    if (!mbox_base) { debug("[WARN]: Failed to Memory-Map Mailbox"); }

    max.QuadPart = PERI_MASK;
    mbox_packet = (PULONG)MmAllocateContiguousMemory(MAX_SIZE, max);
    if (!mbox_packet) { debug("[WARN]: Failed to Memory-Map Mailbox Packet"); }
}

void mbox_mmio_cleanup()
{
    debug("[CALL]: void mbox_mmio_cleanup");
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

//==================[Implementation from PiX-iES]===================

VALUE mbox_setup(BYTE channel)
{
    debug("[CALL]: VALUE mbox_setup");
    if ((!mbox_base) || (!mbox_packet)) { debug("[MBOX]: Error 1"); return MBOX_FAILURE; }

    VALUE checked = 0;
    VALUE mail = ((((ADDRESS)MmGetPhysicalAddress(mbox_packet).QuadPart) & ~0xF) | (channel & 0xF)); //0xF reserved for 4-bit channel //<--Needs to be a physical address so the VC knows where it is
    debug("[MBOX]: physical = 0x%016llX", MmGetPhysicalAddress(mbox_packet).QuadPart);
    debug("[MBOX]: mail = 0x%016llX", mail);

    //KeEnterGuardedRegion();
    KeEnterCriticalRegion();

    mmio_write(mbox_base, MBOX_READ, 0x00000000);
    mmio_write(mbox_base, 1, 0x00000000);
    mmio_write(mbox_base, 2, 0x00000000);
    mmio_write(mbox_base, 3, 0x00000000);
    mmio_write(mbox_base, MBOX_POLL, 0x00000000);
    mmio_write(mbox_base, MBOX_SENDER, 0x00000000);
    //mmio_write(mbox_base, MBOX_STATUS, 0x40000000);
    mmio_write(mbox_base, MBOX_CONFIG, 0x00000400); // <-- may be all that's required
    mmio_write(mbox_base, MBOX_WRITE,  0x00000000);

    //mmio_write(mbox_base, MBOX_POLL, 0x00000001);
    //mmio_write(mbox_base, MBOX_CONFIG, 0x00000401);

    KeStallExecutionProcessor(MBOX_TIMEOUT);

    for (size_t i = 0; i < (MBOX_SIZE / 4); ++i)
    {
        debug("[MBOX]: mbox_base[%d] | Address: 0x%016llX | Value: 0x%016llX",
            i, MmGetPhysicalAddress(mbox_base + i), mmio_read(mbox_base, i));
    }

    VALUE timeout = 0;
    while ((mbox_peek() & MBOX_FULL) != 0)
    {
        debug("[HANG]: timeout");
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        ++timeout;
        if (timeout > MBOX_RETRIES) { debug("[MBOX]: Error 2"); goto failure; }
    }
    mbox_write(mail);
    KeStallExecutionProcessor(MBOX_TIMEOUT);

    VALUE timeout1 = 0;
    VALUE timeout2 = 0;
    while (1)
    {
        while ((mbox_peek() & MBOX_EMPTY) != 0)
        {
            debug("[HANG]: timeout2");
            KeStallExecutionProcessor(MBOX_TIMEOUT);
            ++timeout2;
            if (timeout2 > MBOX_RETRIES) { debug("[MBOX]: Error 3"); goto failure; }
        }

        checked = mbox_read();
        if (mail == checked) { goto success; }

        debug("[HANG]: timeout1");
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        ++timeout1;
        if (timeout1 > MBOX_RETRIES) { debug("[MBOX]: Error 4"); goto failure; }
    }

success:
    //KeLeaveGuardedRegion();
    KeLeaveCriticalRegion();
    return MBOX_SUCCESS;
failure:
    //KeLeaveGuardedRegion();
    KeLeaveCriticalRegion();
    return MBOX_FAILURE;
}

VALUE mbox_get_num_displays()
{
    debug("[CALL]: void mbox_get_num_displays");
    VALUE i = 1;
    VALUE a = 0;
    VALUE retval = 0;

    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x00040013;   //MBOX_TAG_GET_DISPLAY_NUM
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
a=i;mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS != mbox_setup(8)) { retval = mbox_packet[a]; }
    else { debug("[WARN]: Mailbox Transaction Error"); }
    return retval;
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
    VALUE retval = 0;

    mbox_packet[i++] = MBOX_REQUEST; //Mailbox Request

    mbox_packet[i++] = 0x0004000e;   //MBOX_TAG_GET_VSYNC
    mbox_packet[i++] = 4;            //Data Length (bytes)
    mbox_packet[i++] = 4;            //Data Length (again)
a=i;mbox_packet[i++] = 0;            //Value

    mbox_packet[i++] = 0;            //End Mark
    mbox_packet[0] = i * 4;          //Update Packet Size

    if (MBOX_SUCCESS == mbox_setup(8)) { retval = mbox_packet[a]; }
    else { debug("[WARN]: Mailbox Transaction Error"); }
    return retval;
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