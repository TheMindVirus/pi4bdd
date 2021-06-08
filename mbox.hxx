#ifndef _MBOX_HXX_
#define _MBOX_HXX_

//===================[Definitions from PiX-iES]=====================

#define STATIC   extern //static
#define REFER         *
#define PACK(ALIGNMENT)   alignas(ALIGNMENT)
#define MAX_SIZE   1500

#define ADDRESS   unsigned long long   // 64-bit
#define VALUE           unsigned int   // 32-bit
#define BYTE           unsigned char   //  8-bit
#define BUFFER            BYTE REFER   //  8-bit buffer
#define ELEMENT       volatile VALUE   // no-cache
#define POINTER        ELEMENT REFER   // no-cache pointer
#define MAILBOX        VALUE PACK(4)   // 4-byte aligned mailbox element
#define BALIGN         __declspec(align(16)) volatile PULONG // Madness

#define PERI_BASE      0xFE000000   // Low-Peripheral Mode
#define MBOX_BASE      0x0000B880 + PERI_BASE
#define MBOX_MAX       0x30000000   // Highest Valid Address
#define MBOX_SIZE            0x48
#define MBOX_MTU             1500   // MTU = 1500 bytes

#define MBOX_READ               0   // 0x00
#define MBOX_PEEK               4   // 0x10
#define MBOX_SENDER             5   // 0x14 // Must not be set to 2
#define MBOX_STATUS             6   // 0x18
#define MBOX_CONFIG             7   // 0x1C
#define MBOX_WRITE              8   // 0x20
#define MBOX_PEEK2             12   // 0x30
#define MBOX_SENDER2           13   // 0x34
#define MBOX_STATUS2           14   // 0x38
#define MBOX_CONFIG2           15   // 0x3C

#define MBOX_DEFCONFIG      0x400   // Shouldn't be anything else
#define MBOX_DEFCONFIG2     0x401   // This can be 0x401 or 0x501

#define MBOX_RETRIES        65535   // times
#define MBOX_TIMEOUT          100   // microseconds

#define MBOX_REQUEST   0x00000000
#define MBOX_EMPTY     0x40000000
#define MBOX_FULL      0x80000000
#define MBOX_SUCCESS   0x80000000
#define MBOX_FAILURE   0x80000001

//====================[Variables from PiX-iES]======================

STATIC ADDRESS   dev_framebuffer1;
STATIC VALUE     dev_pitchspace1;
STATIC VALUE     dev_width1;
STATIC VALUE     dev_height1;

STATIC ADDRESS   dev_framebuffer2;
STATIC VALUE     dev_pitchspace2;
STATIC VALUE     dev_width2;
STATIC VALUE     dev_height2;

STATIC ELEMENT   t1;
STATIC ELEMENT   t2;
STATIC ELEMENT   t3;

//=====================[Macros from PiX-iES]========================

//#define mmio_read(base, offset)           (REFER(POINTER)(base + offset))
//#define mmio_write(base, offset, value)   (REFER(POINTER)(base + offset) = value)

#define mmio_read(base, offset)           READ_REGISTER_ULONG((PULONG)(base + offset))
#define mmio_write(base, offset, value)   WRITE_REGISTER_ULONG((PULONG)(base + offset), value)

#define mbox_peek()                       mmio_read(mbox_handle, MBOX_STATUS)
#define mbox_read()                       mmio_read(mbox_handle, MBOX_READ)
#define mbox_write(addrech)               mmio_write(mbox_handle, MBOX_WRITE, addrech)

void mbox_mmio_setup();
void mbox_mmio_cleanup();

VALUE mbox_setup(BYTE channel);
VALUE mbox_get_num_displays();
void mbox_set_display(VALUE display);
void mbox_get_display_info();

//==================================================================

#endif//_MBOX_HXX_