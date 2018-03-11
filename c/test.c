#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

#include "pal.h"
#include "context.h"

#define NUMBER_OF_TRIALS 4
#define MAPPED_MEMORY_SIZE 4096

static int* mapped_memory[NUMBER_OF_TRIALS];
static void *alt_signal_stack;

static intptr_t observed_rsp[NUMBER_OF_TRIALS];

static void change_protection_to_readable(void *addr, intptr_t rsp)
{
	int rc;

	bool foundIt = false;
	for (int i = 0; i < NUMBER_OF_TRIALS; i++)
	{
		if (mapped_memory[i] == addr)
		{
			observed_rsp[i] = rsp;
			foundIt = true;
			break;
		}
	}

	if (!foundIt)
	{
		//This was not the address we were expecting.
		abort();
	}

	rc = mprotect(addr, MAPPED_MEMORY_SIZE, PROT_READ | PROT_WRITE);
	if (rc != 0)
	{
		rc = errno;
		abort();
	}
}

static void segv_handler(int code, siginfo_t *siginfo, void *context)
{
	//see where we are
	ucontext_t *fault_context = (ucontext_t *)context;
	CONTEXT ctx;
	RtlCaptureContext(&ctx);

	//fix protection
	void *addr = siginfo->si_addr;
	change_protection_to_readable(addr, ctx.Rsp);

	/* Unmask signal so we can receive it again */
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, code);
	int sigmaskRet = sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
	if (sigmaskRet != 0)
	{
		abort();
	}

	if (addr == mapped_memory[0])
	{
		*mapped_memory[1] = 1;
		return;
	}
	else if (addr == mapped_memory[1])
	{
		return;
	}

	//switch back to executing on the regular stack and pretend the signal
	//never happened -_-
	CONTEXTFromNativeContext(fault_context, &ctx, CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_XSTATE);
	RtlRestoreContext(&ctx);
}

static void doRead(int id, int *addr)
{
	printf("reading from mapped memory %d: %d\n", id, *addr);
}

static int* allocate_memory()
{
	void* mem = mmap(NULL, MAPPED_MEMORY_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED)
	{
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	return (int*)mem;
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	int rc;

	for (int i = 0; i < NUMBER_OF_TRIALS; i++)
	{
		mapped_memory[i] = allocate_memory();
		observed_rsp[i] = 0;
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
	myNewAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
	rc = sigaction(SIGSEGV, &myNewAction, NULL);
	if (rc != 0)
	{
		perror("sigaction");
		return EXIT_FAILURE;
	}

	CONTEXT ctx;
	RtlCaptureContext(&ctx);
	printf("main stack: 0x%lx alt sig stack: %p\n", ctx.Rsp, alt_signal_stack);

	for	(int i = 0; i < NUMBER_OF_TRIALS; i++)
	{
		doRead(i, mapped_memory[i]);
	}

	for	(int i = 0; i < NUMBER_OF_TRIALS; i++)
	{
		printf("Observered signal stack rsp %d: 0x%lx\n", i, observed_rsp[i]);
	}

	rc = EXIT_SUCCESS;

	//Check that the nested case is different from the non-nest case.
	if (observed_rsp[0] == observed_rsp[1])
	{
		puts("Nested signal RSP matches non-nested signal!?");
		rc = EXIT_FAILURE;
	}

	//Check the rsp the first time we SIGSEGV before pivoting off the signal stack.
	if (observed_rsp[0] != observed_rsp[2])
	{
		puts("rsp[0] and [2] don't match");
		rc = EXIT_FAILURE;
	}

	//Check that the signal rsp is the same after we pivot back to the main stack
	//and SIGSEGV again.
	if (observed_rsp[0] != observed_rsp[3])
	{
		puts("rsp[0] and rsp[3] don't match");
		rc = EXIT_FAILURE;
	}

	for	(int i = 0; i < NUMBER_OF_TRIALS; i++)
	{
		ptrdiff_t diff = observed_rsp[i] - (intptr_t)alt_signal_stack;
		diff = labs(diff);
		if (diff > SIGSTKSZ)
		{
			printf("rsp[%d] is not in alt_signal_stack\n", i);
			rc = EXIT_FAILURE;
		}
	}

	if (rc == EXIT_SUCCESS)
	{
		puts("all tests pass");
	}
	else
	{
		puts("some tests failed");
	}
	
	return rc;
}
