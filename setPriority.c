#include "types.h"
#include "stat.h"
#include "user.h"
int main(int argc, char* argv[])
{
	
	int old , a , b;
	a = atoi(argv[1]);
	b = atoi(argv[2]);
	old = set_priority(a, b);
	printf(1, "Old priority of pid %d was :- %d now changed to :-%d\n", b, old, a);
}