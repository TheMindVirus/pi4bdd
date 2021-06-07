    //////////////////////////////////////////////////////////////////////////////////////////////
   //                                                                                          //
  //                                  (V) WoR-Project 2021                                    //
 //                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

#include "mbox.hxx"

#define PI_PERI_BASE                            0xFE000000   // Low-Peripheral Mode
#define PI_PERI_LENGTH                           0x1000000

#define MBOX_DEFCONFIG                               0x400   // Shouldn't be anything else
#define MBOX_DEFCONFIG2                              0x401   // This can be 0x401 or 0x501
#define MBOX_SIZE                                     0x48
#define MBOX_MAX                                0x30000000
#define MBOX_MTU                                      1500   // MTU = 1500

#define MBOX_COUNT                                       4   // Number of Mailboxes present
#define MBOX_TESTS                                       3   // Re-runs

#define MBOX0_BASE                  (PI_PERI_BASE + 0xB880)  // MailboxHandle[0] 0x0
#define MBOX1_BASE                  (PI_PERI_BASE + 0xB980)  // MailboxHandle[1] 0x0
#define MBOX2_BASE                  (PI_PERI_BASE + 0xBA80)  // MailboxHandle[2] 0x0
#define MBOX3_BASE                  (PI_PERI_BASE + 0xBB80)  // MailboxHandle[3] 0x0

#define MBOX_READ                                        0   // 0x00
#define MBOX_PEEK                                        4   // 0x10
#define MBOX_SENDER                                      5   // 0x14 // Must not be set to 2
#define MBOX_STATUS                                      6   // 0x18
#define MBOX_CONFIG                                      7   // 0x1C
#define MBOX_WRITE                                       8   // 0x20
#define MBOX_PEEK2                                      12   // 0x30
#define MBOX_SENDER2                                    13   // 0x34
#define MBOX_STATUS2                                    14   // 0x38
#define MBOX_CONFIG2                                    15   // 0x3C

#define MBOX_RETRIES                                 65535   // times
#define MBOX_TIMEOUT                                   100   // microseconds

#define MBOX_REQUEST                            0x00000000
#define MBOX_EMPTY                              0x40000000
#define MBOX_FULL                               0x80000000
#define MBOX_FAILURE                            0x80000001
#define MBOX_SUCCESS                            0x80000000

#define mmio_read(Base, Offset)                 READ_REGISTER_ULONG((PULONG)(Base + Offset))
#define mmio_write(Base, Offset, Value)         WRITE_REGISTER_ULONG((PULONG)(Base + Offset), Value)

unsigned int mbox_setup(size_t Mailbox, unsigned char Channel);
#define mbox_peek(MailboxHandle)                mmio_read(MailboxHandle,  MBOX_STATUS)
#define mbox_read(MailboxHandle)                mmio_read(MailboxHandle,  MBOX_READ)
#define mbox_write(MailboxHandle, AddreCh)      mmio_write(MailboxHandle, MBOX_WRITE, AddreCh)
void mbox_get_mon_info(size_t Mailbox);

PULONG MailboxHandle[4] = { 0 };
__declspec(align(16)) volatile PULONG MailboxPacket[4] = { 0 };
PMDL MailboxMDL[4] = { 0 };

volatile int t1 = 0;
volatile int t2 = 0;
volatile int t3 = 0;

void main(void)
{
    debug("[CALL]: main");
    PHYSICAL_ADDRESS base;
    PHYSICAL_ADDRESS lowest;
    PHYSICAL_ADDRESS highest;
    PHYSICAL_ADDRESS skipbytes;
    LONGLONG bases[] = { MBOX0_BASE, MBOX1_BASE, MBOX2_BASE, MBOX3_BASE };

    for (size_t i = 0; i < MBOX_COUNT; ++i)
    {
        // Map MailboxHandle
        base.QuadPart = bases[i];
        MailboxHandle[i] = (PULONG)MmMapIoSpace(base, MBOX_SIZE, MmNonCached);
        if (!MailboxHandle[i]) { debug("[WARN]: Failed to Map MailboxHandle[%llu]", i); return; }

        // Allocate MailboxMDL
        lowest.QuadPart = 0x0;
        highest.QuadPart = MBOX_MAX;
        skipbytes.QuadPart = 0x0;
        MailboxMDL[i] = MmAllocatePagesForMdlEx(lowest, highest, skipbytes, MBOX_MTU, MmNonCached, MM_ALLOCATE_REQUIRE_CONTIGUOUS_CHUNKS);
        if (!MailboxMDL[i]) { debug("[WARN]: Failed to Allocate MailboxMDL[%llu]", i); return; }

        // Allocate MailboxPacket
        base.QuadPart = MBOX_MAX;
        MailboxPacket[i] = (PULONG)MmAllocateContiguousMemory(MBOX_MTU, base);
        if (!MailboxPacket[i]) { debug("[WARN]: Failed to Allocate MailboxPacket[%llu]", i); return; }
        for (size_t j = 0; j < (MBOX_MTU / 4); ++j) { MailboxPacket[i][j] = 0; }
    }

    for (size_t i = 0; i < MBOX_COUNT; ++i)
    {
        // Test Mailbox Memory
        for (size_t j = 0; j < MBOX_TESTS; ++j)
        {
            for (size_t k = 0; k < (MBOX_MTU / 4); ++k) { MailboxPacket[i][k] = 0; }
            mbox_get_mon_info(i);
            debug("[STAT]: Mailbox: %llu | Test: %llu | t1 = %d | t2 = %d | t3 = %d |", i, j, t1, t2, t3);
        }
    }

    for (size_t i = 0; i < MBOX_COUNT; ++i)
    {
        // Unmap Mailbox Memory
        MmFreeContiguousMemory(MailboxPacket[i]);
        MmFreePagesFromMdl(MailboxMDL[i]);
        MmUnmapIoSpace(MailboxHandle[i], MBOX_SIZE);
    }
}

unsigned int mbox_setup(size_t Mailbox, unsigned char Channel)
{
    debug("[CALL]: mbox_setup");
    if ((!MailboxHandle[Mailbox]) || (!MailboxMDL[Mailbox]) || (!MailboxPacket[Mailbox])) { debug("[MBOX]: Lock-up in Sector 7G, Sir"); return MBOX_FAILURE; }

    unsigned int checked = 0;
    unsigned int mail = (((unsigned int)MmGetPhysicalAddress(MailboxPacket[Mailbox]).QuadPart) & ~0xF) | (Channel & 0xF); // 0xF Reserved for 4-bit Channel
    debug("[MBOX]: mail = 0x%08lX", mail);

    t1 = 0;
    t2 = 0;
    t3 = 0;

    for (size_t i = 0; i <= MBOX_CONFIG2; ++i) { mmio_write(MailboxHandle[Mailbox], i, 0x0); }
    mmio_write(MailboxHandle[Mailbox], MBOX_CONFIG, MBOX_DEFCONFIG);
    mmio_write(MailboxHandle[Mailbox], MBOX_CONFIG2, MBOX_DEFCONFIG2);
    mmio_write(MailboxHandle[Mailbox], MBOX_READ, 0x0);

    for (size_t i = 0; i < (MBOX_SIZE / 4); ++i)
    {
        debug("[MBOX]: MailboxHandle[%llu] | Address: 0x%016llx | Value: 0x%08lX", i,
            MmGetPhysicalAddress(MailboxHandle[Mailbox] + i).QuadPart,
            mmio_read(MailboxHandle[Mailbox], i));
    }

    while ((mbox_peek(MailboxHandle[Mailbox]) & MBOX_FULL) != 0)
    {
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        ++t1; if (t1 > MBOX_RETRIES) { return MBOX_FAILURE; }
    }

    mbox_write(MailboxHandle[Mailbox], mail);

    while (1)
    {
        KeStallExecutionProcessor(MBOX_TIMEOUT);
        while ((mbox_peek(MailboxHandle[Mailbox]) & MBOX_EMPTY) != 0)
        {
            KeStallExecutionProcessor(MBOX_TIMEOUT);
            ++t2; if (t2 > MBOX_RETRIES) { return MBOX_FAILURE; }
        }

        checked = mbox_read(MailboxHandle[Mailbox]);
        if (mail == checked) { KeFlushIoBuffers(MailboxMDL[Mailbox], TRUE, TRUE); return MBOX_SUCCESS; }
        ++t3; if (t3 > MBOX_RETRIES) { return MBOX_FAILURE; }
    }
}

void mbox_get_mon_info(size_t Mailbox)
{
    unsigned int i = 1;
    unsigned int a = 0;
    MailboxPacket[Mailbox][i++] = MBOX_REQUEST;   // Mailbox Request

    MailboxPacket[Mailbox][i++] = 0x00040013;     // MBOX_TAG_GET_NUM_DISPLAYS
    MailboxPacket[Mailbox][i++] = 4;              // Buffer Length
    MailboxPacket[Mailbox][i++] = 4;              // Data Length
a=i;MailboxPacket[Mailbox][i++] = 0;              // Value

    MailboxPacket[Mailbox][i++] = 0;              // End Mark
    MailboxPacket[Mailbox][0] = i * 4;            // Update Packet Size
    
    if (MBOX_SUCCESS != mbox_setup(Mailbox, 8)) { debug("[WARN]: Mailbox Transaction Error"); }

    for (size_t j = 0; j < i; ++j)
    {
        debug("[MBOX]: MailboxPacket[%llu][%llu] | Value: 0x%08lX", Mailbox, j, MailboxPacket[Mailbox][j]);
    }
}

    //////////////////////////////////////////////////////////////////////////////////////////////
   //                                                                                          //
  //                                  (V) WoR-Project 2021                                    //
 //                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////