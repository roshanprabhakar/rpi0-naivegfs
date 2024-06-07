#if 0
int main(int argc, char const **argv) {
	// First, get the character devices connected. We just assume these
	// are pies.
	int num_pies;
	struct dirent **devs = get_connected_pies(&num_pies);	
	if(num_pies == -1) { panic("Could not retrieve pies!\n"); return -1; }

	// Initialize some constants...
	unsigned baudrate = B115200;
	unsigned boot_addr = 0x8000; // Boot address for the pi0s.
	double timeout = 2*5; // In tenths of a second.

	int arg_iter = 1;
	char const *pi_prog_path = get_arg(&arg_iter, argc, argv);

	// Read the program to be deployed.
	int num_prog_bytes;
	void *prog = get_bytes(pi_prog_path,&num_prog_bytes);
	if(prog == NULL) {
		panic("Could not load program binary\n");
		free_pie_list(devs,num_pies);
		exit(0);
	}

	// Begin booting each of the pies.
	pi *pies = malloc(sizeof(pi) * num_pies);

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
			panic("Failed to open a pie!\n");
			goto end;
		}

		// Now that the pi is allocated and the fd is properly set up, 
		// jump to code that uses the established connection to perform
		// the boot protocol.
		if(boot(&pies[init_i], prog, num_prog_bytes) != 0) {
			panic("boot on pi<%d> failed, aborting all.\n", pies[init_i].fd);
			goto end;
		}
	}

	//..send commands, read commands, etc..

	// Now that all pies have been booted up successfully, start
	// reading their outputs.
	get_pi_output(pies, num_pies, STDOUT_FILENO);

end:
	for(int i = 0; i < init_i; ++i) {
		if(pies[i].fd != -1) close(pies[i].fd);
	}
	free_pie_list(devs,num_pies);	
	free(pies);
	free(prog);
	return 0;
}

