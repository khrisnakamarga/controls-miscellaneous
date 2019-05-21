/* Lab #6 - Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h>
#include "TimerIRQ.h" // TimerIRQ interrupt
#include "IRQConfigure.h"
#include "matlabfiles.h"

#define SATURATE(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
struct biquad {
	double b0; double b1; double b2; // numerator
	double a0; double a1; double a2; // denominator
	double x0; double x1; double x2; // input
	double y1; double y2; }; // output
/* prototypes */
void *Timer_Irq_Thread(void* resource);
double cascade(double xin, // input
		struct biquad *fa, // biquad array
		int ns, // no. segments
		double ymin, // min output
		double ymax); // max output

/* definitions */
// globally create the ThreadResource Structure
typedef struct {
	NiFpga_IrqContext irqContext; // IRQ context reserved
	NiFpga_Bool irqThreadRdy; // IRQ thread ready flag
} ThreadResource;

MyRio_Aio CI0;
MyRio_Aio CO0;
uint32_t timeoutValue;
double VADin;
double VADout;
double hiSat; // maximum saturation voltage
double lowSat; // minimum saturation voltage
NiFpga_Session myrio_session;

// MATLAB Data
#define IMAX 500 // maximum data points
static double ibuffer[IMAX]; // output voltage buffer
static double obuffer[IMAX]; // input voltage buffer
static double *opt = obuffer; // output voltage buffer pointer
static double *ipt = ibuffer; // input voltage buffer pointer
MATFILE *mf;
// MATLAB Data

/*---------------------------------------------------------------------
Function main()
Purpose: creates the ISR thread that acts as a butterworth filter that
		 outputs a signal on Ch0. Waits for the user to press the DEL
		 key that terminates both main and the ISR thread.
*---------------------------------------------------------------------*/
int main(int argc, char **argv) {
	// definitions
	int err;
	MyRio_IrqTimer irqTimer0;
	ThreadResource irqThread0;
	pthread_t thread;
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    // Registers corresponding to the IRQ channel
    irqTimer0.timerWrite = IRQTIMERWRITE;
    irqTimer0.timerSet = IRQTIMERSETTIME;
    timeoutValue = 500;

    // configure TimerIRQ
    status = Irq_RegisterTimerIrq( &irqTimer0,
    							   &irqThread0.irqContext,
    							   timeoutValue);

    // Set the indicator to allow the new thread
    irqThread0.irqThreadRdy = NiFpga_True;

    // Create new thread to catch the IRQ
    status = pthread_create( &thread,
    						 NULL,
    						 Timer_Irq_Thread,
    						 &irqThread0);

    printf_lcd("\fLab 6 ...");
    // Enter a loop, ending only when a backspace is received
    while (getkey() != DEL) {
//    	wait();
    }
    printf_lcd("\fGood Bye!");

    // Signal the new thread to terminate
    irqThread0.irqThreadRdy = NiFpga_False;
    status = pthread_join(thread, NULL);

    // MATLAB Data (not for debugging)
    mf = openmatfile("Lab6_Khrisna.mat", &err);
    if (!mf) {
    	printf("Can't open mat file %d\n", err);
    } else {
    	matfile_addstring(mf, "myName", "Khrisna Kamarga");
    	matfile_addmatrix(mf, "VADout", obuffer, IMAX, 1, 0);
    	matfile_addmatrix(mf, "VADin", ibuffer, IMAX, 1, 0);
    	matfile_close(mf);
    }
    // MATLAB Data

    // Unregistering the timer interrupt
    status = Irq_UnregisterTimerIrq( &irqTimer0,
    								 irqThread0.irqContext);

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*---------------------------------------------------------------------
Function Timer_Irq_Thread()
Purpose: Performs real-time Butterworth Filtering of the input signal
	 	 at Ch0.
Parameter:
	void* resource = pointer to the ThreadResource for the ISR
*---------------------------------------------------------------------*/
void *Timer_Irq_Thread(void* resource) {
	extern NiFpga_Session myrio_session; // getting myrio session from main()
	uint32_t irqAssert;
	// cast thread resource to ThreadResource structure
	ThreadResource* threadResource = (ThreadResource*) resource;
	// initialize analog interfaces before allowing IRQ
	AIO_initialize(&CI0, &CO0); // initialize
	Aio_Write(&CO0, 0); // zero analog input

	hiSat = 10; // maximum saturation voltage
	lowSat = -10; // minimum saturation voltage

	// dynamic system coefficients
	int myFilter_ns = 2; // No. of sections
	uint32_t timeoutValue = 500; // T - us; f_s = 2000 Hz
	static struct biquad myFilter[] = {
		{1.0000e+00, 9.9999e-01, 0.0000e+00,
		1.0000e+00, -8.8177e-01, 0.0000e+00, 0, 0, 0, 0, 0},
		{2.1878e-04, 4.3755e-04, 2.1878e-04,
		1.0000e+00, -1.8674e+00, 8.8220e-01, 0, 0, 0, 0, 0}
	};
	irqAssert = 0;

	while (threadResource->irqThreadRdy == NiFpga_True) {
		// wait for the occurrence (or timeout) of the IRQ.
		Irq_Wait( threadResource->irqContext,
				  TIMERIRQNO,
				  &irqAssert,
				  (NiFpga_Bool*) &(threadResource->irqThreadRdy));
		// schedule the next interrupt
		NiFpga_WriteU32( myrio_session,
						 IRQTIMERWRITE,
						 timeoutValue );
		NiFpga_WriteBool( myrio_session,
						  IRQTIMERSETTIME,
						  NiFpga_True);
		// if there is an occurrence of the IRQ
		if (irqAssert) {
			VADin = Aio_Read(&CI0); // reads analog input (current)
			// Your interrupt service code here
			VADout = cascade(VADin, myFilter, myFilter_ns, lowSat, hiSat);
//			printf_lcd("\fVAD:%f", VADout); // causes delay
			Aio_Write(&CO0, VADout); // debugging (send y(n) to AO)
			Irq_Acknowledge(irqAssert); // acknowledge the occurrence

			// MATLAB Data
			if (opt < obuffer + IMAX) {
				*opt++ = VADout;
				*ipt++ = VADin;
			} else {
				opt = obuffer;
				ipt = ibuffer;
			}
		}
	}

	// terminate the thread and return from function
	pthread_exit(NULL);
	return NULL;
}

/*---------------------------------------------------------------------
Function cascade()
Purpose: Calculates the signal output based on Biquad approximation of
		 a Butterworth filter.
Parameter:
	double xin = input signal
	struct biquad *fa = pointer to the biquad coefficients struct
	int ns = number of biquad segments
	double ymin = minimum saturation output
	double ymax = maximum saturation output
*---------------------------------------------------------------------*/
double cascade( double xin, // input
				struct biquad *fa, // biquad array
				int ns, // no. segments
				double ymin, // min output
				double ymax) { // max output
	struct biquad* f;
	double y0;
	int i;
	f = fa; // pointer to the first biquad
	y0 = xin; // input as output

	for (i = 1; i <= ns; i++) {
		f->x0 = y0; // y0 as input value becomes x0 of the current biquad
		// biquad cascade function
		y0 = (f->b0*f->x0 + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2)
			  /f->a0;

		if (i == ns) {
			y0 = SATURATE(y0, ymin, ymax); // saturate y0
		}
		// update the previous value of x and y as next
		f->x2 = f->x1;
		f->x1 = f->x0;
		f->y2 = f->y1;
		f->y1 = y0;
		f++; // increment pointer to get the next biquad
	}
	return y0;
}
