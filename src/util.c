/* -*- mode: c; -*- */

/* ========================================================================== */

#include "aa-elf-util.h"
#include <libelf.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <link.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fts.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>


/* -------------------------------------------------------------------------- */

/**
 * This function is run when this shared library is loaded.
 * It is required for `elf_version' to be called at some point before almost
 * any `libelf' APIs can be used; so it's good to just knock this out early.
 */
static void validate_libelf_version( void ) __attribute__(( constructor ));

  static void
validate_libelf_version( void )
{
  if ( elf_version( EV_CURRENT ) == EV_NONE )
    {
      fprintf( stderr, "`libelf' library is out of date!\n" );
      exit( EXIT_FAILURE );
    }
}


/* -------------------------------------------------------------------------- */

/**
 * Note that this return `true' for Object Files, Shared Objects, Executables,
 * and ( most importantly ) for AR archives containing Object Files as well!
 *
 * Actually `arelfp' is pretty redundant, because we could just check the output
 * of `elf_kind' to distinguish between AR archives and the other types.
 */
  bool
elfp( const char * fname )
{
  Elf_Kind k = ELF_K_NONE;
  Elf * e = NULL;
  int fd = open( fname, O_RDONLY );

  if ( fd == -1 )  /* Failed to open file. */
    {
      return false;
    }

  e = elf_begin( fd, ELF_C_READ, (Elf *) NULL );
  if ( e == NULL )  /* Not an ELF file. */
    {
      //printf( "%s\n", elf_errmsg( elf_errno() ) );
      close( fd );
      return false;
    }

  k = elf_kind( e );

  elf_end( e );
  e = NULL;
  close( fd );

  return ( k == ELF_K_ELF );
}


/* -------------------------------------------------------------------------- */

#define AR_MAGIC      "!<arch>"
#define AR_MAGIC_SIZE ( sizeof( AR_MAGIC ) - 1 )

  bool
arp( const char * fname )
{
  char buf[AR_MAGIC_SIZE];
  FILE * f = fopen( fname, "rb" );
  fread( & buf, AR_MAGIC_SIZE, 1, f );
  fclose( f );
  return ( memcmp( buf, AR_MAGIC, AR_MAGIC_SIZE ) == 0 );
}

typedef struct {
  char   name[PATH_MAX];
  time_t date;
  uid_t  uid;
  gid_t  gid;
  mode_t mode;
  off_t  size;
  union {
    char raw[60];
    struct {
      char name[16];
      char date[12];
      char uid[6];
      char gid[6];
      char mode[8];
      char size[10];
      char magic[2];
    } formatted;
  } buf;
} ar_member_t;

typedef struct {
        int      fd;
  const char   * fname;
        size_t   skip;
        char   * extfn;
        bool     verbose;
} ar_handle_t;

  static bool
ar_open_fd( const char * filename, int fd, ar_handle_t * handle, bool verbose )
{
	char buf[AR_MAGIC_SIZE];

	if ( ( read( fd, buf, AR_MAGIC_SIZE ) != AR_MAGIC_SIZE ) ||
	     ( strncmp( buf, AR_MAGIC, AR_MAGIC_SIZE ) != 0 )
     ) return false;

	handle->fname   = filename;
	handle->fd      = fd;
	handle->skip    = 0;
	handle->extfn   = NULL;
	handle->verbose = verbose;

	return true;
}

  static bool
ar_open( const char * filename, ar_handle_t * handle, bool verbose )
{
	int fd = -1;

	if ( ( fd = open( filename, O_RDONLY ) ) == -1 )
    {
      fprintf( stderr, "%s: could not open\n", filename);
    }

	if ( ! ar_open_fd( filename, fd, handle, verbose ) )
    {
      close( fd );
      return false;
    }

	return true;
}

  static bool
ar_next( ar_handle_t * ar, ar_member_t * member )
{
	char    * s   = NULL;
	ssize_t   len = 0;

	if ( ar->skip && lseek( ar->fd, ar->skip, SEEK_CUR ) == -1 )
    {
    close_and_ret:
      free( ar->extfn );
      close( ar->fd );
      ar->extfn = NULL;
      ar->fd    = -1;
      return false;
    }

	if ( read( ar->fd, member->buf.raw,
             sizeof( member->buf.raw )
           ) != sizeof( member->buf.raw )
     )
    {
      goto close_and_ret;
    }

	/* ar header starts on an even byte (2 byte aligned)
	 * '\n' is used for padding */
	if ( member->buf.raw[0] == '\n' )
    {
      memmove( member->buf.raw, member->buf.raw + 1, 59 );
      if ( read( ar->fd, member->buf.raw + 59, 1 ) != 1 ) goto close_and_ret;
    }

	if ( ( member->buf.formatted.magic[0] != '`' ) ||
       ( member->buf.formatted.magic[1] != '\n' )
     )
    {
      /* When dealing with corrupt or random embedded cross-compilers, they may
       * be abusing the archive format; only complain when in verbose mode. */
      if ( ar->verbose )
        {
          fprintf( stderr, "%s: invalid ar entry\n", ar->fname );
        }
      goto close_and_ret;
    }

	if ( ( member->buf.formatted.name[0] == '/' ) &&
       ( member->buf.formatted.name[1] == '/' )
     )
    {
      if ( ar->extfn != NULL )
        {
          fprintf( stderr,
                   "%s: Duplicate GNU extended filename section\n",
                   ar->fname
                 );
          goto close_and_ret;
        }

      len = atoi( member->buf.formatted.size );
      ar->extfn = malloc( sizeof( char ) * ( len + 1 ) );
      assert( ar->extfn != NULL );  /* FIXME */

      if ( read( ar->fd, ar->extfn, len ) != len ) goto close_and_ret;
      ar->extfn[len--] = '\0';
      for (; len > 0; len--)
        {
          if ( ar->extfn[len] == '\n' ) ar->extfn[len] = '\0';
        }
      ar->skip = 0;
      return ar_next( ar, member );
    }

	s = member->buf.formatted.name;
	if ( ( s[0] == '#' ) && ( s[1] == '1' ) && ( s[2] == '/' ) )
    {
      /* BSD extended filename, always in use on Darwin */
      len = atoi( s + 3 );
      if ( len <= (ssize_t) sizeof( member->buf.formatted.name ) )
        {
          if ( read( ar->fd, member->buf.formatted.name, len ) != len )
            {
              goto close_and_ret;
            }
        }
      else
        {
          s = alloca( sizeof( char ) * len + 1 );
          if ( read( ar->fd, s, len ) != len ) goto close_and_ret;
          s[len] = '\0';
        }
    }
  else if ( ( s[0] == '/' ) && ( s[1] >= '0' ) && ( s[1] <= '9' ) )
    {
      /* GNU extended filename */
      if ( ar->extfn == NULL )
        {
          fprintf( stderr,
                   "%s: GNU extended filename without special data section\n",
                   ar->fname
                 );
          goto close_and_ret;
        }
      s = ar->extfn + atoi( s + 1 );
    }

	snprintf( member->name, sizeof( member->name ), "%s:%s", ar->fname, s );
	member->name[sizeof( member->name ) - 1] = '\0';
	if ( ( s = strchr( member->name + strlen( ar->fname ), '/' ) ) != NULL )
    {
      *s = '\0';
    }
	member->date = atoi( member->buf.formatted.date );
	member->uid  = atoi( member->buf.formatted.uid );
	member->gid  = atoi( member->buf.formatted.gid );
	member->mode = strtol( member->buf.formatted.mode, NULL, 8 );
	member->size = atoi( member->buf.formatted.size );
	ar->skip = member->size - len;

	return true;
}


/* -------------------------------------------------------------------------- */

  bool
arelfp( const char * fname )
{
  ElfW(Ehdr)    header;
  ar_handle_t   handle;
  ar_member_t   member;
  struct stat   st;
  char        * ar_buffer = NULL;

  if ( ! ar_open( fname, & handle, true ) ) return false;

  fstat( handle.fd, & st );

  ar_buffer = mmap( 0, st.st_size, PROT_READ, MAP_PRIVATE, handle.fd, 0 );

  while ( ar_next( & handle, & member ) )
    {
      off_t cur_pos = lseek( handle.fd, 0, SEEK_CUR );
      assert( cur_pos != -1 );  /* FIXME */
      if ( member.size < sizeof( header ) ) continue;
      memcpy( & header, ( ar_buffer + cur_pos ), sizeof( header ) );
      if ( memcmp( header.e_ident, ELFMAG, SELFMAG ) == 0 )
        {
          munmap( ar_buffer, st.st_size );
          free( handle.extfn );
          handle.extfn = NULL;
          close( handle.fd );
          handle.fd = -1;
          return true;
        }
    }

  munmap( ar_buffer, st.st_size );
  return false;
}


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
static bool dev_lst_mark( dev_lst *, dev_t, ino_t ) __attribute__(( nonnull ));
static void dev_lst_realloc( dev_lst *, size_t ) __attribute__(( nonnull ));
static void dev_lst_free( dev_lst * ) __attribute__(( nonnull ));

  static void
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

  static bool
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

  static void
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

  void
do_print_elf_objects( const char * fpath, void * _unused )
{
  if ( elfp( fpath ) || arelfp( fpath ) )
    {
      /* We have a hit! */
      printf( "%s\n", fpath );
    }
}


  void
print_elfs_recur( char * const * paths, int pathc )
{
  map_files_recur( paths, pathc, do_print_elf_objects, NULL );
}



/* -------------------------------------------------------------------------- */


  void
map_files_recur( char * const * paths, int pathc, do_file_fn fn, void * aux )
{
  char    ** abspaths = NULL;
  char    *  fpath    = NULL;
  FTS     *  fs       = NULL;
  FTSENT  *  child    = NULL;
  FTSENT  *  parent   = NULL;
  dev_lst *  visited  = NULL;

  /* Initialize the marker list */
  visited = malloc( sizeof( dev_lst ) );
  assert( visited != NULL );
  visited->nodes = NULL;
  visited->ncnt  = 0;
  visited->nxt   = NULL;
  dev_lst_realloc( visited, DEV_LST_DEFAULT_SIZE );

  /* Convert any relative paths to absolute paths */
  abspaths = malloc( sizeof( char * ) * pathc );
  assert( abspaths != NULL );
  for ( int i = 0; i < pathc; i++ )
    {
      abspaths[i] = realpath( paths[i], NULL );
      assert( abspaths[i] != NULL );
    }

  fs = fts_open( abspaths, FTS_LOGICAL|FTS_COMFOLLOW, NULL );
  assert( fs != NULL );

  while ( ( parent = fts_read( fs ) ) != NULL )
    {
      errno = 0;
      child = fts_children( fs, 0 );
      if ( errno != 0 ) perror( "fts_children" );

      /* The first file opened is used to assign the first device in the
       * marker list */
      if ( visited->ncnt <= 0 ) visited->dev = child->fts_statp->st_dev;

      while ( child != NULL )
        {
          if ( dev_lst_mark( visited,
                             child->fts_statp->st_dev,
                             child->fts_statp->st_ino
                           )
             )
          {
            child = child->fts_link;
            continue;
          }

          /* Paste together the path and filenames */
          asprintf( & fpath, "%s%s", child->fts_path, child->fts_name );
          fn( fpath, aux );  /* Apply the provided function */

          free( fpath );
          fpath = NULL;
          child = child->fts_link;
        }
    }

  /* Cleanup */
  parent = NULL;
  child  = NULL;

  for ( int i = 0; i < pathc; i++ ) free( abspaths[i] );
  free( abspaths );
  abspaths = NULL;

  dev_lst_free( visited );
  visited = NULL;

  fts_close( fs );
  fs = NULL;
}


/* -------------------------------------------------------------------------- */

struct fn_aux_s { do_file_fn fn; void * aux; };


  static void
do_to_elfs( const char * fname, void * aux )
{
  if ( elfp( fname ) )
    {
      ( (struct fn_aux_s *) aux )->fn( fname,
                                       ( (struct fn_aux_s *) aux )->aux
                                     );
    }
  else if ( arelfp( fname ) )
    {
      /* FIXME: Split AR members into temporary files and and apply */
    }
}

  void
map_elfs_recur( char * const * paths, int pathc, do_file_fn fn, void * aux )
{
  struct fn_aux_s user_args = { fn, aux };
  map_files_recur( paths, pathc, do_to_elfs, & user_args );
}


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

/* vim: set filetype=c : */
