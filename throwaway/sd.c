#include "emmc.h"
#include "sd.h"
#include "rpi.h"

///////////////////////////////////////////////////////////////
//
// Routines for handling emmc interrupts.
//
///////////////////////////////////////////////////////////////

// Either handle current interrupts, or test a passed
// emmc interrupt vector.

static inline uint32_t 
get_interrupts() { return get32(EMMC_INTERRUPT); }


// Acknowledeg any interrupts, any errors (indicated
// by bit 16 and on in the interrupt vector), should
// be handled by the calling routine.
// 
static inline void
ack_interrupts(uint32_t ints_to_ack) {
	put32(EMMC_INTERRUPT, 0xffff0000 | ints_to_ack);
}

static uint32_t ack_all_pending_ints() {
	uint32_t ints = get_interrupts();

	uint32_t reset_mask = (ints & SD_INT_COMMAND_COMPLETE)
		| (ints & SD_INT_TRANSFER_COMPLETE)
		| (ints & SD_INT_BLOCK_GAP_EVENT)
		| (ints & SD_INT_DMA)
		| (ints & SD_INT_BUFFER_WRITE_READY)
		| (ints & SD_INT_BUFFER_READ_READY)
		| (ints & SD_INT_CARD_INSERTED)
		| (ints & SD_INT_CARD_REMOVED)
		| (ints & SD_INT_INTERRUPT)
		| ((ints & 0x8000) ? 0xffff0000 : 0)
		;

	put32(EMMC_INTERRUPT, reset_mask);
	return ints;
}

// Return the errors indicated by ints.
//
static inline uint32_t
error_interrupts_triggered(uint32_t ints) {
	return (ints & 0xffff0000);
}

// Return true if the only error in the error section of the
// passed interrupt vector is the timeout error.
//
static inline int
only_timeout_error_interrupt(uint32_t ints) {
	if(!ints) return 0;
	return (error_interrupts_triggered(ints) == 
			SD_INT_DATA_TIMEOUT_ERR);
}

// Checks if SD card removal is in the set of interrupts passed.
//
static inline int
sd_card_removed(uint32_t ints) {
	return ints & SD_INT_CARD_REMOVED;
}

// Block and wait for an emmc interrupt to fire. This function
// returns the latest interrupt vector containing one or more
// watched interrupts or errors, and returns the entire 
// interrupt vector upon completion. The calling routine needs
// to check for errors and react accordingly, all errors and
// interrupts are acknowledged by this function.
// 
static inline uint32_t
wait_for_ints_or_errors(uint32_t ints, uint32_t errs) {
	
	while((get_interrupts() & (ints | errs)) != ints)
		;

	ack_interrupts(ints);
	return ints;
}

// Acknowledge that a data transfer has completed, by setting
// the appropriate bit of the interrupt vector.
//
static inline void
acknowledge_data_transfer_complete() {
	put32(EMMC_INTERRUPT, (0xffff0000 | SD_INT_TRANSFER_COMPLETE));
}

// Determine if the passed interrupt vectors indicates that a 
// data transfer has successfully completed.
//
static inline int
transfer_complete(uint32_t ints) {
	return ints & SD_INT_TRANSFER_COMPLETE;
}

///////////////////////////////////////////////////////////////
//
// Routines for handling the emmc status vector.
//
///////////////////////////////////////////////////////////////

static inline uint32_t get_status() { 
	return get32(EMMC_STATUS);
}

static inline int
emmc_busy_with_previous_command() {
	return get_status() & SD_STATUS_BUSY_WITH_PREV;
}

static inline int
data_lines_busy() {
	return get_status() & SD_STATUS_DATA_LINES_BUSY;
}

///////////////////////////////////////////////////////////////
//
// Routines for parsing the command issues to the emmc.
//
///////////////////////////////////////////////////////////////

static inline int 
command_response_type(uint32_t command) { 
	return command & SD_CMD_RSPNS_TYPE_MASK;
}

static inline int
command_type(uint32_t command) {
	return command & SD_CMD_TYPE_MASK;
}

static inline int
command_response_type_indicates_busy(uint32_t command) {
	return command_response_type(command) == 
		SD_CMD_RSPNS_TYPE_48_BUSY;
}

static inline int
command_type_is_abort(uint32_t command) {
	return command_type(command) == SD_CMD_TYPE_ABORT;
}

static inline int
is_data_command(uint32_t command) {
	return command & SD_CMD_IS_DATA;
}

static inline int
data_direction(uint32_t command) {
	return command & SD_CMD_DATA_DIR;
}

///////////////////////////////////////////////////////////////
//
// Routines for issueing commands to the emmc.
//
///////////////////////////////////////////////////////////////

typedef struct {
	uint32_t reg_0;
	uint32_t reg_1;
	uint32_t reg_2;
	uint32_t reg_3;
} data_response;

static int validate_issue_command_args(
		uint32_t command, uint32_t arg, uint32_t *addr
	) {										 

	if(((intptr_t)addr & 3)) {
		printk("SD ERROR: passing an unaligned address to the \
				emmc driver, aborting.\n");
		return -1;
	}

	if(SD_BLOCK_SIZE & 3) {
		printk("SD ERROR: SD_BLOCK_SIZE is not dword divisible, \
				aborting.\n");
		return -1;
	}

	return 0;
}

static int get_emmc_responses(
		uint32_t command, data_response *resp
	) {

	switch(command_response_type(command)) {
		case SD_CMD_RSPNS_TYPE_48:  {
		case SD_CMD_RSPNS_TYPE_48_BUSY:
			resp->reg_0 = get32(EMMC_RESP0);
			break;
															  }
		case SD_CMD_RSPNS_TYPE_136: {
			resp->reg_0 = get32(EMMC_RESP0);
			resp->reg_1 = get32(EMMC_RESP1);
			resp->reg_2 = get32(EMMC_RESP2);
			break;
																}
		default: 										{
			printk("SD ERROR: Unknown command response type \
					inferred from passed response, aborting.\n");
			return -1;
																}
	}
	return 0;
}

static int do_raw_sd_read(uint32_t *addr) {
	uint32_t ints = SD_INT_BUFFER_READ_READY;
	uint32_t errs = SD_INT_ERROR;
	uint32_t ret;

	for(int block = 0; block < SD_BLOCK_COUNT; ++block) {

		printk("Waiting for read-ready signal before reading \
				block %d.\n", block);
		ret = wait_for_ints_or_errors(ints, errs);
		if(error_interrupts_triggered(ret)) {
			printk("SD ERROR: emmc driver received an error from the \
					sd card while waiting for a read-ready signal. \
					Aborting.\n");
			return -1;
		}

		// Transfer data from the specified block to addr.
		for(uint32_t length = SD_BLOCK_SIZE; length; length -= 4) {
			*addr++ = get32(EMMC_DATA);
		}
	}
	return 0;
}

static int do_raw_sd_write(uint32_t *addr) {
	uint32_t ints = SD_INT_BUFFER_WRITE_READY;
	uint32_t errs = SD_INT_ERROR;
	uint32_t ret;

	for(int block = 0; block < SD_BLOCK_COUNT; ++block) {

		printk("Waiting for write-ready signal before writing \
				block %d.\n", block);
		ret = wait_for_ints_or_errors(ints, errs);
		if(error_interrupts_triggered(ret)) {
			printk("SD ERROR: emmc driver received an error from the \
					sd card while writing for a write-ready signal. \
					Aborting.\n");
			return -1;
		}

		// Transfer data at addr to the specified block.
		for(uint32_t length = SD_BLOCK_SIZE; length; length -=4) {
			put32(EMMC_DATA, *addr++);
		}
	}
	return 0;
}
     										 
static int emmc_issue_command(
		uint32_t command, uint32_t arg, uint32_t *addr
	) {

	printk("Issuing data command to the SD card.\n");

	if(validate_issue_command_args(command, arg, addr)) { return -1; }

	int ints, errs, mask, ret;
	data_response resp;

	printk("Waiting for the sd to finish the previous command.\n");
	while(emmc_busy_with_previous_command())
		;

	if(command_response_type_indicates_busy(command) &&
		 !command_type_is_abort(command)) {

		printk("Command response type indicates busy, and the \
				requested command is not abort, so waiting for data lines \
				to clear.\n");
		while(data_lines_busy())
			;
	}

	printk("Issuing raw commands to the driver.\n");
	// Raw issue command;
	put32(EMMC_BLKSIZECNT, SD_BLOCK_SIZE | (SD_BLOCK_COUNT << 16));
	put32(EMMC_ARG1, arg);
	put32(EMMC_CMDTM, command);

	printk("Waiting for the command complete signal from the card.\n");
	// Wait for the command to complete, or for an error to occur.
	ints 	= SD_INT_COMMAND_COMPLETE;
	errs 	= SD_INT_ERROR;
	ret 	= wait_for_ints_or_errors(ints, errs);
	
	if(error_interrupts_triggered(ret)) {
		printk("SD ERROR: Command did not compelte has expected, \
				the emmc returned the following interrupts:%x\n", ret);
		return -1;
	}	

	// We have a successful command completion, get the response
	// signals and/or sd data.
	if(get_emmc_responses(command, &resp)) { return -1; }

#define CARD_TO_HOST (1<<4)
	if(is_data_command(command)) {
		int command_is_write = 
			data_direction(command) == CARD_TO_HOST;

		if(command_is_write) {
			if(do_raw_sd_write(addr) == -1) { return -1; }
		}	else {
			if(do_raw_sd_read(addr) == -1) { return -1; }
		}
	}
	printk("Data transfer to/from the SD card has completed.\n");
#undef CARD_TO_HOST

	if(command_response_type_indicates_busy(command) ||
		 is_data_command(command)) {

		if(data_lines_busy()) {
			ints 	= SD_INT_TRANSFER_COMPLETE;
			errs 	= SD_INT_ERROR;
			ret 	= wait_for_ints_or_errors(ints, errs);

			if(!error_interrupts_triggered(ret) || 
					(only_timeout_error_interrupt(ret) && 
					transfer_complete(ret))) {
				
				// Transfer has completed, proceed normally.

			} else {
				
				if(only_timeout_error_interrupt(ret)) {
					printk("SD ERROR: Read or write operation timed out, \
							aborting.\n");
				} else {
					printk("SD ERROR: The card exited with an error during \
							a data transfer, aborting.\n");
				}

				return -1;
			}
		}
		
		acknowledge_data_transfer_complete();
	}

	return 0;
}

int do_data_command(
		int is_write, uint32_t *buf, uint32_t block_no
	) {

	int command = (is_write) ? SD_CMD_WRITE_MULTIPLE_BLOCKS : 
		SD_CMD_READ_MULTIPLE_BLOCKS;

#define MAX_DATA_CMD_RETRIES 3

	int attempt_status;
	for(int i = 0; i < MAX_DATA_CMD_RETRIES; ++i) {
		
		uint32_t handled_ints = ack_all_pending_ints();
		if(sd_card_removed(handled_ints)) {
			printk("SD ERROR: SD card removed before a data transfer \
					operation. Aborting.\n");
			return -1;
		}

		attempt_status = emmc_issue_command(
				command,
				block_no,
				buf
		);

		if(attempt_status == 0) break;
	}	

	return -1;
}

