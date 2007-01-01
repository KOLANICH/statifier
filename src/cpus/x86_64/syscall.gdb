# Copyright (C) 2004, 2005 Valery Reznic
# This file is part of the Elf Statifier project
# 
# This project is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License.
# See LICENSE file in the doc directory.

# Syscalls defines forx86_64 
# syscall instruction is 'syscall'
define is_it_syscall
	is_it_syscall_2 0x0f 0x05
end
