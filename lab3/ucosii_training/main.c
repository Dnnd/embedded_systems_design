#include <stdio.h>
#include <system.h>
#include <unistd.h>
#include <string.h>
#include "includes.h"
#include "alt_ucosii_simple_error_check.h"
#include "altera_avalon_pio_regs.h"
#include "alt_types.h"
#include "sys/alt_irq.h"

/* Стеки задач */
#define   TASK_STACKSIZE       1024
OS_STK initialize_task_stk[TASK_STACKSIZE];
OS_STK print_status_task_stk[TASK_STACKSIZE];
OS_STK getsem_task1_stk[TASK_STACKSIZE];
OS_STK getsem_task2_stk[TASK_STACKSIZE];
OS_STK receive_task1_stk[TASK_STACKSIZE];
OS_STK receive_task2_stk[TASK_STACKSIZE];
OS_STK send_task_stk[TASK_STACKSIZE];

/* Приоритеты задач */
#define INITIALIZE_TASK_PRIORITY   6
#define PRINT_STATUS_TASK_PRIORITY 7
#define GETSEM_TASK1_PRIORITY      8
#define GETSEM_TASK2_PRIORITY      9
#define RECEIVE_TASK1_PRIORITY    10
#define RECEIVE_TASK2_PRIORITY    11
#define SEND_TASK_PRIORITY        12

#define TICKS_PER_MS (OS_TICKS_PER_SEC / 1000)
#define TICKS_PER_MINUTE (OS_TICKS_PER_SEC * 60)
#define TICKS_PER_HOUR (TICKS_PER_MINUTE * 60)

/* Объявление очереди */
#define   MSG_QUEUE_SIZE  30            //Size of message queue used in example
OS_EVENT *msgqueue;                    //Message queue pointer
void *msgqueueTbl[MSG_QUEUE_SIZE]; // Storage for messages

/* Объявление семафора */
OS_EVENT *shared_resource_sem;

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
		printf("The shared resource is owned by: %s\n",
				&sem_owner_task_name[0]);
		printf("\n");
		printf("The Number of times getsem_task1 acquired the semaphore %lu\n",
				getsem_task1_got_sem);
		printf("\n");
		printf("The Number of times getsem_task2 acquired the semaphore %lu\n",
				getsem_task2_got_sem);
		printf("\n");
		INT32U clk = OSTimeGet();
		INT32U hours = clk / TICKS_PER_HOUR;
		INT32U minutes = (clk % TICKS_PER_HOUR) / TICKS_PER_MINUTE;
		INT32U seconds = (clk % TICKS_PER_MINUTE) / OS_TICKS_PER_SEC;
		INT32U ms = (clk % OS_TICKS_PER_SEC) / TICKS_PER_MS;
		printf("Time: %d:%d:%d:%d\n", hours, minutes, seconds, ms);
		OS_Q_DATA data;
		INT8U err = OSQQuery(msgqueue, &data);
		if (err == OS_ERR_NONE) {
			printf("The number of messages in queue: %d\n", data.OSNMsgs);
		}

		printf(
				"****************************************************************\n");
		printf("\n");
	}
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

void send_task(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	INT32U msg = 0;
	OS_Q_DATA queue_data;

	while (1) {
		return_code = OSQQuery(msgqueue, &queue_data);
		alt_ucosii_check_return_code(return_code);
		if (queue_data.OSNMsgs < MSG_QUEUE_SIZE) {
			return_code = OSQPostOpt(msgqueue, (void *) &msg,
			OS_POST_OPT_BROADCAST);
			alt_ucosii_check_return_code(return_code);
			msg++;
			number_of_messages_sent++;
		} else {
			OSTimeDlyHMSM(0, 0, 1, 0);
		}
	}
}

void receive_task1(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	INT32U *msg;

	while (1) {
		msg = (INT32U *) OSQPend(msgqueue, 0, &return_code);
		alt_ucosii_check_return_code(return_code);
		number_of_messages_received_task1++;
		OSTimeDlyHMSM(0, 0, 0, 333);
	}
}

void receive_task2(void* pdata) {
	INT8U return_code = OS_NO_ERR;
	INT32U *msg;

	while (1) {
		msg = (INT32U *) OSQPend(msgqueue, 0, &return_code);
		alt_ucosii_check_return_code(return_code);
		number_of_messages_received_task2++;
		OSTimeDlyHMSM(0, 0, 1, 0);
	}
}



void initialize_task(void* pdata) {
	INT8U return_code = OS_NO_ERR;


	initOSDataStructs();

	initCreateTasks();


	return_code = OSTaskDel(OS_PRIO_SELF);
	alt_ucosii_check_return_code(return_code);
	while (1)
		;
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
	return_code = OSTaskCreate(print_status_task,
	NULL, (void *) &print_status_task_stk[TASK_STACKSIZE - 1],
	PRINT_STATUS_TASK_PRIORITY);
	alt_ucosii_check_return_code(return_code);
	return 0;
}
