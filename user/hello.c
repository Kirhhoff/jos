// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	volatile int i=0,j=1;
	volatile int k=j/i;

	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
}
