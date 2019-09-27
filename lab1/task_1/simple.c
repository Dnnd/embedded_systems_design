#include <stdio.h>
#include <unistd.h>
#include "altera_avalon_pio_regs.h"
#include "system.h"  

#define NONE_PRESSED 0xF  // Value read from button PIO when no buttons pressed
#define DEBOUNCE 30000  // Time in microseconds to wait for switch debounce

#define BUTTON_1 7
#define BUTTON_2 11
#define BUTTON_3 13
#define BUTTON_4 14

int main(void) {
	int buttons;  // Use to hold button pressed value
	int led = 0x01;  // Use to write to led

	printf("Simple\n");   // print a message to show that program is running

	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, led); // write initial value to pio

	while (1) {
		buttons = IORD_ALTERA_AVALON_PIO_DATA(BUTTONS_BASE); // read buttons via pio
		if (buttons != NONE_PRESSED) {
			printf("%d\n", buttons);
		}
		if (buttons == NONE_PRESSED) {
			continue;
		} else if (buttons == BUTTON_1) {
			led = 0x80;
		} else if (buttons == BUTTON_4) {
			led = 0x1;
		} else if (buttons == BUTTON_2) {
			if (led >= 0x80) {
				led = 0x1;
			} else {
				led = led << 1;
			}
		} else if (buttons == BUTTON_3) {
			if (led <= 0x01) {
				led = 0x80;
			} else {
				led = led >> 1;
			}
		}
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, led); // write new value to pio

		/* Switch debounce routine
		 Wait for small delay after intial press for debouncing
		 Wait for release of key
		 Wait for small delay after release for debouncing */

		usleep(DEBOUNCE);
		while (buttons != NONE_PRESSED)  // wait for button release
			buttons = IORD_ALTERA_AVALON_PIO_DATA(BUTTONS_BASE);   // update
		usleep(DEBOUNCE);

	}

}  // end
