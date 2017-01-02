/*
GPIO HALT: monitors a single GPIO pin, initiates orderly shutdown.
Similar functionality to the rpi_power_switch kernel module from the
fbtft project, but easier to compile (no kernel headers needed).

Connect button between any GND pin (there are several on the GPIO header)
and the GPIO pin of interest.  Internal pullup is used; no resistors needed.
By default GPIO21 is used; this and GND are the last pins on the Model B+
GPIO (40 pin) header, so it's very easy to plug in a button quick-connect.
Different pin can be specified on the command line or by editing the code.
Avoid pins 8 and 10; these are configured as a serial port by default on
most systems (this can be disabled but takes some doing).

To run automatically at startup, move the executable to /usr/local/bin and
edit /etc/rc.local, inserting this one line before the final 'exit 0':

/usr/local/bin/gpio-halt &

An alternate pin number can optionally be specified before the '&'

This is mostly just a pared-down 'retrogame' from the Cupcade project.

Written by Phil Burgess for Adafruit Industries, distributed under BSD
License.  Adafruit invests time and resources providing this open source
code, please support Adafruit and open-source hardware by purchasing
products from Adafruit!


Copyright (c) 2014 Adafruit Industries.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/mman.h>

// A few globals ---------------------------------------------------------

char
  *progName,                         // Program name (for error reporting)
   sysfs_root[] = "/sys/class/gpio", // Location of Sysfs GPIO files
   running      = 1;                 // Signal handler will set to 0 (exit)
int
   pin          = 228;                // Shutdown pin # (override w/argv)
volatile unsigned int
  *gpio;                             // GPIO register table
const int
   debounceTime = 20;                // 20 ms for button debouncing


// Some utility functions ------------------------------------------------

// Set one GPIO pin attribute through the Sysfs interface.
int pinConfig(char *attr, char *value) {
	char filename[50];
	int  fd, w, len = strlen(value);
	sprintf(filename, "%s/gpio%d/%s", sysfs_root, pin, attr);
	if((fd = open(filename, O_WRONLY)) < 0) return -1;
	w = write(fd, value, len);
	close(fd);
	return (w != len); // 0 = success
}

// Un-export Sysfs pins; don't leave filesystem cruft.  Write errors are
// ignored as pins may be in a partially-initialized state.
void cleanup() {
	char buf[50];
	int  fd;
	sprintf(buf, "%s/unexport", sysfs_root);
	if((fd = open(buf, O_WRONLY)) >= 0) {
		sprintf(buf, "%d", pin);
		write(fd, buf, strlen(buf));
		close(fd);
	}
}

// Quick-n-dirty error reporter; print message, clean up and exit.
void err(char *msg) {
	printf("%s: %s.  Try 'sudo %s'.\n", progName, msg, progName);
	cleanup();
	exit(1);
}

// Interrupt handler -- set global flag to abort main loop.
void signalHandler(int n) {
	running = 0;
}

// Main stuff ------------------------------------------------------------

int main(int argc, char *argv[]) {

	char                   buf[50],      // For sundry filenames
	                       c;            // Pin input value ('0'/'1')
	int                    fd,           // For mmap, sysfs, uinput
	                       timeout = -1, // poll() timeout
	                       pressed;      // Last-read pin state
	struct pollfd          p;            // GPIO file descriptor

	progName = argv[0];             // For error reporting
	signal(SIGINT , signalHandler); // Trap basic signals (exit cleanly)
	signal(SIGKILL, signalHandler);

	if(argc > 1) pin = atoi(argv[1]);

	// ----------------------------------------------------------------
	// All other GPIO config is handled through the sysfs interface.

	sprintf(buf, "%s/export", sysfs_root);
	if((fd = open(buf, O_WRONLY)) < 0) // Open Sysfs export file
		err("Can't open GPIO export file");
	sprintf(buf, "%d", pin);
	write(fd, buf, strlen(buf));  // Export pin
	pinConfig("active_low", "0"); // Don't invert
	// Set pin to input, detect rise+fall events
	if(pinConfig("direction", "in") ||
	   pinConfig("edge"     , "both"))
		err("Pin config failed");
	// Get initial pin value
	sprintf(buf, "%s/gpio%d/value", sysfs_root, pin);
	if((p.fd = open(buf, O_RDONLY)) < 0)
		err("Can't access pin value");
	pressed = 0;
	if((read(p.fd, &c, 1) == 1) && (c == '0')) pressed = 1;
	p.events  = POLLPRI; // Set up poll() events
	p.revents = 0;
	close(fd); // Done exporting

	// ----------------------------------------------------------------
	// Monitor GPIO file descriptor for button events.  The poll()
	// function watches for GPIO IRQs in this case; it is NOT
	// continually polling the pins!  Processor load is near zero.

	while(running) { // Signal handler can set this to 0 to exit
		// Wait for IRQ on pin (or timeout for button debounce)
		if(poll(&p, 1, timeout) > 0) { // If IRQ...
			if(p.revents) { // Event received?
				// Read current pin state, store in
				// 'pressed' state flag, but don't halt
				// yet -- must wait for debounce!
				lseek(p.fd, 0, SEEK_SET);
				read(p.fd, &c, 1);
				if(c == '0')      pressed = 1;
				else if(c == '1') pressed = 0;
				p.revents = 0; // Clear flag
			}
			timeout = debounceTime; // Set timeout for debounce
			// Else timeout occurred
		} else if(timeout == debounceTime) { // Button debounce timeout
			if(pressed) {
				(void)system("shutdown -h now");
				running = 0;
			}
		}
	}

	// ----------------------------------------------------------------
	// Clean up

	cleanup(); // Un-export pins

	puts("Done.");

	return 0;
}
