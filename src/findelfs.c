/* -*- mode: c; -*- */

#include <stdlib.h>
#include "aa-elf-util.h"


/* ========================================================================== */


/* -------------------------------------------------------------------------- */

  int
main( int argc, char * argv[], char ** envp )
{
  print_elfs_recur( argv + 1, argc - 1 );
  return EXIT_SUCCESS;
}


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

/* vim: set filetype=c : */
