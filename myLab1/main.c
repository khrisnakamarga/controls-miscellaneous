/* Lab #1 - Name: Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>
#include <stdarg.h>

/* prototypes */
double	double_in(char *prompt);
char * fgets_keypad(char *buf, int buflen);
int printf_lcd(char *format, ...);

/*--------------------------------------------------------------
Function main
Purpose: Assigns double values inputted from the keypad
		 and prints it on the lcd
--------------------------------------------------------------*/
int main(int argc, char **argv) {
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    double val = double_in("Value: ");
    double val2 = double_in("Velocity: ");
    printf_lcd("\f%lf\n%lf", val2, val);
    int num = printf_lcd("four"); // to test printf
    printf("%d", num); // should return 4

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*--------------------------------------------------------------
Function double_in
Purpose: Inputs data from the keypad and prompting in the LCD
Parameters: (in) - prompt, the name of the prompt
 Returns: val, value from the keypad
*--------------------------------------------------------------*/
double double_in(char *prompt) {
	char string[40];
	double val;
	int error;
	static int i; // iterator for spaces

	printf_lcd("\f");
	error = 1;
	while (error == 1) {
		*string = NULL; // resetting *string
		printf_lcd("\v");
		for (i = 0; i < 20; i++) {
			printf_lcd(" ");
		} // making sure that the prompt is clear
		printf_lcd("\v");
		printf_lcd(prompt); // printing the prompt
		fgets_keypad(string, 40); // prompting input
		char *pstring = string; // making string pointer
		if (*string == NULL) {
			printf_lcd("\v\nShort. Try Again    ");
		} else if ((strpbrk(string, "[]") != NULL) ||
				   (strpbrk(++pstring, "-") != NULL) ||
				   (strstr(string, "..") != NULL)) {
			printf_lcd("\v\nBad Key. Try Again. ");
		} else {
			error = 0;
			sscanf(string,"%lf",&val);
		}
	}
	return val;
}

/*--------------------------------------------------------------
Function printf_lcd
Purpose: printing the string passed
Parameters: (in) - format, string passed
 Returns: length, -1 if error, otherwise prints the
 	 	 	 	 	 	 	 	 length of the string
*--------------------------------------------------------------*/
int printf_lcd(char *format, ...) {
	int length = 0; // excluding EOF
	char string[81];
	va_list args;

	va_start(args, format);
	vsnprintf(string, 81, format, args);
	va_end(args);

	char *pstring = string;
	while (*pstring != '\0') {
		putchar_lcd(*pstring); // putting each char to LCD
		pstring++; // incrementing the pointer
		length++; // incrementing the length
		// catching error
		if (length >= 80) { // if the length > max, error
			return -1;
		}
	}
	return length;
}


