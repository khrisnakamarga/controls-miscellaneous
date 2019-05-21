/* Lab #7 - Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <pthread.h>
#include "TimerIRQ.h" // TimerIRQ interrupt
#include "IRQConfigure.h"
#include "matlabfiles.h"
#include "math.h"
#include "ctable2.h"

#define SATURATE(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
struct biquad {
	double b0; double b1; double b2; // numerator
	double a0; double a1; double a2; // denominator
	double x0; double x1; double x2; // input
	double y1; double y2; }; // output

typedef struct {
	NiFpga_IrqContext irqContext; // IRQ context reserved
	struct table *a_table; // table
	NiFpga_Bool irqThreadRdy; // IRQ thread ready flag
} ThreadResource;

struct table {
	const char *e_label; // entry label label
	int e_type; // entry type (0-show; 1-edit
	double value; // value
};

/* prototypes */
void *Timer_Irq_Thread(void* resource);

double cascade(double xin, // input
		struct biquad *fa, // biquad array
		int ns, // no. segments
		double ymin, // min output
		double ymax); // max output

double vel(void);

/* definitions */

MyRio_Aio CI0;
MyRio_Aio CO0;
uint32_t timeoutValue;
double VADin;
double VADout;
NiFpga_Session myrio_session;
MyRio_Encoder encC0;
NiFpga_Session myrio_session;
double e; // current error
double hisat = 7.5; // Max saturation voltage
double losat = -7.5; // Min saturation value
static double k = 0.0451; // constant k = kvi * kt (kvi = 0.41, kt = 0.11)

// MATLAB Data
#define IMAX 250 // # of max points = 500
static double vel_buffer[IMAX]; // velocity buffer (250 pts)
static double torq_buffer[IMAX]; // torque buffer (250 pts)
static double *vel_pt = vel_buffer; // velocity buffer pointer
static double *torq_pt = torq_buffer; // torque buffer pointer
static double vref_curr; // current vref value
static double vref_prev; // previous vref value
static double kp_stor; // stored kp value for matlab
static double ki_stor; // stored ki value for matlab
static double bti_stor; // stored bti value for matlab
MATFILE *mf;
int err;
// MATLAB Datafct

/*---------------------------------------------------------------------
Function main()
Purpose: creates the timer interrupt thread that performs
		 real-time DC motor velocity control. Calls the
		 ctable2 function to display the editable coefficients
		 and the measured values. User is able to interract
		 with the table through the keypad.
*---------------------------------------------------------------------*/
int main(int argc, char **argv) {
	MyRio_IrqTimer irqTimer0;
	ThreadResource irqThread0;
	pthread_t thread;
	NiFpga_Status status;

	/* Initialize Table Editor Variables */
	char *Table_Title = "Motor Control Table";
	table my_table[] = {
		{"V_ref: (rpm) ", 1, 0.0 },
		{"V_act: (rpm) ", 0, 0.0 },
		{"VDAout: (mV) ", 0, 0.0 },
		{"Kp: (V-s/r) ", 1, 0.0 },
		{"Ki: (V/r) ", 1, 0.0 },
		{"BTI: (ms) ", 1, 5.0 }
	};

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    // Registers corresponding to the IRQ channel
    irqTimer0.timerWrite = IRQTIMERWRITE;
    irqTimer0.timerSet = IRQTIMERSETTIME;
    timeoutValue = 5000; // BTI = 5ms

    // configure TimerIRQ
    status = Irq_RegisterTimerIrq(
    			&irqTimer0,
				&irqThread0.irqContext,
				timeoutValue);

    // Set the indicator to allow the new thread
    irqThread0.irqThreadRdy = NiFpga_True;

    /* Set table to irqthread table */
    irqThread0.a_table = my_table;

    // Create new thread to catch the IRQ
    /* Create New Thread to Catch the IRQ */
    status = pthread_create(&thread,
							NULL,
							Timer_Irq_Thread,
							&irqThread0);

    /* Run table editor */
    ctable2(Table_Title, my_table, 6);

    // Signal the new thread to terminate
    irqThread0.irqThreadRdy = NiFpga_False;
    status = pthread_join(thread, NULL);

    // Unregistering the timer interrupt
    status = Irq_UnregisterTimerIrq( &irqTimer0,
    								 irqThread0.irqContext);

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*---------------------------------------------------------------------
Function Timer_Irq_Thread()
Purpose: Performs real-time DC motor velocity control by using
		 the transfer function coefficients defined in main()
Parameter:
	void* resource = pointer to the ThreadResource for the ISR
*---------------------------------------------------------------------*/
void *Timer_Irq_Thread(void* resource) {
	extern NiFpga_Session myrio_session; // getting myrio session from main()
	uint32_t irqAssert;
	// cast thread resource to ThreadResource structure
	ThreadResource* threadResource = (ThreadResource*) resource;

	/* Declare Names for Table Entries From Table Pointer */
	double *vref = &((threadResource->a_table+0)->value); // V_ref (rpm)
	double *vact = &((threadResource->a_table+1)->value); // V_act (rpm)
	double *VDAout = &((threadResource->a_table+2)->value); // VDAout (mV)
	double *kp = &((threadResource->a_table+3)->value); // Kp (Vs/r)
	double *ki = &((threadResource->a_table+4)->value); // Ki (V/r)
	double *bti = &((threadResource->a_table+5)->value); // BTI (ms)
	double T = *bti/1000.; // T used in Tustin's method TF

	// initialize analog interfaces before allowing IRQ
	AIO_initialize(&CI0, &CO0); // initialize
	Aio_Write(&CO0, 0.0); // zero analog input
	EncoderC_initialize(myrio_session, &encC0);

	// Set up vref_curr and vref_prev to compare for timing the saving of each response
	vref_curr = *vref;
	vref_prev = *vref;

	/* Define dynamic system as array of biquad structures */
	int myFilter_ns = 1; // No. of sections
	uint32_t timeoutValue = 5000;

	static struct biquad myFilter[] = { //myFilter is name of array of biquad struct being initialized
		{0.0000e+00, 0.0000e+00, 0.0000e+00, // First 2 lines establish number of biquad sections & length of BTI in microseconds
		0.0000e+00, 0.0000e+00, 0.0000e+00, 0, 0, 0, 0, 0}
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
			/* Check if current vref has changed since last BTI*/
			if (vref_curr != *vref) {
				vref_prev = vref_curr; // move forward in iteration by setting previous vel = current vel
				vref_curr = *vref; // new current vel value is equal to entered vref
				/*Address of pt is set to beginning of buffer for torque and velocity */
				vel_pt = vel_buffer;
				torq_pt = torq_buffer;
			}

			// Compute PI Coeff
			myFilter -> a0 = 1;
			myFilter -> a1 = -1;
			myFilter -> b0 = (*kp) + (0.5*(*ki)*T);
			myFilter -> b1 = -(*kp) + (0.5*(*ki)*T);
			*vact = (vel() * 1000 * 60) / (5 * 2000);
			e = (*vref - *vact) * (2*M_PI) / (60);
			*VDAout = cascade(e, myFilter, myFilter_ns, losat, hisat);

			Aio_Write(&CO0, *VDAout); // debugging (send y(n) to AO)


			// MATLAB Data
			if (vel_pt < vel_buffer + IMAX) {
				*torq_pt++ = (*VDAout) * k;
				*vel_pt++ = *vact;
			}
//			else {
//				torq_pt = torq_buffer;
//				vel_pt = vel_buffer;
//			}
			*VDAout = *VDAout * 1000;
			Irq_Acknowledge(irqAssert); // acknowledge the occurrence
		}
	}
	/* record kp, ki and bti for matlab */
	kp_stor = *kp; // user-entered kp value stored for matlab
	ki_stor = *ki; // user-entered ki value stored for matlab
	bti_stor = *bti; // user-entered bti value stored for matlab
	/* Comment out the below section when debugging */
	mf = openmatfile("Lab7_Khrisna.mat", &err);
	if(!mf) printf("CanÆt open mat file %d\n", err);
	matfile_addstring(mf, "myName", "Khrisna Kamarga");
	matfile_addmatrix(mf, "vact", vel_buffer, IMAX, 1, 0);
	matfile_addmatrix(mf, "torq", torq_buffer, IMAX, 1, 0);
	matfile_addmatrix(mf, "vref_curr", &vref_curr, 1, 1, 0);
	matfile_addmatrix(mf, "vref_prev", &vref_prev, 1, 1, 0);
	matfile_addmatrix(mf, "k_p", &kp_stor, 1, 1, 0);
	matfile_addmatrix(mf, "k_i", &ki_stor, 1, 1, 0);
	matfile_addmatrix(mf, "bti", &bti_stor, 1, 1, 0);
	matfile_close(mf);
	/* End of MATLAB Section */

	// terminate the thread and return from function
	pthread_exit(NULL);
	return NULL;
}

/*---------------------------------------------------------------------
Function cascade()
Purpose: Calculates the signal output based on Biquad approximation
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
	f = fa; // pointer to the first biquad
	y0 = xin; // input as output

	f->x0 = y0; // y0 as input value becomes x0 of the current biquad
	// biquad cascade function
	y0 = (f->b0*f->x0 + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2)
		  /f->a0;

	y0 = SATURATE(y0, ymin, ymax); // saturate y0
	// update the previous value of x and y as next
	f->x2 = f->x1;
	f->x1 = f->x0;
	f->y2 = f->y1;
	f->y1 = y0;
	f++; // increment pointer to get the next biquad
	return y0;
}

/*--------------------------------------------------------------
Function vel()
Purpose: Calculating the velocity of the motor by accessing the
		 encoder counter with the knowledge of BTI and BDI.
--------------------------------------------------------------*/
double vel(void) {
	static int32_t cn; // current encoder count
	static int32_t cn1; // previous encoder count
	static int count = 1; // number of times vel() is called
	double encoderSpeed; // cn - cn1
	cn = Encoder_Counter(&encC0); // reads the encoder counter
	if (count == 1) {
		cn1 = cn; // initial condition is set the same as the first count
		count = 0; // ensures that cn1 = cn is only done on the first call
	}
	encoderSpeed= cn - cn1; // current and previous count difference
	cn1 = cn; // replace previous cn (cn1) with the current one
	return encoderSpeed; // return the rpm (double)
}
