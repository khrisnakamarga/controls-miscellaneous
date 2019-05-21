/* Lab #3 - Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include "UART.h"
#include "DIO.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>

/* prototypes */
int putchar_lcd(int value);
char getkey(void);
void wait(void);


/* definitions */
char string[40]; // char array to store the keypad input

/*--------------------------------------------------------------
Function main
Purpose: Testing the subroutines stated in the prototypes.
		 1) putchar_lcd: using printf_lcd and testing edge case
		 2) getkey: printing the key to the console
		 3) using fgets_keypad() and printf_lcd()
--------------------------------------------------------------*/
int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //testing putchar_lcd()
    printf_lcd("\f");
    putchar_lcd('\n'); // try int between 0 - 255, or special chars like '\f', '\b', etc
    printf("EOF: %d\n", putchar_lcd(-100)); // try int < 0 or > 255, should print -1 in the console

    //testing getkey()
    printf("key pressed: %c\n", getkey()); // check if the console prints the same key

    //testing the subroutines from fgets_keypad() and printf_lcd()
    printf_lcd("\f");
    printf_lcd("\fkeys pressed: %s\n", fgets_keypad(string, 40));
    printf_lcd("\fkeys pressed: %s", fgets_keypad(string, 40));
    // compare what is inputted and what is printed, should match

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*--------------------------------------------------------------
Function putchar_lcd
Purpose: Establishes serial communication the first time, then
		 translates the value passed into ASCII characters that
		 is passed to the LCD screen. Returns the value passed
		 if successful, returns EOF otherwise.
Parameters:
		 int value = value to be passed to the LCD screen
--------------------------------------------------------------*/
int putchar_lcd(int value) {
	static MyRio_Uart uart;
	NiFpga_Status status;
	size_t nData = 1;
	uint8_t writeS[20];
	static int calledFirst = 0; // only initializes once

	// initializing the serial communication
	if (calledFirst == 0) {
		uart.name = "ASRL2::INSTR"; // UART on Connector B
		uart.defaultRM = 0; // def. resource manager
		uart.session = 0; // session referencet
		status = Uart_Open( &uart, // port information
							19200, // baud rate
							8, // no. of data bits
							Uart_StopBits1_0, // 1 stop bit
							Uart_ParityNone); // No parity
		calledFirst++;
	}
	// if the communication is successfully established
	if (status >= VI_SUCCESS && value < 256 && value >= 0) {
		if (value == '\f') { // clear lcd
			*writeS = 12;
		} else if (value == '\b') { // move cursor back
			*writeS = 8;
		} else if (value == '\v') { // go to the start of the line
			*writeS = 128;
		} else if (value == '\n') { // new line
			*writeS = 13;
		} else { // ASCII characters
			*writeS = value;
		}
		status = Uart_Write( &uart, // port information
							 writeS, // data array
							 nData); // no. of data codes
		if (status >= VI_SUCCESS) {
			return value;
		} else {
			return EOF;
		}
	} else {
		return EOF;
	}
}

/*--------------------------------------------------------------
Function getkey
Purpose: Scans the keyboard for low bit output, then returns the
	     corresponding value of the keypad press. The value
	     will be returned once the key has been depressed.
--------------------------------------------------------------*/
char getkey(void) {
	int i;
	int j;
	char table[4][4] = { {'1','2','3', UP},
					     {'4','5','6', DN},
					     {'7','8','9',ENT},
				    	 {'0','.','-',DEL} };
	int lowBitDetect = 1; // 1 when low bit hasn't been detected
	NiFpga_Bool rowBit; // row bit

	// defining the DIO data types
	typedef struct { uint32_t dir; // direction register
				     uint32_t out; // output value register
				     uint32_t in; // input value register
				     uint8_t bit; // Bit to modify
	} MyRio_Dio;

	// initializes all the DIO
	MyRio_Dio Ch[8];
	for (i=0; i<8; i++) {
		Ch[i].dir = DIOB_70DIR;
		Ch[i].out = DIOB_70OUT;
		Ch[i].in = DIOB_70IN;
		Ch[i].bit = i;
	}

	while (lowBitDetect) {
		for (i = 0; i < 4; i++) {
			// setting all the columns to high-Z
			for (j = 0; j < 4; j++) {
				Dio_ReadBit(&Ch[j]);
			}
			Dio_WriteBit(&Ch[i], NiFpga_False); // setting the current column to low
			for (j = 4; j < 8; j++) {
				rowBit = Dio_ReadBit(&Ch[j]);
				if (rowBit == NiFpga_False) {
					lowBitDetect = 0; // low bit has been detected
					break;
				}
			}
			if (rowBit == NiFpga_False) {
				break;
			}
			wait();
		}
	}
	while (rowBit == NiFpga_False) { // while the key is still pressed
		rowBit = Dio_ReadBit(&Ch[j]); // purpose: not output multiple numbers
		wait();
	}
	return table[j - 4][i];
}

/*----------------------------------------
Function wait
	Purpose: waits for xxx ms.
	Parameters: none
	Returns: none
*-----------------------------------------*/
void wait(void) {
	uint32_t i;
	i = 417000;
	while(i>0){
		i--;
	}
	return;
}
