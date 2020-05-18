// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	extern volatile pte_t uvpt[];
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	void *tmp,*origin;
	
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// verify write operation and COW bit
	if(!(err&FEC_WR)||!(uvpt[((uint32_t)addr)>>PGSHIFT]&PTE_COW))
		panic("error");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	tmp=(void*)PFTEMP;
	origin=(void*)ROUNDDOWN((uint32_t)addr,PGSIZE);

	// allocate a page onto tmp region to accommodate copied page
	if(sys_page_alloc(0,tmp,PTE_W)<0)
		panic("error");
	// copy original page to tmp region
	memmove(tmp,origin,PGSIZE);
	// map original va to newly copyied page
	if(sys_page_map(0,tmp,0,origin,PTE_W)<0)
		panic("error");
	// unmap tmp, finish copy-on-write
	if(sys_page_unmap(0,tmp)<0)
		panic("error");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 4: Your code here.
	extern volatile pte_t uvpt[];
	void* addr=(void*)(pn*PGSIZE);
	pte_t pte=uvpt[pn];

	// duppage only if the page is present and belongs to user
	if((pte&PTE_P)&&(pte&PTE_U)){
		if((pte&PTE_W)||(pte&PTE_COW)){
			if(sys_page_map(0,addr,envid,addr,PTE_COW)<0)
				panic("error");
			if(sys_page_map(0,addr,0,addr,PTE_COW)<0)
				panic("error");
		}else{
			if(sys_page_map(0,addr,envid,addr,0)<0)
				panic("error");
		}
	}	
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	extern unsigned char end[];
	envid_t childid;
	uint32_t addr;
	
	// set handler every time
	// to ensure it non-null
	set_pgfault_handler(pgfault);

	// perform fork
	if((childid=sys_fork(end))<0)
		return -1;

	// child process
	if(childid==0){
		// update thisenv global variable
		thisenv=&envs[ENVX(sys_getenvid())];

		return 0;
	}

	return childid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
