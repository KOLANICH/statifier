/*
  Copyright (C) 2004 Valery Reznic
  This file is part of the Elf Statifier project
  
  This project is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License.
  See LICENSE file in the doc directory.
*/

/*
 * This program create new load segment for the "pseudo_static" exe
 * Segment contains following:
 *    - changed elf header
 *    - changed phdrs
 *    - starter program, which restore registers
 *    - registers' values
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <elf.h>
#include <link.h> /* I need it for define ElfW() */

static FILE *my_fopen(
	const char *path,
	const char *mode,
	const char *pgm_name
)
{
	FILE *file;
	file = fopen(path, mode);
	if (file == NULL) {
		fprintf(
			stderr,
			"%s: Can't open '%s' file. Errno = %d (%s)\n",
		        pgm_name, path, errno, strerror(errno)
		);	
		return NULL;
	}
	return file;
}

static size_t my_fread(
		void *      ptr, 
		size_t      nmemb, 
		FILE *      file, 
		const char *item,
		const char *pgm_name,
		const char *file_name
)
{
	size_t result;
	result = fread(ptr, 1, nmemb, file);
	if (result != nmemb) {
		fprintf(
			stderr, 
			"%s: can't read '%s' from file '%s'. Errno=%d, (%s).\n",
			pgm_name, item, file_name, errno, strerror(errno)
		);
		return 0;
	}	
	return result;
}

static int my_fseek(
		FILE *      file, 
		long        offset,
		const char *pgm_name,
		const char *file_name
)
{
	int result;
	result = fseek(file, offset, SEEK_SET);
	if (result == -1) {
		fprintf(
			stderr,
			"%s: Can't fseek in the file '%s' to the pos=%ld. Errno = %d (%s)\n",
			pgm_name, file_name, offset, errno, strerror(errno)
		);
		return -1;
	}
	return result;
}

void *my_malloc(size_t size, const char *item, const char *pgm_name)
{
	void *result;
	result = malloc(size);
	if (result == NULL) {
		fprintf(
			stderr,
			"%s: Can't malloc %lu byte for '%s'.\n",
			pgm_name, (unsigned long)size, item
		);
		return NULL;
	}
	return result;
}

static int get_ehdr_phdrs_and_shdrs(
	const char *path,
	const char *pgm_name,
	ElfW(Ehdr) *ehdr,
	ElfW(Phdr) **phdrs,
	int        *first_load_segment,
	ElfW(Shdr) **shdrs
)
{
	FILE *file;
	size_t result;
	size_t phdrs_size, shdrs_size;
	size_t index;

	file = my_fopen(path, "r", pgm_name);
	if (file == NULL) return 0;

	result = my_fread(ehdr, sizeof(*ehdr), file, "ehdr", path, pgm_name);
	if (result == 0) return 0;

	if ( ehdr->e_phentsize == 0) {
		fprintf(
			stderr,
			"%s: in the file '%s' e_phentsize == 0\n",
			pgm_name, path
		);
		return 0;
	}

	if ( ehdr->e_phnum == 0) {
		fprintf(
			stderr,
			"%s: in the file '%s' e_phnum == 0\n",
			pgm_name, path
		);
		return 0;
	}
	phdrs_size = ehdr->e_phentsize * ehdr->e_phnum;
	*phdrs = my_malloc(phdrs_size, "phdrs", pgm_name);
	if (*phdrs == NULL) return 0;

	if (my_fseek(file, ehdr->e_phoff, pgm_name, path) == -1) return 0;
	if (my_fread(*phdrs, phdrs_size, file, "phdrs", pgm_name, path) == 0) return 0;
	for (index = 0; index < ehdr->e_phnum; index++) {
		if ( (*phdrs)[index].p_type == PT_LOAD) {
			if (first_load_segment != NULL) {
				*first_load_segment = index;
			}
			break;
		}
	}

	if (shdrs != NULL) {
		shdrs_size = ehdr->e_shentsize * ehdr->e_shnum;
		*shdrs = my_malloc(shdrs_size, "shdrs", pgm_name);
		if (*shdrs == NULL) return 0;

		if (my_fseek(file, ehdr->e_shoff, pgm_name, path) == -1) return 0;
		if (my_fread(*shdrs, shdrs_size, file, "shdrs", pgm_name, path) == 0) return 0;
	}

	fclose(file);
	return 1;
}

static off_t my_file_size(const char *path, const char *pgm_name, int *err)
{
	struct stat buf;
	int result;
	result = stat(path, &buf);
	if (result == -1) {
		*err = 1;
		fprintf(
			stderr,
			"%s: can't fstat file '%s'. Errno = %d (%s).\n",
			pgm_name, path, errno, strerror(errno)
		);
		return -1;
	}
	*err = 0;
	return buf.st_size;
}
int main(int argc, char *argv[])
{
	ElfW(Ehdr) ehdr_exe,   ehdr_core;
	ElfW(Phdr) *phdrs_exe, *phdrs_core, *phdrs_out, *ph_in, *ph_out;
	ElfW(Shdr) *shdrs_exe;
	int first_load_segment;
	FILE *input;
	FILE *output = stdout;
	int result;
	const char *pgm_name = argv[0];	
	const char *exe_in, *core, *starter;
	size_t ind_core, ind_out, num_seg_out;
	size_t num_load_segment_in_core;
	size_t align, rest;
	off_t  starter_pgm_size, file_size, starter_seg_size;
	static int err;
	const char *s_ignored_segments;
	int ignored_segments;
	int arg_ind;
	char *starter_segment, *cur_ptr;
	size_t cur_size;

	if (argc < 5) {
		fprintf(stderr, "Usage: %s <exe_in> <gdb_core_file> <starter_program> <ignored_seg> <seg_file_1> [<seg_file_2>...]\n", pgm_name);
		exit(1);
	}

	arg_ind            = 1;
	exe_in             = argv[arg_ind++];
	core               = argv[arg_ind++];
	starter            = argv[arg_ind++];
	s_ignored_segments = argv[arg_ind++];

	ignored_segments = atoi(s_ignored_segments);
	if (ignored_segments <= 0) {
		fprintf(
			stderr,
			"%s: ignored_segment='%s', should be > 0\n",
			pgm_name, s_ignored_segments
		);
		exit(1);
	}

	if ( 
		get_ehdr_phdrs_and_shdrs(
			exe_in, 
			pgm_name, 
			&ehdr_exe,
			&phdrs_exe,
			&first_load_segment,
			&shdrs_exe
		) == 0
	) exit(1);

	if ( 
		get_ehdr_phdrs_and_shdrs(
			core, 
			pgm_name, 
			&ehdr_core, 
			&phdrs_core,
			NULL,
			NULL
		) == 0
	) exit(1);

	phdrs_out = my_malloc(
		ehdr_core.e_phentsize * ehdr_core.e_phnum,
		"phdrs for output exe file",
		pgm_name
	);
	if (phdrs_out == NULL) exit(1);

	ph_out   = &phdrs_out[1]; /* leave place for starter segment */
	num_load_segment_in_core = 0;
	for (
		ind_core = 0, ph_in = phdrs_core; 
		ind_core < ehdr_core.e_phnum;
		ind_core++, ph_in++
	) {
		if (ph_in->p_type == PT_LOAD) {
			ph_out->p_type  = PT_LOAD;
			ph_out->p_vaddr = ph_in->p_vaddr;
			ph_out->p_paddr = ph_in->p_vaddr;
			ph_out->p_flags = ph_in->p_flags;
			ph_out->p_align = 1; 	
			num_load_segment_in_core++;
			ph_out++;
		}
	}

	if ((num_load_segment_in_core - ignored_segments) != (argc - arg_ind)) {
		fprintf(
			stderr,
			"%s: mismatch: core file '%s' has %lu LOAD segments but command line supply ignored_segments='%d' and %d files\n",
			pgm_name,
			core,
			(unsigned long)num_load_segment_in_core,
			ignored_segments,
			argc - arg_ind
		);
		exit(1);
	}

	num_seg_out = 
		num_load_segment_in_core
		+ 1 /* My segment */
		- ignored_segments /* last segment with stack, etc */
	;

	align = phdrs_exe[first_load_segment].p_align;

	starter_pgm_size = my_file_size(starter, pgm_name, &err);
	if (err != 0) exit(1);

	/* Starter seg size is elf header size + size of all phdrs + 
	 * + size of all shdrs + starter program size
	 */
	starter_seg_size =                                \
		sizeof(ehdr_exe)                        + \
		ehdr_exe.e_shnum * ehdr_exe.e_shentsize + \
	       	num_seg_out      * sizeof(*ph_out)      + \
		starter_pgm_size
	;

	/* Now round it up to the align boundary if needed*/
	rest = starter_seg_size % align;
	if (rest) starter_seg_size += (align - rest);

	/* Fill in first phdr */
	phdrs_out[0].p_type   = PT_LOAD;
	phdrs_out[0].p_offset = 0;
	phdrs_out[0].p_vaddr  = phdrs_out[1].p_vaddr - starter_seg_size;
	phdrs_out[0].p_paddr  = phdrs_out[1].p_vaddr - starter_seg_size;
	phdrs_out[0].p_filesz = starter_seg_size;
	phdrs_out[0].p_memsz  = starter_seg_size;
	phdrs_out[0].p_flags  = PF_X | PF_R; 
	phdrs_out[0].p_align  = 1;

	/* Fill Size and offset for others */
	for (ind_out = 1; ind_out < num_seg_out; ind_out++) {
		file_size = my_file_size(argv[arg_ind], pgm_name, &err);
		arg_ind++;
		if (err != 0) exit(1);

		phdrs_out[ind_out].p_filesz = file_size;
		phdrs_out[ind_out].p_memsz  = file_size;
		phdrs_out[ind_out].p_offset = phdrs_out[ind_out - 1].p_offset + phdrs_out[ind_out - 1].p_filesz;
	}

	/* Adjust shdrs */
	for (ind_out = 0; ind_out < ehdr_exe.e_shnum; ind_out++) {
		shdrs_exe[ind_out].sh_offset += starter_seg_size;
	}

	/* Adjust Ehdr */
	ehdr_exe.e_entry = 
		phdrs_out[0].p_vaddr                    +
		sizeof(ehdr_exe)                        + 
		num_seg_out * sizeof(*ph_out)           + 
		ehdr_exe.e_shentsize * ehdr_exe.e_shnum
	;
	ehdr_exe.e_phoff = sizeof(ehdr_exe);
	ehdr_exe.e_shoff = ehdr_exe.e_phoff + num_seg_out * sizeof(*ph_out);
	ehdr_exe.e_phnum = num_seg_out;

	/* Allocate space for the starter segment */
	starter_segment = my_malloc(starter_seg_size, "starter_segment", pgm_name);
	if (starter_segment == NULL) exit(1);
	memset(starter_segment, 0, starter_seg_size);

	cur_ptr  = starter_segment;
	cur_size = sizeof(ehdr_exe);
	memcpy(cur_ptr, &ehdr_exe, cur_size);
	cur_ptr += cur_size;
	cur_size = num_seg_out * sizeof(*ph_out);
	memcpy(cur_ptr, phdrs_out, cur_size);
	/* Add schdrs */
	cur_ptr += cur_size;
	cur_size = ehdr_exe.e_shentsize * ehdr_exe.e_shnum;
	memcpy(cur_ptr, shdrs_exe, cur_size);

	/* Read starter program in */
	cur_ptr += cur_size;
	input = my_fopen(starter, "r", pgm_name);
	if (input == NULL) exit(1);

	result = my_fread(cur_ptr, starter_pgm_size, input, "all file", starter, pgm_name);
	if (result == 0) exit(1);
	fclose(input);

	fwrite(starter_segment, 1, starter_seg_size, output);
	
 	exit(0);
	return 0;
}
