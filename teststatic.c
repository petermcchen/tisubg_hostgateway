#include "stdio.h"
main()
{
	foo(0);
	foo(1);
	foo(2);
	foo(3);
}
foo(int val)
{
	static int s1 = 0;
	int s2 = 0;
	int s3;

	s1 += val;
	s2 += val;
	s3 += val;
	printf("s1 : %d\n", s1);
	printf("s2 : %d\n", s2);
	printf("s3 : %d\n", s3);
}
