
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>

  int
main( int argc, char * argv[], char ** envp )
{
  GElf_Shdr     shdr;
  Elf         * elf    = NULL;
  Elf_Scn     * scn    = NULL;
  Elf_Data    * data   = NULL;
  int           fd     = -1;
  int           count  = -1;
  char        * symstr = NULL;

  elf_version( EV_CURRENT );

  fd  = open( argv[1], O_RDONLY );
  elf = elf_begin( fd, ELF_C_READ, NULL );

  while ( ( scn = elf_nextscn( elf, scn ) ) != NULL )
    {
      gelf_getshdr( scn, & shdr );
      if ( shdr.sh_type == SHT_SYMTAB )
        {
          /* found a symbol table, go print it. */
          break;
        }
    }

  data  = elf_getdata( scn, NULL );
  count = shdr.sh_size / shdr.sh_entsize;

  /* print the symbol names */
  for ( int ii = 0; ii < count; ++ii )
    {
      GElf_Sym sym;
      gelf_getsym( data, ii, & sym );
      symstr = elf_strptr( elf, shdr.sh_link, sym.st_name );
      if ( ( symstr != NULL ) && ( symstr[0] != '\0' ) )
        {
          #if 0
          switch ( GELF_ST_BIND( sym.st_info ) )
            {
              case STB_GLOBAL:
              case STB_WEAK:
              case STB_LOOS:
              case STB_HIOS:
              case STB_LOPROC:
              case STB_HIPROC:
                printf( "%s\n", symstr );
                break;

              case STB_LOCAL:
              case STB_NUM:
              default:
                break;
            }
          #endif

          /* Skip undefined symbols */
          if ( sym.st_shndx == STN_UNDEF )
            {
              continue;
            }

          /* Skip locals and the fake `STB_NUM' value.
           * This represents the number of valid Symbol Binding types according
           * to the ELF Spec.
           * The remaining types are extensions. */
          if ( ( GELF_ST_BIND( sym.st_info ) == STB_LOCAL ) ||
               ( GELF_ST_BIND( sym.st_info ) == STB_NUM ) )
            {
              continue;
            }

          puts( symstr );
        }
    }
  elf_end( elf );
  close( fd );
  return EXIT_SUCCESS;
}
