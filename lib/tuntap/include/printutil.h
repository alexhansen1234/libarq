#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void print_error( const char * fmt, ... );
void print_debug( const char * fmt, ... );
int set_err_file( const char * path );
int set_debug_file( const char * path );
