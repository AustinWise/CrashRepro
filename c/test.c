#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <signal.h>

#include "pal.h"
#include "context.h"


#define MAPPED_MEMORY_SIZE 4096

static int *mapped_memory1;
static int *mapped_memory2;
static void *alt_signal_stack;

DWORD64 Rsp1;
DWORD64 Rsp2;

static void change_protection_to_readable(void* addr)
{
	int rc;

	if (addr != mapped_memory1 && addr != mapped_memory2)
	{
		//This was not the address we were expecting.
		abort();
	}

	rc = mprotect(addr, MAPPED_MEMORY_SIZE, PROT_READ);
	if (rc != 0)
	{
		abort();
	}
}

static void segv_handler(int code, siginfo_t *siginfo, void *context)
{
	//fix protection
	void *addr = siginfo->si_addr;
	change_protection_to_readable(addr);

    /* Unmask signal so we can receive it again */
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, code);
    int sigmaskRet = sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
    if (sigmaskRet != 0)
    {
		abort();
    }

	//resume execution
	ucontext_t* fault_context = (ucontext_t*)context;
	CONTEXT ctx;
	//capture first to get any missing registers
	RtlCaptureContext(&ctx);

	if (addr == mapped_memory1)
	{
		Rsp1 = ctx.Rsp;
	}
	else if (addr == mapped_memory2)
	{
		Rsp2 = ctx.Rsp;
	}

	CONTEXTFromNativeContext(fault_context, &ctx, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_XSTATE);
	RtlRestoreContext(&ctx);
}

static void doRead(int id, int *addr)
{
	printf("reading from mapped memory %d: %d\n", id, *addr);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	int rc;

	mapped_memory1 = mmap(NULL, MAPPED_MEMORY_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapped_memory1 == MAP_FAILED)
	{
		perror("mmap1");
		return EXIT_FAILURE;
	}
	mapped_memory2 = mmap(NULL, MAPPED_MEMORY_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapped_memory2 == MAP_FAILED)
	{
		perror("mmap2");
		return EXIT_FAILURE;
	}

	alt_signal_stack = malloc(SIGSTKSZ);
	if (alt_signal_stack == 0)
	{
		perror("malloc");
		return EXIT_FAILURE;
	}

	stack_t ss;
	memset(&ss, 0, sizeof(ss));
	ss.ss_size = SIGSTKSZ;
	ss.ss_sp = alt_signal_stack;
	rc = sigaltstack(&ss, NULL);
	if (rc != 0)
	{
		perror("sigaltstack");
		return EXIT_FAILURE;
	}

	struct sigaction myNewAction;
    myNewAction.sa_handler = NULL;
	myNewAction.sa_sigaction = segv_handler;
	myNewAction.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
	rc = sigaction(SIGSEGV, &myNewAction, NULL);
	if (rc != 0)
	{
		perror("sigaction");
		return EXIT_FAILURE;
	}

	CONTEXT ctx;
	RtlCaptureContext(&ctx);
	printf("current stack: 0x%lx alt sig stack: %p\n", ctx.Rsp, alt_signal_stack);

	doRead(1, mapped_memory1);
	doRead(2, mapped_memory2);

	printf("rsp1: 0x%lx, rsp2: 0x%lx\n", Rsp1, Rsp2);

	DWORD64 diff = Rsp2 - (DWORD64)alt_signal_stack;
	if (diff > 0x4000)
	{
		puts("Test failed: the second time the signal handler was called on the wrong stack");
		return EXIT_FAILURE;
	}
	else
	{
		puts("The program behaved as expected.");
		return EXIT_FAILURE;
	}
}
