/* Lab #0 - Khrisna Kamarga */

/* includes */
#include "stdio.h"
#include "MyRio.h"
#include "me477.h"

/* prototypes */
int sumsq(int x); /* sum of squares */

/* definitions */
#define N 4 /* number of loops */

int main(int argc, char **argv) {
	NiFpga_Status status;
	static int x[10]; /* total */
	static int i; /* index */
	status = MyRio_Open(); /* Open NiFpga.*/
	if (MyRio_IsNotSuccess(status))
		return status;
	printf_lcd("\fHello, Khrisna Kamarga\n\n");
	for (i = 0; i < N; i++) {
		x[i] = sumsq(i);
		printf_lcd("%d, ", x[i]);
	}
	status = MyRio_Close(); /* Close NiFpga. */
	return status;
}
int sumsq(int x) {
	static int y = 4;
	y = y + x * x;
	return y;
}
