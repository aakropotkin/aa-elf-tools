/* -*- mode: c; -*- */

#ifndef _LIBSCRIPTS_UTIL_H
#define _LIBSCRIPTS_UTIL_H

/* ========================================================================== */

#define _GNU_SOURCE
#include <elf.h>
#include <stdlib.h>
#include <stdbool.h>


/* -------------------------------------------------------------------------- */

/** Detect if the file at path `fname' has ELF format. */
bool elfp( const char * fname )   __attribute__(( nonnull ));
/** Detect if the file at path `fname' is AR archive. */
bool arp( const char * fname )    __attribute__(( nonnull ));
/** Detect if the file at path `fname' is AR archive containing ELF data. */
bool arelfp( const char * fname ) __attribute__(( nonnull ));


/* -------------------------------------------------------------------------- */

/** Detect if the file descriptor has ELF format. */
bool fdelfp( int fd );
/** Detect if the file descriptor is AR archive. */
bool fdarp( int fd );
/** Detect if the file descriptor is AR archive containing ELF data. */
bool fdarelfp( int fd );


/* -------------------------------------------------------------------------- */

/**
 * Lambda which may be applied to filepaths using `map_files_recur'.
 * FIXME: Change to accept file descriptor instead to allow members of AR
 *        archives to be mapped over as if they were regular files.
 *        The alternative is to temporarily extract members, which is
 *        less efficient.
 */
typedef void (*do_file_fn)( const char * fname, void * aux );

/** Applies a `do_file_fn' to all files. */
void map_files_recur( char * const *, int, do_file_fn, void * aux )
  __attribute__(( nonnull( 1, 3 ) ));

/** Applies `do_file_fn' to ELF files only. */
void map_elfs_recur( char * const *, int, do_file_fn, void * aux )
  __attribute__(( nonnull( 1, 3 ) ));


/* -------------------------------------------------------------------------- */

typedef void (*do_fd_fn)( int fd, void * aux );


/* -------------------------------------------------------------------------- */

void do_print_elf_objects( const char * fpath, void * _unused )
  __attribute__(( nonnull( 1 ) ));

/**
 * Print all ELF files within the directories and subdirectories name in the
 * array `paths'.
 * `pathc' indicates the number of elements in `paths'.
 */
void print_elfs_recur( char * const *, int ) __attribute__(( nonnull ));


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

#endif /* util.h */

/* vim: set filetype=c : */
