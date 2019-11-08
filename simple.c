#include <alt_types.h>
#include <altera_avalon_performance_counter.h>
#include <altera_avalon_pio_regs.h>
#include <sys/alt_irq.h>
#include <system.h>

#define NONE_PRESSED 0xF  // Value read from button PIO when no buttons pressed
#define DEBOUNCE 30000  // Time in microseconds to wait for switch debounce

#define BUTTON_1 8
#define BUTTON_2 4
#define BUTTON_3 2
#define BUTTON_4 1
#define COUNTER_THRESHOLD 1

typedef struct buttons_ctx {
	int edge_capture;
	char is_updated;
} buttons_ctx;

volatile buttons_ctx buttons;

volatile int btn;

//static void  __attribute__ ((section (".tc_in_tcm"))) handle_button_interrupts(void* context, alt_u32 id);
static void handle_button_interrupts(void* context, alt_u32 id) {
	PERF_BEGIN(PERFORMANCE_COUNTER_0_BASE, 1);
    volatile buttons_ctx* buttons_ptr = (volatile buttons_ctx*) context;
	buttons_ptr->edge_capture = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	buttons_ptr->is_updated = 1;
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0xf);
	PERF_END(PERFORMANCE_COUNTER_0_BASE, 1);
}

static void init_button_pio() { /* Recast the edge_capture pointer to match the alt_irq_register() function prototype. */
	void* buttons_ctx_ptr = (void*) &buttons; /* Enable all 4 button interrupts. */
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0xf); /* Reset the edge capture register. */
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0x0); /* Register the ISR. */
#ifdef ALT_ENHANCED_INTERRUPT_API_PRESENT
	alt_ic_isr_register(BUTTONS_IRQ_INTERRUPT_CONTROLLER_ID, BUTTONS_IRQ,
			handle_button_interrupts, buttons_ctx_ptr, 0x0);
#else
	alt_irq_register( BUTTON_PIO_IRQ, edge_capture_ptr, handle_button_interrupts );
#endif
}

int main(void) {
	int led = 0x1;
	buttons.edge_capture = 0;
	buttons.is_updated = 0;
	unsigned counter = 0;
	PERF_RESET(PERFORMANCE_COUNTER_0_BASE);
	PERF_START_MEASURING(PERFORMANCE_COUNTER_0_BASE);
	IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, led);
	init_button_pio();
	while (1) {
		if (btn !=0)
		{
			buttons.is_updated = 1;
			buttons.edge_capture = btn;
			btn = 0;
		if (buttons.is_updated) {
			++counter;
			buttons.is_updated = 0;
			if (buttons.edge_capture == BUTTON_1) {
				led = 0x80;
			} else if (buttons.edge_capture == BUTTON_4) {
				led = 0x1;
			} else if (buttons.edge_capture == BUTTON_2) {
				if (led >= 0x80) {
					led = 0x1;
				} else {
					led = led << 1;
				}
			} else if (buttons.edge_capture == BUTTON_3) {
				if (led <= 0x01) {
					led = 0x80;
				} else {
					led = led >> 1;
				}
			}
			IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, led);
			if (counter == COUNTER_THRESHOLD) {
				PERF_STOP_MEASURING(PERFORMANCE_COUNTER_0_BASE);
				perf_print_formatted_report(PERFORMANCE_COUNTER_0_BASE,
						ALT_CPU_FREQ, 1, "Buttons Interrupt Handler");
				counter = 0;
				break;
			}
		}
		}
	}

}

