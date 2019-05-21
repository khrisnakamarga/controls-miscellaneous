/* Lab #<number> - <your name> */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h>
#include "DIIRQ.h"
#include "IRQConfigure.h"

/* prototypes */
void *DI_Irq_Thread(void* resource);
void wait(void);

/* definitions */
typedef struct {
	NiFpga_IrqContext irqContext; // IRQ context reserved
	NiFpga_Bool irqThreadRdy; 	  // IRQ thread ready flag
	uint8_t irqNumber;			  // IRQ number value
} ThreadResource;

//typedef struct {
//	uint32_t dioCount;			// count register
//	uint32_t dioIrqNumber;  	// number register
//	uint32_t dioIrqEnable;  	// enable register
//	uint32_t dioIrqRisingEdge; 	// rising edge-trig reg.
//	uint32_t dioIrqFallingEdge; // falling edge-trig reg.
//	Irq_Channel dioChannel; 	// supported I/O
//} MyRio_IrqDi;

/*---------------------------------------------------------------------
Function main()
Purpose: prints "count: " + the number of a counter which is incremented
		 every one second. Interrupt is implemented which will interrupt
		 the printing and prints "interrupt_"
*---------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	int i;
	int j;
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;


    //my code here
	ThreadResource irqThread0;
	pthread_t thread;
	MyRio_IrqDi irqDI0;
	// I. Configure the DI IRQ
	const uint8_t IrqNumber = 2;
	const uint32_t Count = 1;
	const Irq_Dio_Type TriggerType = Irq_Dio_FallingEdge;
	// Specify IRQ channel settings
	irqDI0.dioCount = IRQDIO_A_0CNT;
	irqDI0.dioIrqNumber = IRQDIO_A_0NO;
	irqDI0.dioIrqEnable = IRQDIO_A_70ENA;
	irqDI0.dioIrqRisingEdge = IRQDIO_A_70RISE;
	irqDI0.dioIrqFallingEdge = IRQDIO_A_70FALL;
	irqDI0.dioChannel = Irq_Dio_A0;
	// Initiate the IRQ number resource for new thread.
	irqThread0.irqNumber = IrqNumber;
	// Register DI0 IRQ. Terminate if not successful
	status = Irq_RegisterDiIrq( &irqDI0, &(irqThread0.irqContext),
								IrqNumber, Count, TriggerType);
	// Set the indicator to allow the new thread.
	irqThread0.irqThreadRdy = NiFpga_True;

	// Create new thread to catch IRQ
	status = pthread_create(&thread,
							NULL,
							DI_Irq_Thread,
							&irqThread0);
	for (i = 1; i < 61; i++) {
		for (j = 0; j < 200; j++) {
			wait();
		}
		printf_lcd("\fCount = %d", i); // prints count every second
	}

	// Terminates new thread after main.c is complete
	irqThread0.irqThreadRdy = NiFpga_False;
	status = pthread_join(thread,NULL);

	// Unregister interrupt
	int32_t Irq_UnregisterDiIrq(MyRio_IrqDi* irqChannel,
								 NiFpga_IrqContext irqContext,
								 uint8_t irqNumber);


	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*---------------------------------------------------------------------
Function DI_Irq_Thread()
Purpose: The interrupt thread function: prints "interrupt_" and waits
		 until the thread is called to exit.
Parameter: resouce = the resource number
*---------------------------------------------------------------------*/
void *DI_Irq_Thread(void* resource) {
	ThreadResource* threadResource = (ThreadResource*) resource;
	while (threadResource -> irqThreadRdy == NiFpga_True) {
		int j;
		uint32_t irqAssert = 0;
		Irq_Wait(threadResource -> irqContext,
				 threadResource -> irqNumber,
				 &irqAssert,
				 (NiFpga_Bool*) &(threadResource -> irqThreadRdy));
		if (irqAssert & (1 << threadResource->irqNumber)) {
			printf_lcd("\finterrupt_");
			Irq_Acknowledge(irqAssert);
		}
	}
	pthread_exit(NULL);
	return NULL;
}

/*-----------------------------------
Function wait
Purpose: waits for 5 ms.
Parameters: none
Returns: none
*-----------------------------------*/
void wait(void) {
	uint32_t i;
	i = 417000;
	while(i > 0){
		i--;
	}
	return;
}

