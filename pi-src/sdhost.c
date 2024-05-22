#include "emmc.h"

#include <stdint.h>

// Handle any pending emmc interrupts.
// return value indicates which interrupts were handled.
static uint32_t handle_interrupts() {
	uint32_t ints = get_32(EMMC_INTERRUPT);

	uint32_t reset_mask = ints & SD_INT_COMMAND_COMPELTE
		| ints & SD_INT_TRANSFER_COMPLETE
		| ints & SD_INT_BLOCK_GAP_EVENT
		| ints & SD_INT_DMA
		| ints & SD_INT_BUFFER_WRITE_READY
		| ints & SD_INT_BUFFER_READ_READY
		| ints & SD_INT_CARD_INSERTED
		| ints & SD_INT_CARD_REMOVED
		| ints & SD_INT_INTERRUPT
		| ((ints & 0x8000) ? 0xffff0000 : 0)
		;

	put32(EMMC_INTERRUPT, reset_mask);
	return reset_mask;
}

void emmc_issue_command(uint32_t command, uint32_t arg) {
	uint32_t handled = handle_interrupts();

	// Exit early if the sd card was removed.
	if(handled & SD_CARD_REMOVAL) { return; }

	// Wait for previous command to finish.
	while(get32(EMMC_STATUS) & SD_STATUS_BUSY_WITH_PREV)
		;

	//
	// Wait appropriately until the required hardware resources
	// are available.
	//
	if(command & SD_CMD_RSPNS_TYPE_MASK == SD_CMD_RSPNS_TYPE_48_BUSY) {
		// Command type is with busy.

		if(command & SD_CMD_TYPE_MASK != SD_CMD_TYPE_ABORT) {

			// Not an abort command, wait for the data line to be free.
			while(get32(EMMC_STATUS) & SD_STATUS_DATA_LINES_BUSY)
				;
		}
	}	

	// Set block size and number of blocks to transfer.
	put32(EMMC_BLKSIZECNT, SD_BLOCK_SIZE | (SD_BLOCK_COUNT << 16));

	// Set arg1 reg to arg.
	put32(EMMC_ARG1, arg);

	// Issue command to command reg.
	put32(EMMC_CMDTM, command);

	// Wait for the command to complete.
	uint32_t ints;
	while((ints = get32(EMMC_INTERRUPT

	command &= 0xff;
}


int ensure_data_mode() {

}

int read(void *buf, uint32_t n) {

}
