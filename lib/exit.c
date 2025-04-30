
#include <inc/lib.h>

void
exit(void)
{
	// close_all(); // TODO
	sys_env_destroy(0);
}

