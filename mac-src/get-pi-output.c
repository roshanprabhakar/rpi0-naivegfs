#include "bootpi.h"

#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>

#define PI_READ_TIMEOUT_SECS 50

#define PI_READ_SIZE 4096

// It is possible that the exit string is split across
// the boundary between two reads. Thus, we need an internal
// state that is preserved across function calls.
//
// Specifically, we need to keep track of how many
// consecutive matches there are with the exit string.
//
// This function should scan every char in the read buffer.
int pi_done_scan(char c) {

	const char exit_string[] = "DONE!!!\n";

	static int idx = 0;
	
	if(c == exit_string[idx]) {
		++idx;
	} else if (c == exit_string[0]) {
		idx = 1;
	} else {
		idx = 0;
	}

	return idx == sizeof(exit_string) - 1;
}

void reap_pi(pi *pies, int selected_pi) {
	close(pies[selected_pi].fd);
	pies[selected_pi].open = 0;
}

/* Performs a round robin search of the booted raspberry pies,
 * querying each to see if data is available to return to the mac.
 * If there is the descriptor is open and there is data, perform 
 * uninterrupted read from the selected pi, only terminating 
 * after 50 ms after that buffer is drained. Once all descriptors
 * are closed, return control to pi-install
 *
 * unifiedfd: probably stdout_fd
 */
void get_pi_output(pi *pies, int npies, int unifiedfd) {

	printf("Dumping output from %d connected pies.\n", npies);

	// Stop watching for reads after 10 seconds.
	struct timeval all_pies_read_timeout = {
		.tv_sec = PI_READ_TIMEOUT_SECS,
		.tv_usec = 0
	};

	// Stop reading from a given pi after 50 ms.
	struct timeval single_pi_read_timeout = {
		.tv_sec = 0,
		.tv_usec = 50000
	};

	fd_set read_fds;
	unsigned char data[PI_READ_SIZE];
	int prev_pi = -1;
	int prev_fd = -1;

	for(int i = 0; i < 10; ++i) {

		// Reset needed, confirmed by man select.
		// Select modifies this by the end of return to indicate which
		// fds are ready.
		FD_ZERO(&read_fds);
		int maxfd = -1; 		// param needed for select.
		for(int i = 0; i < npies; ++i) {
			if(pies[i].open) {
				FD_SET(pies[i].fd, &read_fds);
				if(pies[i].fd > maxfd) maxfd = pies[i].fd;
			}
		}

		if(maxfd == -1) {
			printf("All pies have exited, shutting down.\n");
			break;	
		}

		// Wait for a pi to generate data.
		int num_ready = select(maxfd + 1, &read_fds, NULL, NULL, &all_pies_read_timeout);
		
		// If returned but none ready, terminate.
		if(num_ready == 0) {
			printf("%d seconds passed, no pi has responded, exiting.\n", PI_READ_TIMEOUT_SECS);
			break;
		}

		// Select the next pi to read from. Begin search right after previously
		// selected pi.

		int selected_pi = prev_pi;
		do {
			++selected_pi;
			if(selected_pi == npies) selected_pi = 0;
		} while(selected_pi != prev_pi && !pies[selected_pi].open);

		int fd = prev_fd = pies[selected_pi].fd;
		printf("Dumping data from pi<%d>\n", fd);

		// Drain this fd, then wait for a specified timeout. If data becomes available
		// before the timeout, flush again. Repeat indefinitely.
		while(1) {
			
			int n = read(fd, data, sizeof(data) - 1);

			if(n < 0) {
				printf("pi<%d> closed, cleaning up..\n", fd);
				reap_pi(pies, selected_pi);
				break;

			} else if(n == 0) {
				break;

			} else {
				data[n] = 0;
				
				// Check for done.
				int done = 0;
				for(unsigned i = 0; i < n; ++i) {
					if(pi_done_scan(data[i])) {
						done = 1;
						break;
					}
				}

				write(unifiedfd, data, n);

				if(done) {
					printf("DONE received from pi<%d>, cleaning up..\n", fd);
					reap_pi(pies, selected_pi);	
					break;	

				} else {

					// Set timeout on waiting for this pi to have more data ready.
					fd_set fs;
					FD_ZERO(&fs);
					FD_SET(fd, &fs);
					if(select(fd + 1, &fs, NULL, NULL, &single_pi_read_timeout) == 0) break;
				}
			}

		}
	}
}
