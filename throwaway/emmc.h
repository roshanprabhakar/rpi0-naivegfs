#ifndef _EMMC_H_
#define _EMMC_H_
#include <stdint.h>

// This is the main interface to the micro SD card. Relevant
// info can be found from page 66 of the bcm2835 docs.
#define EMMC_BASE (0x20300000)
#define SD_BLOCK_SIZE 512
#define SD_BLOCK_COUNT 2

// Contains an argument for an SD card specific command acmd23.
// I'm reasonably sure this will be irrelevant.
#define EMMC_ARG2 (EMMC_BASE + 0x0)

// Must not be modified during data transfer. This reg contains
// the number and size in bytes for data blocks to be transferred.
// [31:16] - number of blocks to be transferred.
// [9:0] - block size in bytes.
#define EMMC_BLKSIZECNT (EMMC_BASE + 0x4)

// Contains an argument for all commands except for the sd card
// specific command acmd23.
#define EMMC_ARG1 (EMMC_BASE + 0x8)

// Issues commands to the SD card. It also contains flag data that
// informs the EMMC module what card response and type of data 
// transfer to expect.
//
// For data transfer 2 modes are supported:
// - Single block of data.
// - Several blocks of the same size.
//
// The host needs to be additionally configured using TM_MULTI_BLOCK.
// This bit must be set correctly based on the command sent to the
// card (1 for CMD[18|25], 0 for CMD[17|24]).
//
// If TM_BLKCNT_EN of this register is set, the transfer stops
// automatically after the number of blocks in BLKSIZECNT have
// been transferred.
//
// The TM_AUTO_CMD_EN bits can be used to make the host automatically
// send a command to the card telling it that the data transfer
// has finished once [31:16] in BLKSIZECNT have reached zero.
//
// [29:24] CMD_INDEX - index of the command to be issues to the card
#define SD_CMD_TYPE_MASK (3<<22)
// [23:22] CMD_TYPE - type of command to issue to the card:
//					- 00 = normal
#define SD_CMD_TYPE_NORMAL (0)
//					- 01 = suspend (the current data transfer)
#define SD_CMD_TYPE_SUSPEND (1<<22)
//					- 10 = resume (the last data transfer)
#define SD_CMD_TYPE_RESUME (2<<22)
//					- 11 = abort (the current data transfer)
#define SD_CMD_TYPE_ABORT (3<<22)
//
// [21] CMD_ISDATA - 0 indicates no data transfer command,
// 									 1 indicates is data transfer command.
#define SD_CMD_IS_DATA (1<<21)
// [20] - Check that response has same index as command.
// 					- 0 = disabled
// 					- 1 = enabled
// [19] - Check the response CRC
// 					- 0 = disabled
// 					- 1 = enabled
//
#define SD_CMD_RSPNS_TYPE_MASK (3<<16)
// [17:16] CMD_RSPNS_TYPE - Type of response expected from the card:
// 					- 00 = no response
#define SD_CMD_RSPNS_TYPE_NO_RESP (0)
// 					- 01 = 136 bits response
#define SD_CMD_RSPNS_TYPE_136 (1<<16)
// 					- 10 = 48 bits response
#define SD_CMD_RSPNS_TYPE_48 (2<<16)
// 					- 11 = 48 bits response using busy
#define SD_CMD_RSPNS_TYPE_48_BUSY (3<<16)
// 
// [5] TM_MULTI_BLOCK - Type of data transfer
// 					- 0 = single block
// 					- 1 = multi block
//
// [4] TM_DAT_DIR - Direction of data transfer
// 					- 0 = from host to card
// 					- 1 = from card to host
#define SD_CMD_DATA_DIR (1<<4)
//
// [3:2] TM_AUTO_CMD_EN - Select the command to be sent after 
// completion of a data transfer:
// 					- 00 = no command
// 					- 01 = command CMD12
// 					- 10 = command CMD23
// 					- 11 = reserved
//
// [1] TM_BLKCNT_EN - Enable the block counter for multiple block
// transfers
// 					- 0 = disabled
// 					- 1 = enabled
#define EMMC_CMDTM (EMMC_BASE + 0xc)

// EMMC_CMDTM values for reading and writing blocks.
#define SD_CMD_WRITE_MULTIPLE_BLOCKS   25
#define SD_CMD_WRITE_SINGLE_BLOCK      24
#define SD_CMD_READ_MULTIPLE_BLOCKS    18
#define SD_CMD_READ_SINGLE_BLOCK       17


// This register contains the status bits of the SD card's 
// response. This register is only valid once the last command 
// has completed and no new command has been issued.
#define EMMC_RESP0 (EMMC_BASE + 0x10)

// Only relevant for CMD2 and CMD10, otherwise ignore.
#define EMMC_RESP1 (EMMC_BASE + 0x14)

// Only relevant for CMD2 and CMD10, otherwise ignore.
#define EMMC_RESP2 (EMMC_BASE + 0x18)

// Only relevant for CMD2 and CMD10, otherwise ignore.
#define EMMC_RESP3 (EMMC_BASE + 0x1c)

// This register is used to transfer data to/from the card.
// Bit 1 of the INTERRUPT register can be used to check if data is
// available. For pased DMA transfers, the high active signal 
// dma_req can be used.
#define EMMC_DATA (EMMC_BASE + 0x20)

// Contains information inteded for debugging. It indicates the
// status of the card's hardware. Don't use this to poll for
// available data since it does not syncrhonize with the arm
// clock, instead use the INTERRUPT register. bit values can
// be found @ bcm2837.annot pg 72.
//
// [0] - indicates that the card is busy with the previous command.
#define SD_STATUS_BUSY_WITH_PREV 1
// [2] - indicates data line is being used by previous command.
#define SD_STATUS_DATA_LINES_BUSY 2
#define EMMC_STATUS (EMMC_BASE + 0x24)

// Used to configure the emmc module, all bits r/w.
// The only relevant bit seems to be [5] HCTL_8BIT, 
// which indicates whether 8 data lines are used.
#define EMMC_CONTROL0 (EMMC_BASE + 0x28)

// ...
#define EMMC_CONTROL1 (EMMC_BASE + 0x2c)

// This register should be polled to determine the status of the
// SD card. The individual event flags can be disabled with 
// IRPT_MASK. 
// [0] - Command has finished
#define SD_INT_COMMAND_COMPLETE 1
// [1] - Data transfer finished
#define SD_INT_TRANSFER_COMPLETE (1<<1)
// [2] - Data transfer has stopped at block gap
#define SD_INT_BLOCK_GAP_EVENT (1<<2)
// [3] - DMA
#define SD_INT_DMA (1<<3)
// [4] - Data can be written to the DATA register
#define SD_INT_BUFFER_WRITE_READY (1<<4)
// [5] - Data can be read from the DATA register
#define SD_INT_BUFFER_READ_READY (1<<5)
// [6] - SD card was inserted
#define SD_INT_CARD_INSERTED (1<<6)
// [7] - SD card was removed
#define SD_INT_CARD_REMOVED (1<<7)
// [8] - SD Card made interrupt request
#define SD_INT_INTERRUPT (1<<8)
// [9:11] - Reserved, don't care
// [12] - Clock retune request was made, don't care
// [13] - Boot acknowledged has been recieved
#define SD_INT_BOOT_ACK (1<<13)
// [14] - Boot operation has been terminated
#define SD_INT_BOOT_END (1<<14)
// [15] - An error has occurred
#define SD_INT_ERROR (1<<15)
// [16:19] - Used for CRC or Command line, don't care
// [20] - Timeout on data line
#define SD_INT_DATA_TIMEOUT_ERR (1<<20)
// [21] - Error CRC, don't care
#define SD_INT_CRC_ERR (1<<21)
// [22] - End bit on data line is not 1 error
#define SD_INT_DEND_ERR (1<<22)
// [23] - Reserved, don't care
// [24] - Auto command error
#define SD_INT_ACMD_ERR (1<<24)
// [31:25] - Reserved, don't care
#define EMMC_INTERRUPT (EMMC_BASE + 0x30)

#define EMMC_IRPT_MASK (EMMC_BASE + 0x34)
#define EMMC_IRPT_EN (EMMC_BASE + 0x38)
#define EMMC_CONTROL2 (EMMC_BASE + 0x3c)

#define EMMC_FORCE_IRPT (EMMC_BASE + 0x50)

#define EMMC_BOOT_TIMEOUT (EMMC_BASE + 0x70)
#define EMMC_DBG_SEL (EMMC_BASE + 0x74)

#define EMMC_EXRDFIFO_CFG (EMMC_BASE + 0x80)
#define EMMC_EXRDFIFO_EN (EMMC_BASE + 0x84)
#define EMMC_TUNE_STEP (EMMC_BASE + 0x88)
#define EMMC_TUNE_STEPS_STD (EMMC_BASE + 0x8c)
#define EMMC_TUNE_STEPS_DDR (EMMC_BASE + 0x90)
#define EMMC_SPI_INT_SPT (EMMC_BASE + 0xf0)
#define EMMC_SLOTISR_VER (EMMC_BASE + 0xfc)


// TODO prune everything below

// NOTE: Look into card_reset function, it looks like it does
// things such as SET_BLOCKLEN which might be lowkey important

#define CARD_RCA_INVALID	((uint32_t) 0xFFFF0000)

// Command definitions
#define APP_CMD                 55
#define GEN_CMD                 56
#define IS_APP_CMD              0x80000000
#define ACMD(a)                 (a | IS_APP_CMD)
#define SET_BUS_WIDTH           (6 | IS_APP_CMD)
#define SD_STATUS               (13 | IS_APP_CMD)
#define SEND_NUM_WR_BLOCKS      (22 | IS_APP_CMD)
#define SET_WR_BLK_ERASE_COUNT  (23 | IS_APP_CMD)
#define SD_SEND_OP_COND         (41 | IS_APP_CMD)
#define SET_CLR_CARD_DETECT     (42 | IS_APP_CMD)
#define SEND_SCR                (51 | IS_APP_CMD)

#define SD_CMD_ISDATA			(1 << 21)
#define SD_CMD_DAT_DIR_CH		(1 << 4)
#define SD_CMD_RESERVED(a)  0xffffffff
#define SD_CMD_INDEX(a)			((a) << 24)
#define SD_CMD_MULTI_BLOCK		(1 << 5)
#define SD_CMD_BLKCNT_EN		(1 << 1)
#define SD_CMD_AUTO_CMD_EN_CMD12	(1 << 2)
#define SD_CMD_DAT_DIR_HC		0

#define SD_CMD_CRCCHK_EN		(1 << 19)
#define SD_RESP_NONE        SD_CMD_RSPNS_TYPE_NONE
#define SD_RESP_R1          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R1b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R2          (SD_CMD_RSPNS_TYPE_136 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R3          SD_CMD_RSPNS_TYPE_48
#define SD_RESP_R4          SD_CMD_RSPNS_TYPE_136
#define SD_RESP_R5          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R5b         (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R6          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R7          (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)

#define SD_DATA_READ        (SD_CMD_ISDATA | SD_CMD_DAT_DIR_CH)
#define SD_DATA_WRITE       (SD_CMD_ISDATA | SD_CMD_DAT_DIR_HC)

#endif // _EMMC_H_
