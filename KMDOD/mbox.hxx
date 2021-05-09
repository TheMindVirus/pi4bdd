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
#define BUFFER            BYTE REFER   // 8-bit buffer
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

//====================[Variables from PiX-iES]======================

STATIC PULONG   mbox_base;
//STATIC MAILBOX   mbox_packet[MAX_SIZE] PACK(16); //0xF reserved for 4-bit channel //16-packed mailbox
STATIC PULONG   mbox_packet;

STATIC ADDRESS   dev_framebuffer1;
STATIC VALUE     dev_pitchspace1;
//STATIC BUFFER    dev_edid1;        ///!!!EDID Reported by HDMI0 from Mailbox is Blank or Temperamental
STATIC VALUE     dev_width1;
STATIC VALUE     dev_height1;

STATIC ADDRESS   dev_framebuffer2;
STATIC VALUE     dev_pitchspace2;
//STATIC BUFFER    dev_edid2;        ///!!!EDID Reported by HDMI1 from Mailbox is Blank or Temperamental
STATIC VALUE     dev_width2;
STATIC VALUE     dev_height2;

//=====================[Macros from PiX-iES]========================

//#define mmio_read(base, offset)           (REFER(POINTER)(base + offset))
//#define mmio_write(base, offset, value)   (REFER(POINTER)(base + offset) = value)

#define mmio_read(base, offset)           (READ_REGISTER_ULONG((PULONG)(base + offset)))
#define mmio_write(base, offset, value)   (WRITE_REGISTER_ULONG((PULONG)(base + offset), value))

#define mbox_peek()                       (mmio_read(mbox_base, MBOX_STATUS))
#define mbox_read()                       (mmio_read(mbox_base, MBOX_READ))
#define mbox_write(addrech)               (mmio_write(mbox_base, MBOX_WRITE, addrech))

void mbox_mmio_setup();
void mbox_mmio_cleanup();

VALUE mbox_setup(BYTE channel);
void mbox_set_display(VALUE display);
VALUE mbox_get_vsync();
void mbox_set_vsync(VALUE enabled);
void mbox_get_display_info();

//==================================================================

#endif//_MBOX_HXX_