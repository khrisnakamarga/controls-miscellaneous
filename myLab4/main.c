/* Lab #4 - Khrisna Kamarga */

/* includes */
#include <stdio.h>
#include "Encoder.h"
#include "MyRio.h"
#include "DIO.h"
#include "me477.h"
#include <unistd.h>
#include "matlabfiles.h"

/* prototypes */
double double_in(char *prompt); // calling the double_in() routine
NiFpga_Status
	EncoderC_initialize(NiFpga_Session myrio_session,
						MyRio_Encoder *channel);
uint32_t Encoder_Counter(MyRio_Encoder* channel);
void initializeSM(void);
void wait(void);
double vel(void);
void high(void);
void low(void);
void speed(void);
double vel(void);
void stop(void);
void wait(void);

/* MATLAB declarations */
MATFILE *openmatfile(char *fname, int *err);
int err;
MATFILE *mf; // MATLAB file

/* Define an enumerated type for states */
typedef enum {HIGH, LOW, SPEED, STOP, EXIT} State_Type;

/* Define a table of pointers to the functions for each state */
static void (*state_table[])(void) = {high, low, speed, stop};

/* Global Declaration */
NiFpga_Session myrio_session;
MyRio_Encoder encC0;
static int N;
static int M;
static State_Type curr_state;
static int Clock;
static int run;
MyRio_Dio Ch0;
MyRio_Dio Ch6;
MyRio_Dio Ch7;

#define IMAX 2400 // max points
static double buffer[IMAX]; // speed buffer
static double *bp = buffer; // buffer pointer

/*--------------------------------------------------------------
Function main
Purpose: Testing the finite state machine that controls the
		 DC motor RPM, as well as measuring its speed and disp-
		 laying it in the LCD screen
--------------------------------------------------------------*/
int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open(); // open the myRIO NiFpga Session (1)
    if (MyRio_IsNotSuccess(status)) return status;
    initializeSM(); // setup interface conditions and initialize the state machine (2)
    N = double_in("\fInput N: "); // request the number N of wait intervals in each BTI (3)
    M = double_in("input M: "); // request the number M of intervals for when the motor is on in each BTI (4)
    // This is the main state transition loop
    while (curr_state != EXIT) {
    	state_table[curr_state](); // activates current state
    	Dio_WriteBit(&Ch0, run); // sets the current in channel 0
    	wait();
    	Clock++;
    }

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

// States
/*--------------------------------------------------------------
Function high()
Purpose: Sets run bar and clock to 0. This will output 0.25A to
		 the DC motor. If channel 7 button is pressed, the state
		 is changed to SPEED. If channel 6 button is pressed,
		 the state is changed to STOP. Else, the channel is going
		 to oscilate between high and low
--------------------------------------------------------------*/
void high() {
	if (Clock == N) {
		Clock = 0;
		run = 0;
		if (Dio_ReadBit(&Ch7) == 0) {
			curr_state = SPEED;
		} else if (Dio_ReadBit(&Ch6) == 0) {
			curr_state = STOP;
		} else {
			curr_state = LOW;
		}
	}
}

/*--------------------------------------------------------------
Function low()
Purpose: Sets run bar to 1, not allowing current to the motor.
		 Transitions to high once the clock reaches M
--------------------------------------------------------------*/
void low(void) {
	if (Clock == M) {
		run = 1;
		curr_state = HIGH;
	}
}

/*--------------------------------------------------------------
Function speed)
Purpose: Prints the measured velocity to the LCD screen if button
	     in Channel 6 is pressed. Transitions back to low. Or if
	     M is 1, it transitions back to high
--------------------------------------------------------------*/
void speed() {
	double rpm;
	rpm = (vel() * 1000 * 60) / (5.002 * N * 2000); // in rpm
	//printf_lcd("\fspeed %g rpm", rpm);
	if (bp < buffer + IMAX) *bp++ = rpm;

	if (M == 1) {
		run = 1;
		curr_state = HIGH;
	} else {
		curr_state = LOW;
	}
}

/*--------------------------------------------------------------
Function stop()
Purpose: Stops the DC motor, saves the reading to a MATLAB file
--------------------------------------------------------------*/
void stop() {
	double Npar;
	double Mpar;
	Npar = (double) N;
	Mpar = (double) M;
	run = 1;
	printf_lcd("\fstopping");
	curr_state = EXIT;
	// saving the response to MATLAB
	mf = openmatfile("Lab4.mat", &err);
	if(!mf) printf("Can't open mat file %d\n", err);
	matfile_addstring(mf, "myName", "Khrisna Kamarga");
	matfile_addmatrix(mf, "N", &Npar, 1, 1, 0);
	matfile_addmatrix(mf, "M", &Mpar, 1, 1, 0);
	matfile_addmatrix(mf, "vel", buffer, IMAX, 1, 0);
	matfile_close(mf);
}

/*--------------------------------------------------------------
Function initializeSM()
Purpose: Initializes the channels and setting the variables
--------------------------------------------------------------*/
void initializeSM() {
	// initialize channel 0
	Ch0.dir = DIOA_70DIR;
	Ch0.out = DIOA_70OUT;
	Ch0.in = DIOA_70IN;
	Ch0.bit = 0;
	// initialize channel 6
	Ch6.dir = DIOA_70DIR;
	Ch6.out = DIOA_70OUT;
	Ch6.in = DIOA_70IN;
	Ch6.bit = 6;
	// initialize channel 7
	Ch7.dir = DIOA_70DIR;
	Ch7.out = DIOA_70OUT;
	Ch7.in = DIOA_70IN;
	Ch7.bit = 7;
	// setup all interface conditions
	EncoderC_initialize(myrio_session, &encC0);
	run = 1; // set run_bar = 1 to stop the motor (no current supplied)
	curr_state = LOW; // initial state = LOW
	Clock = 0; // initialize clock
}

/*--------------------------------------------------------------
Function vel()
Purpose: Calculating the velocity of the motor by accessing the
		 encoder counter with the knowledge of BTI and BDI.
--------------------------------------------------------------*/
double vel(void) {
	static uint32_t cn; // current encoder count
	static uint32_t cn1; // previous encoder count
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

