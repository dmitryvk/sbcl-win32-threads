#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int myfun(int c)
{
	fprintf(stderr, "myfun(%d)\n", c);
	Sleep(3000);
	fprintf(stderr, "myfun returns\n");
	return 10;
}
