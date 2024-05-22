#include "emmc.h"
#include "rpi.h"

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

// Return 0 on success, else on failure.
static int wait_for_ints_or_errs(uint32_t int_mask, int err_mask) {

	// Wait for any of the SD card interrupts specified in int_mask,
	// as well as errors specified in err_mask. Once we see one of 
	// these, check to see if any of the specific error flags have 
	// been set and, if so, return those flags.

	int ints;
	while(1) {
		ints = get32(EMMC_INTERRUPT);
		int b = ints & (int_mask | err_mask);
		if(b) break;
	}
	put32(EMMC_INTERRUPT, 0xffff0000 | int_mask);		
	return ints;
}

#define block_while(reg, flags) \
	while(get32(reg) & (flags));

#define block_until(reg, flags) \
	while((get32(reg) & (flags)) == 0);

#define response_type(c) (c & SD_CMD_RSPNS_TYPE_MASK)
#define cmd_type(c) (c & SD_CMD_TYPE_MASK)
#define busy SD_CMD_RSPNS_TYPE_48_BUSY

#define CARD_TO_HOST 1

static int check_err(int ret, int ints) {
	return (ret & (0xffff0000 | ints)) != ints;
}

static int emmc_issue_command_int(uint32_t command, 
																	 uint32_t arg, 
																	 uint32_t *addr) {
	uint32_t response_reg_0;
	uint32_t response_reg_1;
	uint32_t resopnse_reg_2;
	uint32_t response_reg_3;

	int ints, errs, mask, ret;

	if(((intptr_t)addr & 3) != 0) {
		printk("Passed an unaligned address to the EMMC driver, \
				aborting requested process.\n");
		return -1;
	}

	if((SD_BLOCK_SIZE & 3) != 0) {
		printk("SD host is using an imporper block size, aborting.\n");
		return -1;
	}

	// Wait until hardware resources are available.
	block_while(EMMC_STATUS, SD_STATUS_BUSY_WITH_PREV);

	if(response_type(command) == busy && cmd_type(command) != abort) {
		block_while(EMMC_STATUS, SD_STATUS_DATA_LINES_BUSY);
	}

	// Set block size and number of blocks to transfer.
	put32(EMMC_BLKSIZECNT, SD_BLOCK_SIZE | (SD_BLOCK_COUNT << 16));

	// Set arg1 reg to arg.
	put32(EMMC_ARG1, arg);

	// Issue command to command reg.
	put32(EMMC_CMDTM, command);

	// Wait for the command to complete, or an error to happen.
	ints = SD_INT_COMMAND_COMPLETE;
	errs = SD_INT_ERROR;
	ret = wait_for_ints_or_errs(ints, errs);

	if(check_err(ret, ints)) {
		printk("Encountered an error while waiting for command \
				complete signal.\n");
		return -1;
	}

	// Now we get the response data.
	switch(command & SD_CMD_RSPNS_TYPE_MASK) {
		case SD_CMD_RSPNS_TYPE_48:
		case SD_CMD_RSPNS_TYPE_48_BUSY:
			response_reg_0 = get32(EMMC_RESP0);
			break;
		case SD_CMD_RSPNS_TYPE_136:
			response_reg_0 = get32(EMMC_RESP0);
			response_reg_1 = get32(EMMC_RESP1);
			response_reg_2 = get32(EMMC_RESP2);
			response_reg_3 = get32(EMMC_RESP3);
			break;
	}

	// If with data, wait until the appropriate interrupt.
	if(command & SD_CMD_IS_DATA) {

		int int_to_watch;
		int is_write;
		if(command & SD_CMD_DATA_DIR == CARD_TO_HOST) {
			is_write = 0;
			int_to_watch = 1 << 5; // read
		} else {
			is_write = 1;
			int_to_watch = 1 << 4; // write
		}

		for(int block = 0; block < SD_BLOCK_COUNT; ++block) {

			errs = SD_INT_ERROR;
			ret = wait_for_ints_or_errs(int_to_watch, errs);
			if(check_err(ret, int_to_watch)) {
				printk("SD Host received an error while waiting for \
						a read ready or write ready signal. Aborting.\n");
				return -1;
			}

			// Transfer the block.
			uint32_t length = SD_BLOCK_SIZE;
			if(is_write) {
				for(; length > 0; length -= 4) {
					put32(EMMC_DATA, *addr++);
				}
			} else {
				for(; length > 0; length -= 4) {
					*addr++ = get32(EMMC_DATA);
				}
			}

			// Block transfer complete.
		}

		// Wait for transfer to complete (set if read/write transfer
		// or with busy).

		if(response_type(command) == busy || (command & SD_CMD_IS_DATA)) {
			
			// First, check that command inhibit is not already 0.
			if((get32(EMMC_STATUS) & SD_STATUS_DATA_LINES_BUSY) == 0) {

				put32(EMMC_STATUS, (0xffff0000 | SD_INT_TRANSFER_COMPLETE));

			} else {

				ints = SD_INT_TRANSFER_COMPLETE;
				errs = SD_INT_ERROR;
				ret = wait_for_ints_or_errs(ints, errs);

				if(check_err(ret, ints) && 
						check_err(ret, (ints | SD_INT_DATA_TIMEOUT_ERR))) {

					printk("SD host encoutnered an error while waiting for a \
							transfer complete or timeout signal. Aborting.\n");

					return -1;
				}

				put32(EMMC_INTERRUPT, (0xffff0000 | SD_INT_TRANSFER_COMPLETE));
			}
		}
	}

	return 0;
}

static int emmc_issue_command(uint32_t command, 
												uint32_t arg, 
												uint32_t *addr) {

	uint32_t handled_ints = handle_interrupts();
	if(handled_ints & SD_CARD_REMOVAL) {
		printk("SD card removed right before issue command, \
				aborting.\n");
		return -1;
	}

	return emmc_issue_command_int(command, arg, addr);
}

