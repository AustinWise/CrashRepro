#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#define MAPPED_MEMORY_SIZE 4096
#define MY_MSG "in handler\n"

static int *mapped_memory;
static void *alt_signal_stack;

static void segv_handler(int code, siginfo_t *siginfo, void *context)
{
	int rc;

	void *addr = siginfo->si_addr;

	if (addr != mapped_memory)
	{
		//This was not the address we were expecting.
		abort();
	}

	rc = mprotect(mapped_memory, MAPPED_MEMORY_SIZE, PROT_READ);
	if (rc != 0)
	{
		abort();
	}

	write(STDERR_FILENO, MY_MSG, sizeof(MY_MSG));
}

int main(int argc, char **argv)
{
	int rc;

	mapped_memory = mmap(NULL, MAPPED_MEMORY_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mapped_memory == MAP_FAILED)
	{
		perror("mmap");
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
	memset(&myNewAction, 0, sizeof(myNewAction));
	myNewAction.sa_sigaction = segv_handler;
	myNewAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
	rc = sigaction(SIGSEGV, &myNewAction, NULL);
	if (rc != 0)
	{
		perror("sigaction");
		return EXIT_FAILURE;
	}

	printf("reading from mapped memory: %d\n", *mapped_memory);

	puts("I am still alive!?");

	return EXIT_SUCCESS;
}
