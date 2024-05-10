#include <stdio.h>
#include <sys/select.h>

#define PI_READ_TIMEOUT_SECS 5

/* Performs a round robin search of the booted raspberry pies,
 * querying each to see if data is available to return the mac.
 * If there is the descriptor is open and there is data, perform 
 * uninterrupted read from the selected pi, only terminating 
 * after 100 ms after the buffer is drained. Once all descriptors
 * are closed, return control to pi-install
 *
 * unifiedfd: probably stdout_fd
 */
void get_pi_output(pi *pies, int npies, int unifiedfd) {

	// Stop watching for reads after 10 seconds.
	struct timeval timeout = {
		.tv_sec = PI_READ_TIMEOUT_SECS,
		.tv_usec = 0
	};

	// Determine param needed for blocking select.
	int maxfd = -1;
	for(int i = 0; i < npies; ++i) {
		if(pies[i].fd > maxfd) { maxfd = pies[i].fd; }
	}

	fd_set readfds;
	int prev_pi = 0;

	while(1) {

		// Reset needed, confirmed by man select.
		// Select modifies this by the end of return to indicate which
		// fds are ready.
		FD_ZERO(&read_fds);
		for(int i = 0; i < npies; ++i) {
			FD_SET(pies[i].fd, &read_fds);
		}

		// Wait for a pi to generate data.
		int num_ready = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
		
		// If returned but none ready, terminate.
		if(num_ready == 0) {
			printf("%d seconds passed, no pi has responded, exiting.\n", PI_READ_TIMEOUT_SECS);
			return;
		}

		// Select the next pi to read from.
		int selected_pi = prev_pi + 1;
		for(; selected_pi != prev_pi; ++selected_pi) {
			if(selected_pi == npies) { selected_pi = 0; }	
			if(FD_ISSET(pies[selected_pi].fd, &read_fds)) { break; }
		}

		// Perform a loop of read + timeout until this fd writes no more data or
		// times out.
		// TODO
	}
}
