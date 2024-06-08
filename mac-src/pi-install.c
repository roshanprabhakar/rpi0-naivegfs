/**
 * This program finds all devices connected via usb, assumes they
 * are pis, and initiates the boot process on all of them. The
 * boot protocol was developed as a cs240lx lab, and is documented 
 * here:
 *
 * https://github.com/dddrrreee/cs140e-24win/blob/main/labs/7-bootloader/BOOTLOADER.md
 *
 * All connected pies run the pi-side of the boot protocol.
 **/

// For now, deplay the same program to all the pies.

#include "libmac.h"
#include "bootpi.h"
#include "dfs.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <sys/stat.h>

static pi *pies = NULL;
static int num_pies;

int boot_connected_pies(char const *pi_prog_path, char const *dfs_name,
		uint32_t *first_free_id, int *dirty_dfs) {

	int boot_failed = 0;

	// First, get the character devices connected. We just assume these
	// are pies.
	struct dirent **devs = get_connected_pies(&num_pies);	
	if(num_pies == -1) { panic("Could not retrieve pies!\n"); return -1; }

	// Initialize some constants...
	unsigned baudrate = B115200;
	unsigned boot_addr = 0x8000; // Boot address for the pi0s.
	double timeout = 2*5; // In tenths of a second.

	// Read the program to be deployed.
	int num_prog_bytes;
	void *prog = get_bytes(pi_prog_path,&num_prog_bytes);
	if(prog == NULL) {
		panic("Could not load program binary\n");
		free_pie_list(devs,num_pies);
		return -1;
	}

	// Begin booting each pi.
	pies = malloc(sizeof(pi) * num_pies);

	unsigned init_i = 0;
	char dev_wholename[32] = "/dev/"; // /dev/ + name + extra + \0
	for(; init_i < num_pies; ++init_i) {

		// Attempt to initialize char device to talk to pi.
		pies[init_i].name = devs[init_i]->d_name;
		strncpy(dev_wholename + 5, pies[init_i].name, 27);
	
		// Make 5 attempts to open tty file.
		pies[init_i].fd = -1;
		for(int i = 0; i < 5 && pies[init_i].fd == -1; ++i) {
			printf("[%d] attempting to open %s\n", i + 1, dev_wholename);
			pies[init_i].fd = open(dev_wholename, O_RDWR | O_NOCTTY | O_SYNC);
		}

		// Some more config needed to talk to the pi via char file.
		pies[init_i].fd = 
			set_tty_to_8n1(pies[init_i].fd, baudrate, timeout);

		if(pies[init_i].fd == -1) {
			printf("Failed to open a pie!\n");
			boot_failed = 1;
			goto end;
		}

		// Now that the pi is allocated and the fd is properly set up, 
		// jump to code that uses the established connection to perform
		// the boot protocol.
		if(boot(&pies[init_i], prog, num_prog_bytes) != 0) {
			printf("boot on pi<%d> failed, aborting all.\n", pies[init_i].fd);
			boot_failed = 1;
			goto end;
		}

		// Now that the pi has been booted successfully, perform the 
		// dfs init.

		int orig_len = strlen(dfs_name);
		int new_len = orig_len + 4;
		char *name_to_send = malloc(new_len + 1);
		strcpy(name_to_send, dfs_name);
		for(int i = 0; i < 4; ++i) {
			name_to_send[orig_len + i] = (char)0xff;
		}
		name_to_send[orig_len + 4] = 0;

		int saved_first_free_id = *first_free_id;
		if(dfs_boot(
					&pies[init_i], 
					name_to_send, 
					(new_len + 1)/4,
					first_free_id) != 0) {

			printf("dfs boot on pi<%d> failed, aborting all.\n", pies[init_i].fd);
			free(name_to_send);
			boot_failed = 1;
			goto end;
		}

		if(saved_first_free_id != *first_free_id)
			*dirty_dfs = 1;

		free(name_to_send);
	}

end:
	free(prog);
	free_pie_list(devs,num_pies);	
	if(!boot_failed) return 0;

	for(int i = 0; i < init_i; ++i) {
		if(pies[i].fd != -1) close(pies[i].fd);
	}
	free(pies);
	return -1;
}

// Returns the next id that may be allocated, or -1 if none.
int init_dfs(char const *name, struct dfs_file *df) {

	int fd = open(name, O_RDWR);

	struct stat s;
	if(fstat(fd, &s) == -1) {
		return -1;
	}

	if(s.st_size == 0) {
		memset(df, 0, sizeof(*df));
		write(fd, df, sizeof(*df));	
	} else {
		read(fd, df, sizeof(*df));
	}

	close(fd);

	return 0;
}

static inline char const *get_arg(int *i, int argc, char const **argv) {
	if (*i >= argc) { panic("Not enough arguments!\n"); exit(0); }
	return argv[(*i)++];
}

int main(int argc, char const **argv) {

	int arg_iter = 1;
	char const *pi_prog_path = get_arg(&arg_iter, argc, argv);
	char const *dfs_name = get_arg(&arg_iter, argc, argv);

	struct dfs_file dfs_data;
	if(init_dfs(dfs_name, &dfs_data) == -1) {
		printf("Failed to initialize dfs root.\n");
		return 0;
	}

	printf("First available id: %d\n", dfs_data.first_free_id);
	for(int i = 0; i < 13; ++i) {
		if(*(uint8_t *)&dfs_data.entries[i] == 0)
			break;
		else {
			for(int j = 0; j < 11; ++j) {
				printf("%c", dfs_data.entries[i].name[j]);
			}
			printf("\t%d\t%d\n",
					dfs_data.entries[i].device_no,
					dfs_data.entries[i].cluster_no);
		}
	}
	printf("Booting all connected pies.\n");

	int dirty_dfs = 0;
	int boot_successful = boot_connected_pies(
				pi_prog_path, 
				dfs_name,
				&dfs_data.first_free_id, 
				&dirty_dfs);

	printf("dfs after: first available id: %d\n", dfs_data.first_free_id);

	if(dirty_dfs) {
		printf("Writing back modified dfs to disk.\n");
		int fd = open(dfs_name, O_WRONLY);
		write(fd, &dfs_data, sizeof(dfs_data));
		close(fd);
	}

	if(boot_successful == -1) {
		printf("Failed to boot all connected pies, aborting.\n");
		return 0;
	}

	// Now that all pies have been booted up successfully, start
	// reading their outputs.
	get_pi_output(pies, num_pies, STDOUT_FILENO);

	for(int i = 0; i < num_pies; ++i) {
		if(pies[i].open) { close(pies[i].fd); }
	}
	free(pies);

	return 0;
}
