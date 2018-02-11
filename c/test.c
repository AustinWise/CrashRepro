#include <stdio.h>
#include <pthread.h>

int main(int argc, char** argv)
{
	pthread_t self = pthread_self();
	clockid_t cid;
	struct timespec ts;

	if (pthread_getcpuclockid(self, &cid) != 0)
	{
		puts("failed to pthread_getcpuclockid");
		return 1;
	}

	if (clock_gettime(cid, &ts) != 0)
	{
		puts("failed to clock_gettime");
		return 2;
	}

	return 0;
}
