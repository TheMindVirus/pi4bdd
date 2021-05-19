#include "mbox.hxx"

#define PI_PERI_BASE                            0xFE000000   // Low-Peripheral Mode
#define PI_PERI_LENGTH                           0x1000000

#define MAILBOX_BASE                (PI_PERI_BASE + 0xB880)  // MailboxHandle 0x0
#define MAILBOX_LENGTH                                0x48

#define MAILBOX0_OFFSET_READ                             0   // 0x00 + 0xB880 + 0xFE000000
#define MAILBOX0_OFFSET_PEEK                             4   // 0x10 + 0xB880 + 0xFE000000
#define MAILBOX0_OFFSET_SENDER                           5   // 0x14 + 0xB880 + 0xFE000000
#define MAILBOX0_OFFSET_STATUS                           6   // 0x18 + 0xB880 + 0xFE000000
#define MAILBOX0_OFFSET_CONFIG                           7   // 0x1C + 0xB880 + 0xFE000000

#define MAILBOX1_OFFSET_WRITE                            8   // 0x20 + 0xB880 + 0xFE000000
#define MAILBOX1_OFFSET_PEEK                            12   // 0x30 + 0xB880 + 0xFE000000
#define MAILBOX1_OFFSET_SENDER                          13   // 0x34 + 0xB880 + 0xFE000000
#define MAILBOX1_OFFSET_STATUS                          14   // 0x38 + 0xB880 + 0xFE000000
#define MAILBOX1_OFFSET_CONFIG                          15   // 0x3C + 0xB880 + 0xFE000000

#define MAILBOX_MAX                             0x30000000
#define MAILBOX_SIZE                                  1500   // MTU = 1500

#define MAILBOX_RETRIES                              65535   // times

#define MAILBOX_REQUEST                         0x00000000
#define MAILBOX_EMPTY                           0x40000000
#define MAILBOX_FULL                            0x80000000
#define MAILBOX_FAILURE                         0x80000001
#define MAILBOX_SUCCESS                         0x80000000

#define mmio_read(Base, Offset)              READ_REGISTER_ULONG((PULONG)(Base + Offset))
#define mmio_write(Base, Offset, Value)      WRITE_REGISTER_ULONG((PULONG)(Base + Offset), Value)

unsigned int mailbox_setup(unsigned char Channel);
#define mailbox_peek()                       mmio_read(MailboxHandle,  MAILBOX0_OFFSET_STATUS)
#define mailbox_read()                       mmio_read(MailboxHandle,  MAILBOX0_OFFSET_READ)
#define mailbox_write(AddreCh)               mmio_write(MailboxHandle, MAILBOX1_OFFSET_WRITE, AddreCh)
void mbox_get_mon_info(void);

PULONG MailboxHandle = NULL;
__declspec(align(16)) volatile PULONG MailboxPacket = NULL;
PMDL MailboxMDL = NULL;

volatile int t1 = 0;
volatile int t2 = 0;
volatile int t3 = 0;

void main(void)
{
    debug("[CALL]: main");
    PHYSICAL_ADDRESS base;

    // Map MailboxHandle
    base.QuadPart = MAILBOX_BASE;
    MailboxHandle = (PULONG)MmMapIoSpace(base, MAILBOX_LENGTH, MmNonCached);
    if (!MailboxHandle) { debug("[WARN]: Failed to Map MailboxHandle"); return; }

    // Allocate MailboxPacket
    base.QuadPart = MAILBOX_MAX;
    MailboxPacket = (PULONG)MmAllocateContiguousMemory(MAILBOX_SIZE, base);
    if (!MailboxPacket) { debug("[WARN]: Failed to Allocate MailboxPacket"); MmUnmapIoSpace(MailboxHandle, MAILBOX_LENGTH); return; }
    for (int i = 0; i < (MAILBOX_SIZE / 4); ++i) { MailboxPacket[i] = 0; }

    // Allocate MailboxMDL
    PHYSICAL_ADDRESS lowest;
    PHYSICAL_ADDRESS highest;
    PHYSICAL_ADDRESS skipbytes;
    lowest.QuadPart = 0x0;
    highest.QuadPart = 0x30000000;
    skipbytes.QuadPart = 0x0;
    MailboxMDL = MmAllocatePagesForMdlEx(lowest, highest, skipbytes, MAILBOX_SIZE, MmNonCached, MM_ALLOCATE_REQUIRE_CONTIGUOUS_CHUNKS);
    if (!MailboxMDL) { debug("[WARN]: Failed to Allocate MailboxMDL"); MmFreeContiguousMemory(MailboxPacket); MmUnmapIoSpace(MailboxHandle, MAILBOX_LENGTH); return; }

    // Test Mailbox Memory
    mbox_get_mon_info(); debug("[STAT]: t1 = %d, t2 = %d, t3 = %d", t1, t2, t3);
    mbox_get_mon_info(); debug("[STAT]: t1 = %d, t2 = %d, t3 = %d", t1, t2, t3);
    mbox_get_mon_info(); debug("[STAT]: t1 = %d, t2 = %d, t3 = %d", t1, t2, t3);

    // Unmap Mailbox Memory
    MmFreeContiguousMemory(MailboxPacket);
    MmFreePagesFromMdl(MailboxMDL);
    MmUnmapIoSpace(MailboxHandle, MAILBOX_LENGTH);
}

unsigned int mailbox_setup(unsigned char Channel)
{
    debug("[CALL]: mailbox_setup");
    if ((!MailboxHandle) || (!MailboxMDL) || (!MailboxPacket)) { debug("[MBOX]: Lock-up in Sector 7G, Sir"); return MAILBOX_FAILURE; }

    unsigned int checked = 0;
    unsigned int mail = (((unsigned int)MmGetPhysicalAddress(MailboxPacket).QuadPart) & ~0xF) | (Channel & 0xF); // 0xF Reserved for 4-bit Channel
    debug("[MBOX]: mail = 0x%08lX", mail);

    t1 = 0;
    t2 = 0;
    t3 = 0;

    for (int i = 0; i <= MAILBOX0_OFFSET_CONFIG; ++i)
    {
        mmio_write(MailboxHandle, i, 0x0);
    }
    mmio_write(MailboxHandle, MAILBOX0_OFFSET_CONFIG, 0x400);
    mmio_write(MailboxHandle, MAILBOX0_OFFSET_READ, 0x0);

    for (unsigned int i = 0; i < (MAILBOX_LENGTH / 4); ++i)
    {
        debug("[MBOX]: MailboxHandle[%d] | Address: 0x%016llx | Value: 0x%08lX", i, MmGetPhysicalAddress(MailboxHandle + i).QuadPart, mmio_read(MailboxHandle, i));
    }

    while ((mailbox_peek() & MAILBOX_FULL) != 0)
    {
        KeStallExecutionProcessor(100);
        ++t1; if (t1 > MAILBOX_RETRIES) { return MAILBOX_FAILURE; }
    }

    mailbox_write(mail);

    while (1)
    {
        KeStallExecutionProcessor(100);
        while ((mailbox_peek() & MAILBOX_EMPTY) != 0)
        {
            KeStallExecutionProcessor(100);
            ++t2; if (t2 > MAILBOX_RETRIES) { return MAILBOX_FAILURE; }
        }

        checked = mailbox_read();
        if (mail == checked) { KeFlushIoBuffers(MailboxMDL, TRUE, TRUE); return MAILBOX_SUCCESS; }
        
        ++t3; if (t3 > MAILBOX_RETRIES) { return MAILBOX_FAILURE; }
    }
}

void mbox_get_mon_info(void)
{
    unsigned int i = 1;
    unsigned int a = 0;
    MailboxPacket[i++] = 0;          //Mailbox Request

    MailboxPacket[i++] = 0x00040013; //MBOX_TAG_GET_NUM_DISPLAYS
    MailboxPacket[i++] = 4;          //Buffer Length
    MailboxPacket[i++] = 4;          //Data Length
a=i;MailboxPacket[i++] = 0;          //Value

    MailboxPacket[i++] = 0;          //End Mark
    MailboxPacket[0] = i * 4;        //Update Packet Size

    if (MAILBOX_SUCCESS != mailbox_setup(8)) { debug("[WARN]: Mailbox Transaction Error"); }

    for (unsigned int j = 0; j < i; ++j)
    {
        debug("[MBOX]: MailboxPacket[%d] | Value: 0x%08lX", j, MailboxPacket[j]);
    }
}

    //////////////////////////////////////////////////////////////////////////////////////////////
   //                                                                                          //
  //                                  (V) WoR-Project 2021                                    //
 //                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////