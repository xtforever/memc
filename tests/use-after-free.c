#include "mls.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>



#define expect_fail(a) wrap_expect_test( a, # a, 1 )
#define expect_ok(a) wrap_expect_test( a, # a, 0 )


int _expect_test( void (*fn) (void), const char *name, int res )
{
	printf("running: %s\n", name );
	
	int pid;
	if( (pid=fork()) == 0 ) {
		fn();
		_exit(0);
	}
		
	if( pid < 0 ) return 1;
	
	
	int status;
	pid_t w = waitpid(pid, &status, 0); // Wait for the specific child
	if (w == -1) {
		perror("waitpid failed");
		return 1;
	}
	if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status); // Fetch the exit code
		printf("Child process (PID: %d) exited with code %d\n", pid, exit_code);

		if( (res && exit_code) || (!res && !exit_code) ) {
			printf("success\n");
			return 0;
		} else {
			printf("error\n");
			return 1;
		}
	}

	printf("Child process (PID: %d) did not exit normally\n", pid);
	return 1;
}

void wrap_expect_test( void (*fn) (void), const char *name, int res )
{
	int r  = _expect_test(fn,name,res);
	if( r ) {
		printf("\n\nmls test halted, because function '%s' did not exit as expected\n", name );
		_exit(1);
	}
	
}

void test1(void)
{
	int p1=m_create(1,1);
	m_free(p1);
	int p2 = m_create(1,1);

	ASERR(p1!=p2, "p1=%d,p2=%d",p1,p2);
	m_free(p2);

	printf("p1=%d, p2=%d\n", p1, p2 );
}

/* create a random alloc/access/free pattern
   
 */

void test2(void)
{
	int rnd[2000] = { 0 };
	clock_t start_time = clock();
	clock_t end_time = 1 * CLOCKS_PER_SEC + start_time; 
	
	long r=0,a=0,f=0;
	
	while( clock() < end_time ) {	
		int slot = random() % 40;
		int access = random() % 10;
		if( rnd[slot] ) {
			if( access ) {
				r++;
				m_buf( rnd[slot] );
			} else {
				f++;
				m_free( rnd[slot] );
				rnd[slot] = 0;
			}
		} else {
			a++;
			rnd[slot] = m_create(1,1);
		}
	}
		
	for( int i =0; i < sizeof(rnd)/sizeof(rnd[0]); i++ ) {
		m_free( rnd[i] );
	}

	printf( "Read:%ld, Alloc:%ld, Free:%ld\n", r, a, f );
}





void test3(void)
{

	int p1=m_create(1,1);
	printf("%d\n", p1 >> 24 );
	p1 += 1 << 24; /* force different uaf protection pattern */
	printf("%d\n", p1 >> 24 );
	m_buf(p1);
}

void equals(int vs, char *name, char *val )
{
	char *s = v_get(vs,name,1);
	if( strcmp( s, val ) ) {
		printf("mismatch: expected: '%s', got: '%s'", val, s );
		_exit(1);
	}	
}

void test4(void)
{
	int p;
	char **s = NULL;
	int m=m_create(1,sizeof(char*));
	m_put(m, &s );
	int m1 = m_regex( m, "([^nN]+)(.*)-([a-z[:digit:]]*)", "startNext-1a2b345" );
	m_foreach( m1, p, s ) {
		char *str = *s;
		printf("match: %s\n", str );
	}
	
	ASSERT( m1 == m );
	trace_level=1;
	int v = v_init();
	v_set(v,"v1", STR(m1,1), -1 );
	v_set(v,"v2", STR(m1,2), -1 );
	v_set(v,"v3", STR(m1,3), -1 );
	m_free_strings( m, 0 );
	
	equals( v,"v1", "start" );
	equals( v,"v2", "Next" );
	equals( v,"v3", "1a2b345" );       	
	v_free(v);
	m_destruct();
}




	
int main(void)
{
	m_init();

	expect_ok(test4);
	expect_ok( test1 );
	expect_ok( test2 );
	expect_fail( test3 );
	
	m_destruct();
	return 0;
}
