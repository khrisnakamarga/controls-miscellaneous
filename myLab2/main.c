/* Lab #2 - Name: Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"
#include <string.h>

/* prototypes */
int getchar_keypad(void);

/*--------------------------------------------------------------
Function main
Purpose: Testing the getchar_keypad() subroutine by calling
		 fgets_keypad() and printing the string returned twice
--------------------------------------------------------------*/
int main(int argc, char **argv)
{
	NiFpga_Status status;

    status = MyRio_Open();		    /*Open the myRIO NiFpga Session.*/
    if (MyRio_IsNotSuccess(status)) return status;

    //my code here
    char string[40];
    printf_lcd("\fLab #2\n");
    printf_lcd("\f%s\n", fgets_keypad(string, 40));
    printf_lcd("\n%s", fgets_keypad(string, 40));

	status = MyRio_Close();	 /*Close the myRIO NiFpga Session. */
	return status;
}

/*--------------------------------------------------------------
Function getchar_keypad
Purpose: Prompts the user to input an array of characters.
		 returns the the first character typed. When the buffer
		 is not empty, the next character is returned.
		 The characters inputted through the keypad can be
		 editted using the delete button.
*--------------------------------------------------------------*/
int getchar_keypad(void) {
	static int n = 0; // making sure the location of n doesn't change
	int buff_len = 40; // number of characters in the buffer
	static char buff[40]; // buffer char array
	static char *buffPtr; // buffer pointer

	if (n == 0) { // if the buffer is empty
		buffPtr = &buff; // resetting the buffer pointer
		char key = getkey(); // obtaining the key from the keypad
		while (key != ENT && n < buff_len) {
			if (key == DEL) { // if delete is pressed
				buffPtr--; // decrement the pointer
				n--; // reduce the size
				putchar_lcd('\b');
				putchar_lcd(' ');
				putchar_lcd('\b'); // delete the spot and go back
			} else {
				*buffPtr = key; // assign the key to the current buffer
				buffPtr++; // increment the buffer
				n++; // increment buffer size
				putchar_lcd(key); // show the char on LCD
			}
			key = getkey(); // re-obtain the key from the keypad
			while (key == DEL && n == 0) {
				key = getkey();
				putchar_lcd(' ');
				putchar_lcd('\b');
			}
		}
		n++; // increment the buffer size
		buffPtr = &buff; // set the pointer to the beginning of the buffer
	}

	if (n > 1) { // if the buffer is not empty
		n--; // decrement the buffer size
		return *buffPtr++; // return the character at the pointer; increment
	} else {
		n--; // decrement the buffer size
		return EOF; // return EOF
	}
}
