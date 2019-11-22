#include <stdio.h>
#include <system.h>
#include <unistd.h>
#include <string.h>
#include "includes.h"
#include "alt_ucosii_simple_error_check.h"
#include "altera_avalon_pio_regs.h"

/* Стеки задач */
#define   TASK_STACKSIZE       1024
OS_STK initialize_task_stk[TASK_STACKSIZE];
OS_STK print_status_task_stk[TASK_STACKSIZE];
OS_STK receive_task1_stk[TASK_STACKSIZE];
OS_STK receive_task2_stk[TASK_STACKSIZE];
OS_STK send_task_stk[TASK_STACKSIZE];
OS_STK getsem_task1_stk[TASK_STACKSIZE];
OS_STK getsem_task2_stk[TASK_STACKSIZE];

/* Приоритеты задач */
#define INITIALIZE_TASK_PRIORITY   6
#define PRINT_STATUS_TASK_PRIORITY 7
#define GETSEM_TASK1_PRIORITY      8
#define GETSEM_TASK2_PRIORITY      9
#define RECEIVE_TASK1_PRIORITY    10
#define RECEIVE_TASK2_PRIORITY    11
#define SEND_TASK_PRIORITY        13

/* Объявление очереди */
#define   MSG_QUEUE_SIZE  30            //Size of message queue used in example
OS_EVENT *msgqueue;                    //Message queue pointer
void *msgqueueTbl[MSG_QUEUE_SIZE]; // Storage for messages

/* Объявление семафора */
OS_EVENT *shared_resource_sem;

OS_EVENT *button_interrupts_mbox;

/* Глобальные переменные */
INT32U number_of_messages_sent = 0;
INT32U number_of_messages_received_task1 = 0;
INT32U number_of_messages_received_task2 = 0;
INT32U getsem_task1_got_sem = 0;
INT32U getsem_task2_got_sem = 0;
char sem_owner_task_name[20];

/* Прототипы функций */
int initOSDataStructs(void);
int initCreateTasks(void);

void print_status_task(void* pdata) {
	while (1) {
		OSTimeDlyHMSM(0, 0, 3, 0);
		printf(
				"****************************************************************\n");
		printf(
				"Hello From MicroC/OS-II Running on NIOS II.  Here is the status:\n");
		printf("\n");
		printf("The number of messages sent by the send_task:         %lu\n",
				number_of_messages_sent);
		printf("\n");
		printf("The number of messages received by the receive_task1: %lu\n",
				number_of_messages_received_task1);
		printf("\n");
		printf("The number of messages received by the receive_task2: %lu\n",
				number_of_messages_received_task2);
		printf("\n");
		printf(
				"****************************************************************\n");
		printf("\n");
	}
}

static void sevenseg_set_hex(int hex) {
	static alt_u8 segments[16] = { 0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82,
			0xF8, 0x80, 0x90, /* 0-9 */
			0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E }; /* a-f */

	unsigned int data = segments[hex & 15] | (segments[(hex >> 4) & 15] << 8);

	IOWR_ALTERA_AVALON_PIO_DATA(SEVEN_SEG_BASE, data);
}

void send_task(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	OS_Q_DATA queue_data;
	while (1) {
		OSMboxPend(button_interrupts_mbox, 0, &return_code);
		if (return_code == OS_ERR_NONE) {
			INT32U data = IORD_ALTERA_AVALON_PIO_DATA(BUTTONS_BASE);
			return_code = OSQQuery(msgqueue, &queue_data);
			alt_ucosii_check_return_code(return_code);
			if (queue_data.OSNMsgs < MSG_QUEUE_SIZE) {
				return_code = OSQPostOpt(msgqueue, (void *) data,
				OS_POST_OPT_BROADCAST);
				alt_ucosii_check_return_code(return_code);
				number_of_messages_sent++;
			}
		}
	}
}

void shutdown_led(void *ptmr, void * args) {
	IOWR_ALTERA_AVALON_PIO_DATA(SEVEN_SEG_BASE, -1);

}

OS_TMR *timer = NULL;
void receive_task1(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	timer = OSTmrCreate(30, 0, OS_TMR_OPT_ONE_SHOT, shutdown_led,
						(void *) 0, "led shutdown", &return_code);
	while (1) {
		INT32U msg = (INT32U) OSQPend(msgqueue, 0, &return_code);
		if (return_code == OS_ERR_NONE) {
			OSTmrStop(timer, OS_TMR_OPT_NONE, (void *) 0, &return_code);
			sevenseg_set_hex(~msg & 0xf);
			OSTmrStart(timer, &return_code);
			number_of_messages_received_task1++;
			alt_ucosii_check_return_code(return_code);
		}
	}
}

#define HISTORY_SIZE 32
INT32U history[HISTORY_SIZE];

void receive_task2(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	INT32U msg;
	INT8U history_idx = 0;

	while (1) {
		msg = (INT32U) OSQPend(msgqueue, 0, &return_code);
		if (return_code == OS_ERR_NONE) {
			INT32U btn = ~msg & 0xf;
			if (btn == 1 || btn == 4 || btn == 8 || btn == 2) {

				printf("%d: button %d\n", history_idx, btn);
				history[history_idx] = btn;
				history_idx = (history_idx + 1) % HISTORY_SIZE;
			} else {
				for (INT8U i = 0; i < history_idx; ++i) {
					printf("%d: button %d\n", i, history[i]);
				}
				history_idx = 0;
			}
			alt_ucosii_check_return_code(return_code);
			number_of_messages_received_task2++;
		}
	}
}

void isr_button(void *context, alt_u32 id) {
	INT32U value;
	OSIntEnter();
	value = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	OS_EVENT *queue = (OS_EVENT*) context;
	OSMboxPost(queue, (void*) value);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0xf);
	OSIntExit();
}

void initialize_task(void* pdata) {
	INT8U return_code = OS_NO_ERR;

	initOSDataStructs();

	initCreateTasks();
	return_code = alt_ic_isr_register(BUTTONS_IRQ_INTERRUPT_CONTROLLER_ID,
	BUTTONS_IRQ, isr_button, (void *) button_interrupts_mbox, 0x00);
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0xf);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0x0);
	return_code = OSTaskDel(OS_PRIO_SELF);
	alt_ucosii_check_return_code(return_code);
	while (1)
		;
}

void getsem_task1(void* pdata) {
	INT8U return_code = OS_NO_ERR;

	while (1) {
		OSSemPend(shared_resource_sem, 0, &return_code);
		alt_ucosii_check_return_code(return_code);
		strcpy(&sem_owner_task_name[0], "getsem_task1");
		getsem_task1_got_sem++;
		OSSemPost(shared_resource_sem);
		OSTimeDlyHMSM(0, 0, 0, 100);
	}
}

void getsem_task2(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	INT8U led;
	led = 1;

	while (1) {
		led = led << 1;
		if (led == 0)
			led = 1;
		IOWR_ALTERA_AVALON_PIO_DATA(GREEN_LED_BASE, led);

		OSSemPend(shared_resource_sem, 0, &return_code);
		strcpy(&sem_owner_task_name[0], "getsem_task2");
		alt_ucosii_check_return_code(return_code);
		getsem_task2_got_sem++;
		OSSemPost(shared_resource_sem);
		OSTimeDlyHMSM(0, 0, 0, 130);
	}
}

int main(int argc, char* argv[], char* envp[]) {
	INT8U return_code = OS_NO_ERR;
	return_code = OSTaskCreate(initialize_task,
	NULL, (void *) &initialize_task_stk[TASK_STACKSIZE - 1],
	INITIALIZE_TASK_PRIORITY);
	alt_ucosii_check_return_code(return_code);
	OSStart();
	return 0;
}

int initOSDataStructs(void) {
	msgqueue = OSQCreate(&msgqueueTbl[0], MSG_QUEUE_SIZE);
	shared_resource_sem = OSSemCreate(1);
	button_interrupts_mbox = OSMboxCreate((void *) 0);
	return 0;
}

int initCreateTasks(void) {
	INT8U return_code = OS_NO_ERR;
	return_code = OSTaskCreate(getsem_task1,
	NULL, (void *) &getsem_task1_stk[TASK_STACKSIZE - 1],
	GETSEM_TASK1_PRIORITY);
	return_code = OSTaskCreate(getsem_task2,
	NULL, (void *) &getsem_task2_stk[TASK_STACKSIZE - 1],
	GETSEM_TASK2_PRIORITY);
	return_code = OSTaskCreate(receive_task1,
	NULL, (void *) &receive_task1_stk[TASK_STACKSIZE - 1],
	RECEIVE_TASK1_PRIORITY);
	alt_ucosii_check_return_code(return_code);
	return_code = OSTaskCreate(receive_task2,
	NULL, (void *) &receive_task2_stk[TASK_STACKSIZE - 1],
	RECEIVE_TASK2_PRIORITY);
	alt_ucosii_check_return_code(return_code);
	return_code = OSTaskCreate(send_task,
	NULL, (void *) &send_task_stk[TASK_STACKSIZE - 1],
	SEND_TASK_PRIORITY);
	alt_ucosii_check_return_code(return_code);
//	return_code = OSTaskCreate(print_status_task,
//	NULL, (void *) &print_status_task_stk[TASK_STACKSIZE - 1],
//	PRINT_STATUS_TASK_PRIORITY);
	alt_ucosii_check_return_code(return_code);
	return 0;
}
