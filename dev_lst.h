/* -*- mode: c; -*- */

#ifndef _DEV_LST_H
#define _DEV_LST_H

/* ========================================================================== */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>


/* -------------------------------------------------------------------------- */

/** Used to track inodes which have been visited on a device. */
typedef struct _dev_lst {
  dev_t             dev;
  ino_t           * nodes;
  size_t            ncnt;
  size_t            ncap;
  struct _dev_lst * nxt;
} dev_lst;


#define DEV_LST_DEFAULT_SIZE 256

/**
 * Mark the file corresponding to the Device/Inode indicated as "visited".
 * Return true if the given file had already been visited previously.
 */
bool dev_lst_mark( dev_lst *, dev_t, ino_t ) __attribute__(( nonnull ));
void dev_lst_realloc( dev_lst *, size_t )    __attribute__(( nonnull ));
void dev_lst_free( dev_lst * )               __attribute__(( nonnull ));


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

#endif /* dev_lst.h */

/* vim: set filetype=c : */
