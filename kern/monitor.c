// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "trace", "Print the stack trace", mon_backtrace },
	{ "smps", "Show physical pages mapped to specific virtual address area",mon_showmappings},
	{ "stp", "Set permissions of specific virtual pages",mon_setpermissions},
	{ "clp", "Clear permissions of specific virtual pages", mon_clearpermissions},
};

/***** Funtional inline tools for kernel monitor commands *****/

// transform a string to a number
// only support base 10 or 16
static inline int 
str2unum(char* str,int base){
	int ret=0,offset=0;
	if(base==10)
		while(*str&&(*str>='0')&&(*str<='9')){
			offset=(*str++)-'0';// char to number
			ret=base*ret+offset;
		}
	else if(base==16)
		while(*str&&(((*str>='0')&&(*str<='9'))||((*str>='a')&&(*str<='f')))){
			offset=*str-(*str<='9'?'0':('a'-10));// char to number
			ret=base*ret+offset;
			str++;
		}
	return *str?-1:ret;
}

// transfrom a number into 2-based string of
// specified bit
static inline void
num2binstr(uint32_t num,char* store,int bit){
	while(bit){
		store[--bit]=num%2+'0';
		num/=2;
	}
}

// map a capital character to 
// permission number
static inline uint32_t
char2perm(char c){
	switch (c)
	{
	case 'G': return PTE_G;
	case 'D': return PTE_D;
	case 'A': return PTE_A;
	case 'C': return PTE_PCD;
	case 'T': return PTE_PWT;
	case 'U': return PTE_U;
	case 'W': return PTE_W;
	case 'P': return PTE_P;
	default: return -1;
	}
}

// map a string of capital character
// to permission number, panic when
// illegal perm char passed
static inline uint32_t
str2perm(char* str){
	uint32_t perm=0;
	while(*str)
		perm|=char2perm(*str++);

	if(perm<0)
		panic("Wrong permission argument\n");
	
	// setting for 'Present' bit will be
	// automatically cancelled(forbidden) 
	perm&=(~char2perm('P'));

	return perm;
}

// macro used for string processing
#define IS_HEX(str) (((str[0])=='0')&&((str[1])=='x'))
#define HEX_VAL(str) (str2unum((str+2),16))
#define DEC_VAL(str) (str2unum(str,10))

#define START(str) (ROUNDDOWN(HEX_VAL(argv[1]),PGSIZE))

// nages=1 when there is only start address
// nages=n when there is a decimal second arg
// npages=(end-start)/PGSIZE when there is hexdecimal second arg
// also necessary round up/down
#define N_PAGES(argc,va_start,argv)((argc==2)?1:(IS_HEX(argv[2])?(ROUNDUP(HEX_VAL(argv[2]),PGSIZE)-va_start)/PGSIZE:DEC_VAL(argv[2])))

#define PDE(pgdir,va) (pgdir[(PDX(va))])
#define PTE_PTR(pgdir,va) (((pte_t*)KADDR(PTE_ADDR(PDE(pgdir,va))))+PTX(va))
#define PTE(pgdir,va) (*(PTE_PTR(pgdir,va)))

// macro to check present bit of pde/pte
#define P_PDE(pgdir,va) ((PDE(pgdir,va))&PTE_P)
#define P_PTE(pgdir,va) (PTE(pgdir,va)&PTE_P)
#define PERM(pgdir,va) (PTE(pgdir,va)-PTE_ADDR(PTE(pgdir,va)))

// validate input args and retrieve them
// if legitimate, otherwise panic
// requirements:
// 1. argc >=2
// 2. argv[1] written in hexdecimal form with 0x prefix
// 3. argv[2] written in hexdecimal or decimal form, the former
//    represents end page address, the latter represents number
//    of pages to specify
// 4. retrieved va_start and n_pages must be non-negative
static inline void validate_and_retrieve(int argc,char** argv,uintptr_t* va_start,uint32_t* n_pages,char* hint){
	// check in case of empty args
	if(argc<=1)
		panic(hint);

	// validate and retrieve start address and n_pages
	if(!IS_HEX(argv[1]))
		panic(hint);
	*va_start=START(argv[1]);	
	*n_pages=N_PAGES(argc,*va_start,argv);

	// check for illegal arg
	if (*va_start<0||*n_pages<0)
		panic(hint);
}

// universe funtioncal tool to set/clear page permissions,
// whose behaviour depends on op
void change_permissions(int argc, char **argv,bool op,char* hint){
	uintptr_t va_start;	
	uint32_t n_pages;
	validate_and_retrieve(argc-1,argv,&va_start,&n_pages,hint);
	
	// retrieve target permissions
	uint32_t perm=str2perm(argv[argc-1]);
	uintptr_t va;
	pte_t* va_pte;
	int cnt;
	extern pde_t* kern_pgdir;
	for(cnt=0;cnt<n_pages;cnt++){
		va=va_start+cnt*PGSIZE;
		//check pte present
		if(P_PDE(kern_pgdir,va)&&P_PTE(kern_pgdir,va)){
			va_pte=PTE_PTR(kern_pgdir,va);
			//set or clear
			*va_pte=op?(*va_pte|perm):(*va_pte&(~perm));
		}
	}
}

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	int depth=0; //record the trace depth
	/*
	From left to right:
		register %ebp
		saved ebp
		return address
		argument 0,1,2,3,4
	*/
	uint32_t *cur_ebp=(uint32_t*)read_ebp(),
		saved_ebp,ret_adr,arg0,arg1,arg2,arg3,arg4;
	
	while(true){
		int arg_index;
		struct Eipdebuginfo info;
		
		saved_ebp=*cur_ebp;
		ret_adr=*(cur_ebp+1);
		
		debuginfo_eip((uintptr_t)ret_adr,&info);

		cprintf("depth %d: ebp 0x%x, retadr 0x%x, args",depth,cur_ebp,ret_adr);
		
		for(arg_index=0;arg_index<info.eip_fn_narg;arg_index++)
			cprintf(" 0x%x",*(cur_ebp+2+arg_index));
	
		cprintf("\n       %s:%d: %.*s+%d\n",info.eip_file,info.eip_line,info.eip_fn_namelen,info.eip_fn_name,(uint32_t)ret_adr-(uint32_t)info.eip_fn_addr-5);
		
		/*
			0x0 is the base address of kernel stack.
			Current ebp reachs 0x0, which implies, we have reached the
			root of the calling nest.		
		*/
		if((uint32_t)cur_ebp==(uint32_t)0x0)
			break;

		cur_ebp=(uint32_t*)saved_ebp;// Track back to the base address of caller.
		depth++;// Update the trace depth.
	}

	return 0;
}

int 
mon_showmappings(int argc,char** argv,struct Trapframe* f){
	char hint[]="\nPlease pass arguments in correct formats, for example:\n"
				"  smps 0x3000 0x5000 ---show the mapping from va=0x3000 to va=0x5000\n"
				"  smps 0x3000 100 ---show the mapping of 100 virtual pages from va=0x3000\n"
				"  smps 0x3000 ---show the mapping of va=0x3000 only\n";

	uintptr_t va_start;	
	uint32_t n_pages;
	validate_and_retrieve(argc,argv,&va_start,&n_pages,hint);

	// headline
	cprintf(
		"G: global   I: page table attribute index D: dirty\n"
		"A: accessed C: cache disable              T: write through\n"
		"U: user     W: writeable                  P: present\n"
		"---------------------------------\n"
		"virtual_ad  physica_ad  GIDACTUWP\n");

	uintptr_t va;
	int cnt;
	extern pde_t* kern_pgdir;

	// print out message w.r.t each page
	for(cnt=0;cnt<n_pages;cnt++){
		va=va_start+cnt*PGSIZE;
		if(P_PDE(kern_pgdir,va)&&P_PTE(kern_pgdir,va)){

			// transform permission to binary string
			char permission[10];
			permission[9]='\0';
			num2binstr(PERM(kern_pgdir,va),permission,9);

			// iff page is present
			cprintf("0x%08x  0x%08x  %s\n",va,PTE_ADDR(PTE(kern_pgdir,va)),permission);
			continue;
		}

		// once pde or pte is not present, print blank line
		cprintf("0x%08x  ----------  ---------\n",va);	
	}
		
	return 0;
}


int mon_setpermissions(int argc, char **argv, struct Trapframe *tf){
	char hint[]="\nPlease pass arguments in correct formats, for example:\n"
				"  stp 0x3000 0x5000 AD ---set permission bit A and D from va=0x3000 to va=0x5000\n"
				"  stp 0x3000 100 AD---set permission bit A and D of 100 virtual pages from va=0x3000\n"
				"  stp 0x3000 AD---set permission bit A and D of va=0x3000 only\n"
				"\n"
				"G: global   I: page table attribute index D: dirty\n"
				"A: accessed C: cache disable T: write through\n"
				"U: user     W: writeable     P: present\n"
				"\n"
				"ps: P is forbbiden to set by hand\n";
	change_permissions(argc,argv,1,hint);
	cprintf("Permission has been updated:\n");
	mon_showmappings(argc-1,argv,tf);

	return 0;
}

int mon_clearpermissions(int argc, char **argv, struct Trapframe *tf){
	char hint[]="\nPlease pass arguments in correct formats, for example:\n"
				"  clr 0x3000 0x5000 AD ---clear permission bit A and D from va=0x3000 to va=0x5000\n"
				"  clr 0x3000 100 AD---clear permission bit A and D of 100 virtual pages from va=0x3000\n"
				"  clr 0x3000 AD---clear permission bit A and D of va=0x3000 only\n"
				"\n"
				"G: global   I: page table attribute index D: dirty\n"
				"A: accessed C: cache disable T: write through\n"
				"U: user     W: writeable     P: present\n"
				"\n"
				"ps: P is forbbiden to clear by hand\n";
	change_permissions(argc,argv,0,hint);
	cprintf("Permission has been updated:\n");
	mon_showmappings(argc-1,argv,tf);

	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	// int f=0;
	// for(;f<argc;f++)
	// 	cprintf("arg%d=%s\n",f,argv[f]);
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);
	// hookfunc();

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
