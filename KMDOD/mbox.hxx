#ifndef _MBOX_HXX_
#define _MBOX_HXX_

//===================[Definitions from PiX-iES]=====================

#define STATIC   extern //static
#define REFER         *
#define PACK(ALIGNMENT)   alignas(ALIGNMENT)
#define MAX_SIZE   1500

#define ADDRESS   unsigned long long   //64-bit
#define VALUE           unsigned int   //32-bit
#define BYTE           unsigned char   // 8-bit
#define ELEMENT       volatile VALUE   //no-cache
#define POINTER        ELEMENT REFER   //no-cache pointer
#define MAILBOX        VALUE PACK(4)   //4-byte aligned mailbox element

#define PERI_BASE      0xFE000000
#define PERI_MASK      0x3FFFFFFF
#define MBOX_BASE      0x0000B880 + PERI_BASE
#define MBOX_SIZE      0x24
#define MBOX_DATA      0x10000000

#define MBOX_READ      0 //0x00
#define MBOX_POLL      4 //0x10
#define MBOX_SENDER    5 //0x14
#define MBOX_STATUS    6 //0x18
#define MBOX_CONFIG    7 //0x1C
#define MBOX_WRITE     8 //0x20

#define MBOX_REQUEST   0x00000000
#define MBOX_EMPTY     0x40000000
#define MBOX_FULL      0x80000000
#define MBOX_SUCCESS   0x80000000
#define MBOX_FAILURE   0x80000001

#define MBOX_TIMEOUT   1000000 //microseconds
#define MBOX_RETRIES   10 //times

//=====================[Macros from PiX-iES]========================

//#define mmio_read(base, offset)           (REFER(POINTER)(base + offset))
//#define mmio_write(base, offset, value)   (REFER(POINTER)(base + offset) = value)

#define mmio_read(base, offset)           (READ_REGISTER_ULONG((PULONG)(base + offset)))
#define mmio_write(base, offset, value)   (WRITE_REGISTER_ULONG((PULONG)(base + offset), value))

#define mbox_peek()                       (mmio_read(mbox_base, MBOX_STATUS))
#define mbox_read()                       (mmio_read(mbox_base, MBOX_READ))
#define mbox_write(addrech)               (mmio_write(mbox_base, MBOX_WRITE, addrech))

//==================================================================

#endif//_MBOX_HXX_