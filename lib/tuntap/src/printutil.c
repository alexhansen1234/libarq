#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

FILE * err_file = NULL;
FILE * debug_file = NULL;

void print_error( const char * fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

  if( err_file != NULL)
    vfprintf( err_file, fmt, ap );
  else
    vfprintf( stderr, fmt, ap );
}

void print_debug( const char * fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

  if(debug_file != NULL)
    vfprintf( debug_file, fmt, ap );

}

int set_err_file( const char * path )
{
  FILE * err_file_ptr;

  if( path != NULL )
  {
    err_file_ptr = fopen(path, "a");
    if( err_file_ptr == NULL )
      return 1;
    else
      err_file = err_file_ptr;
  }
  else
  {
    err_file = NULL;
  }

  return 0;
}

int set_debug_file( const char * path )
{
  FILE * debug_file_ptr;

  if( path != NULL )
  {
    debug_file_ptr = fopen(path, "a");
    if( debug_file_ptr == NULL )
      return 1;
    else
      debug_file = debug_file_ptr;
  }
  else
  {
    debug_file = NULL;
  }

  return 0;
}
