// System call stubs.

#include <inc/syscall.h>
#include <inc/lib.h>

static int32_t
syscall(int num, int check, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t ret;
	// Generic system call: pass system call number in AX,
	// up to five parameters in DX, CX, BX, DI, SI.
	// Interrupt kernel with T_SYSCALL.
	//
	// The "volatile" tells the assembler not to optimize
	// this instruction away just because we don't use the
	// return value.
	//
	// The last clause tells the assembler that this can
	// potentially change the condition codes and arbitrary
	// memory locations.

	// move arguments into registers before %ebp modified
	// as with static identifier, num, check and a1 are stroed
	// in %eax, %edx and %ecx respectively, while a3, a4 and a5
	// are addressed via 0xx(%ebp)
	asm volatile("movl %0,%%edx"::"S"(a1):"%edx");
	asm volatile("movl %0,%%ecx"::"S"(a2):"%ecx");
	asm volatile("movl %0,%%ebx"::"S"(a3):"%ebx");
	asm volatile("movl %0,%%edi"::"S"(a4):"%ebx");

	// 1. save eflags
	// 2. save user space %esp in %ebp passed into sysenter_handler
	// 3. save the fifth parameter on the stack cuz there is no
	// idle register to pass it
	asm volatile("pushfl");
	asm volatile("pushl %ebp");
	asm volatile("pushl %0"::"S"(a5));
	asm volatile("add $4,%esp");
	asm volatile("movl %esp,%ebp");

	// save user space %eip in %esi passed into sysenter_handler
	asm volatile("leal .syslabel,%%esi":::"%esi");
	asm volatile("sysenter \n\t"
				 ".syslabel:"
			:
			: "a" (num)
			: "memory");
	
	// retrieve return value
	asm volatile("movl %%eax,%0":"=r"(ret));
	
	// restore %ebp and shift for eflags
	asm volatile("popl %ebp");
	asm volatile("add $0x4,%esp");

	if(check && ret > 0)
		panic("syscall %d returned %d (> 0)", num, ret);
	return ret;
}

void
sys_cputs(const char *s, size_t len)
{
	syscall(SYS_cputs, 0, (uint32_t)s, len, 0, 0, 0);
}

int
sys_cgetc(void)
{
	return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0);
}

int
sys_env_destroy(envid_t envid)
{
	return syscall(SYS_env_destroy, 1, envid, 0, 0, 0, 0);
}

envid_t
sys_getenvid(void)
{
	 return syscall(SYS_getenvid, 0, 0, 0, 0, 0, 0);
}

void
sys_yield(void)
{
	syscall(SYS_yield, 0, 0, 0, 0, 0, 0);
}

int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	return syscall(SYS_page_alloc, 1, envid, (uint32_t) va, perm, 0, 0);
}

int
sys_page_map(envid_t srcenv, void *srcva, envid_t dstenv, void *dstva, int perm)
{
	return syscall(SYS_page_map, 1, srcenv, (uint32_t) srcva, dstenv, (uint32_t) dstva, perm);
}

int
sys_page_unmap(envid_t envid, void *va)
{
	return syscall(SYS_page_unmap, 1, envid, (uint32_t) va, 0, 0, 0);
}

// sys_exofork is inlined in lib.h

int
sys_env_set_status(envid_t envid, int status)
{
	return syscall(SYS_env_set_status, 1, envid, status, 0, 0, 0);
}

int
sys_env_set_pgfault_upcall(envid_t envid, void *upcall)
{
	return syscall(SYS_env_set_pgfault_upcall, 1, envid, (uint32_t) upcall, 0, 0, 0);
}

int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, int perm)
{
	return syscall(SYS_ipc_try_send, 0, envid, value, (uint32_t) srcva, perm, 0);
}

int
sys_ipc_recv(void *dstva)
{
	return syscall(SYS_ipc_recv, 1, (uint32_t)dstva, 0, 0, 0, 0);
}

int
sys_fork(unsigned char end[])
{
	return syscall(SYS_fork,1,(uint32_t)end,0,0,0,0);
}

