/*
  Copyright (C) 2004 Valery Reznic
  This file is part of the Elf Statifier project
  
  This project is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License.
  See LICENSE file in the doc directory.
*/

/* This file define per-processor things for alpha */

#ifndef processor_h
#define processor_h

	#define REGISTER_SIZE 8

	#define SYSCALL_REG   (EF_V0)
	#define PC_REG        (EF_PC)
	#define PC_OFFSET_AFTER_SYSCALL 4

#endif /* processor_h */

