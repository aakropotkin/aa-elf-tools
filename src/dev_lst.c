/* -*- mode: c; -*- */

/* ========================================================================== */

#define  _GNU_SOURCE
#include "dev_lst.h"
#include <stdlib.h>
#include <assert.h>


/* -------------------------------------------------------------------------- */

  void
dev_lst_realloc( dev_lst * elem, size_t nsize )
{
  if ( ( elem->nodes == NULL ) || ( elem->ncnt == 0 ) )
    {
      elem->nodes = malloc( sizeof( ino_t ) * nsize );
    }
  else
    {
      elem->nodes = realloc( elem->nodes, sizeof( ino_t ) * nsize );
    }
  assert( elem->nodes != NULL );
  elem->ncap = nsize;
}


/* -------------------------------------------------------------------------- */

  bool
dev_lst_mark( dev_lst * dlst, dev_t dev, ino_t ino )
{
  /* Find the device corresponding to `dev' if it exists */
  while ( ( dlst != NULL ) && ( dlst->nxt != NULL ) && ( dlst->dev != dev ) )
    {
      dlst = dlst->nxt;
    }
  if ( dlst->dev == dev )  /* Device was found */
    {
      for ( size_t i = 0; i < dlst->ncnt; i++ )  /* Find the inode */
        {
          if ( dlst->nodes[i] == ino ) return true;
        }
      /* Inode not found, add it. Doubling size of inode list if needed. */
      if ( dlst->ncap <= ( dlst->ncnt + 1 ) )
        {
          dev_lst_realloc( dlst, 2 * dlst->ncap );
        }
      dlst->nodes[dlst->ncnt++] = ino;
    }
  else  /* A new device was found, add it to the device list */
    {
      assert( dlst->nxt == NULL );
      dlst->nxt = malloc( sizeof( dev_lst ) );
      assert( dlst->nxt != NULL );
      dlst = dlst->nxt;
      dlst->dev   = dev;
      dlst->nodes = NULL;
      dlst->ncnt  = 0;
      dlst->nxt   = NULL;
      dev_lst_realloc( dlst, DEV_LST_DEFAULT_SIZE );
      dlst->nodes[0] = ino;
      dlst->ncnt++;
    }
  return false;
}


/* -------------------------------------------------------------------------- */

  void
dev_lst_free( dev_lst * dlst )
{
  free( dlst->nodes );
  dlst->nodes = NULL;
  if ( dlst->nxt != NULL )
    {
      dev_lst_free( dlst->nxt );
      dlst->nxt = NULL;
    }
  free( dlst );
}


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

/* vim: set filetype=c : */
