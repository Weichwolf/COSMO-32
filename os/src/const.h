// COSMO-32 Hardware Constants
// Immutable addresses and register definitions

#ifndef CONST_H
#define CONST_H

//----------------------------------------------------------------------
// Memory Map
//----------------------------------------------------------------------

#define FLASH_BASE      0x00000000
#define FLASH_SIZE      0x00040000  // 256KB

#define SRAM_BASE       0x20000000
#define SRAM_SIZE       0x00010000  // 64KB

#define PERIPH_BASE     0x40000000

#define FSMC_BASE       0x60000000
#define FSMC_SIZE       0x00100000  // 1MB
#define FRAMEBUF_OFFSET 0x000E0000  // 896KB offset
#define FRAMEBUF_ADDR   (FSMC_BASE + FRAMEBUF_OFFSET)

//----------------------------------------------------------------------
// Peripherals
//----------------------------------------------------------------------

#define USART1_BASE     0x40000000
#define SPI1_BASE       0x40010000
#define TIM1_BASE       0x40012000
#define I2S_BASE        0x40013000
#define DISPLAY_BASE    0x40018000
#define DMA1_BASE       0x40020000
#define ETH_BASE        0x40023000

#define TIMER_BASE      0xE0000000
#define PFIC_BASE       0xE000E000

//----------------------------------------------------------------------
// USART Registers (offset from USART1_BASE)
//----------------------------------------------------------------------

#define USART_STATR     0x00
#define USART_DATAR     0x04
#define USART_BRR       0x08
#define USART_CTLR1     0x0C

// USART_STATR bits
#define STATR_TXE       (1 << 7)
#define STATR_RXNE      (1 << 5)

// USART_CTLR1 bits
#define CTLR1_UE        (1 << 13)
#define CTLR1_TE        (1 << 3)
#define CTLR1_RE        (1 << 2)

//----------------------------------------------------------------------
// Timer Registers (offset from TIMER_BASE)
//----------------------------------------------------------------------

#define TIMER_MTIME     0x00
#define TIMER_MTIMECMP  0x08

//----------------------------------------------------------------------
// ETH MAC Registers (offset from ETH_BASE)
//----------------------------------------------------------------------

#define ETH_MACCR       0x00
#define ETH_MACSR       0x04
#define ETH_MACA0HR     0x08
#define ETH_MACA0LR     0x0C
#define ETH_DMAOMR      0x10
#define ETH_DMASR       0x14
#define ETH_DMATDLAR    0x18
#define ETH_DMARDLAR    0x1C
#define ETH_DMATPDR     0x20
#define ETH_DMARPDR     0x24

// ETH_MACCR bits
#define MACCR_TE        (1 << 0)
#define MACCR_RE        (1 << 1)

// ETH_DMAOMR bits
#define DMAOMR_SR       (1 << 0)
#define DMAOMR_ST       (1 << 1)

// ETH_DMASR bits
#define DMASR_TS        (1 << 0)
#define DMASR_RS        (1 << 1)

// TX Descriptor bits
#define TDES0_OWN       (1 << 31)
#define TDES0_LS        (1 << 29)
#define TDES0_FS        (1 << 28)
#define TDES0_TCH       (1 << 20)

// RX Descriptor bits
#define RDES0_OWN       (1 << 31)
#define RDES1_RCH       (1 << 14)

//----------------------------------------------------------------------
// Display Registers (offset from DISPLAY_BASE)
//----------------------------------------------------------------------

#define DISP_MODE       0x00
#define DISP_STATUS     0x04
#define DISP_PALETTE    0x40

// Display modes
#define DISP_MODE_640x400_4BPP   0
#define DISP_MODE_320x200_16BPP  1

//----------------------------------------------------------------------
// I2S Registers (offset from I2S_BASE)
//----------------------------------------------------------------------

#define I2S_CTRL        0x00
#define I2S_STATUS      0x04
#define I2S_DATA        0x08
#define I2S_CLKDIV      0x0C
#define I2S_BUFCNT      0x10

//----------------------------------------------------------------------
// DMA Registers (offset from DMA1_BASE)
//----------------------------------------------------------------------

#define DMA_ISR         0x00
#define DMA_IFCR        0x04
// Channel registers at 0x08 + n*0x14

//----------------------------------------------------------------------
// Network Buffer Layout (in SRAM)
//----------------------------------------------------------------------

#define NET_BUF_BASE    0x20002000

#define TX_DESC         0x20002000  // TX descriptor (16 bytes)
#define RX_DESC         0x20002100  // RX descriptor (16 bytes)
#define TX_BUF          0x20002200  // TX buffer (1.5KB)
#define RX_BUF          0x20002800  // RX buffer (2KB)
#define FILE_BUF        0x20003000  // TFTP file buffer
#define FILE_BUF_SIZE   0x4000      // 16KB

//----------------------------------------------------------------------
// Shell Buffer (in SRAM)
//----------------------------------------------------------------------

#define CMD_BUF         0x20001000
#define CMD_BUF_SIZE    128

//----------------------------------------------------------------------
// Stack (end of SRAM)
//----------------------------------------------------------------------

#define STACK_TOP       (SRAM_BASE + SRAM_SIZE)

//----------------------------------------------------------------------
// BASIC Interpreter Memory (in external SRAM)
//----------------------------------------------------------------------

#define BASIC_HEAP      0x600D0000  // BASIC heap in external SRAM
#define BASIC_HEAP_SIZE 0x00010000  // 64KB for arrays/strings

#endif // CONST_H
