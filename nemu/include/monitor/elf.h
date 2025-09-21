#ifndef __MONITOR_ELF_H__
#define __MONITOR_ELF_H__

#include <elf.h>

extern char *exec_file;
extern char *strtab;
extern Elf32_Sym *symtab;
extern int nr_symtab_entry;

void load_elf_tables(int argc, char *argv[]);

#endif
