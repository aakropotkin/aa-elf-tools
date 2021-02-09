/* -*- mode: c; -*- */

/* ========================================================================== */

#include "util.h"
#include "dev_lst.h"
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

  bool
fdelfp( int fd )
{
  ElfW(Ehdr) header;
  size_t rsl = read( fd, & header, sizeof( header ) );
  lseek( fd, SEEK_CUR, ( - rsl ) );
  return ( rsl == sizeof( header ) )                         &&
         ( memcmp( header.e_ident, ELFMAG, SELFMAG ) == 0 );
}


  bool
elfp( const char * fname )
{
  ElfW(Ehdr) header;
  FILE * f = fopen( fname, "rb" );
  if ( f == NULL ) return false;
  size_t rsl = fread( & header, sizeof( header ), 1, f );
  fclose( f );
  return ( rsl == sizeof( header ) )                         &&
         ( memcmp( header.e_ident, ELFMAG, SELFMAG ) == 0 );
}


/* -------------------------------------------------------------------------- */

#define AR_MAGIC      "!<arch>"
#define AR_MAGIC_SIZE ( sizeof( AR_MAGIC ) - 1 )


  bool
fdarp( int fd )
{
  char buf[AR_MAGIC_SIZE];
  size_t rsl = read( fd, & buf, AR_MAGIC_SIZE );
  lseek( fd, ( - rsl ), SEEK_CUR );
  return ( rsl == AR_MAGIC_SIZE )                         &&
         ( memcmp( buf, AR_MAGIC, AR_MAGIC_SIZE ) == 0 );
}


  bool
arp( const char * fname )
{
  char buf[AR_MAGIC_SIZE];
  FILE * f = fopen( fname, "rb" );
  size_t rsl = fread( & buf, AR_MAGIC_SIZE, 1, f );
  fclose( f );
  return ( rsl == AR_MAGIC_SIZE )                         &&
         ( memcmp( buf, AR_MAGIC, AR_MAGIC_SIZE ) == 0 );
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
      if ( verbose ) fprintf( stderr, "%s: could not open\n", filename);
      return false;
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
fdarelfp( int fd )
{
  ElfW(Ehdr)    header;
  ar_handle_t   handle;
  ar_member_t   member;
  struct stat   st;
  char        * ar_buffer = NULL;
  off_t         start_pos = lseek( fd, 0, SEEK_CUR );

  if ( ! ar_open_fd( "", fd, & handle, true ) ) return false;

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
          lseek( fd, start_pos, SEEK_SET );
          return true;
        }
    }

  munmap( ar_buffer, st.st_size );
  lseek( fd, start_pos, SEEK_SET );
  return false;
}


  bool
arelfp( const char * fname )
{
  int fd = -1;
	if ( ( fd = open( fname, O_RDONLY ) ) == -1 ) return false;
  bool rsl = fdarelfp( fd );
  close( fd );
  fd = -1;
  return rsl;
}


/* -------------------------------------------------------------------------- */

  void
do_print_elf_objects( const char * fpath, void * _unused )
{
  if ( elfp( fpath ) || arelfp( fpath ) )
  //if ( arelfp( fpath ) )
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

#if 0
  int
main( int argc, char * argv[], char ** envp )
{
  print_elfs_recur( argv + 1, argc - 1 );
  return EXIT_SUCCESS;
}
#endif


/* -------------------------------------------------------------------------- */



/* ========================================================================== */

/* vim: set filetype=c : */
