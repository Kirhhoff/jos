#include<inc/stdio.h>

/*
    Trace the execution of the following code step-by-step:
        
        int x = 1, y = 3, z = 4;
        cprintf("x %d, y %x, z %d\n", x, y, z);

    In the call to cprintf(), to what does fmt point? To what does ap point?
    List (in order of execution) each call to cons_putc, va_arg, and vcprintf. 
    For cons_putc, list its argument as well. 
    For va_arg, list what ap points to before and after the call. 
    For vcprintf list the values of its two arguments.
*/

/*
1. 
*/
static void question3(){
    int x=1,y=3,z=4;
    cprintf("x %d, y %x, z %d\n",x,y,z);    

}

/*
    Run the following code.

        unsigned int i = 0x00646c72;
        cprintf("H%x Wo%s", 57616, &i);

    What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise.
    Here's an ASCII table that maps bytes to characters.

    The output depends on that fact that the x86 is little-endian. 
    If the x86 were instead big-endian what would you set i to in order to yield the same output? 
    Would you need to change 57616 to a different value?
*/
static void question4(){
    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s\n", 57616, &i);
}

/*
    In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) 
    Why does this happen?

        cprintf("x=%d y=%d", 3);

*/
static void question5(){
    cprintf("x=%d y=%d\n", 3);
}

void hookfunc(void){
    question3();
    question4();
    question5();
}

