#!/bin/bash

# Copyright (C) 2004 Valery Reznic
# This file is part of the Elf Statifier project
# 
# This project is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License.
# See LICENSE file in the doc directory.

# It's main script

function PrepareDirectoryStructure
{
	mkdir -p $WORK_COMMON_DIR  || return
	mkdir -p $WORK_GDB_CMD_DIR || return
	mkdir -p $WORK_GDB_OUT_DIR || return
	mkdir -p $WORK_DUMPS_DIR   || return
	mkdir -p $WORK_OUT_DIR     || return
	return 0
}

function Sanity
{
	local Func=Sanity
	[ $# -ne 1 -o "x$1" = "x" ] && {
		Echo "$0: Usage $Func <OrigExe>"
		return 1
	}

	local OrigExe=$1
	[ -f $OrigExe ] || {
   		Echo "$0: '$OrigExe' not exsist or not regular file."
   		return 1
	}

	[ -x $OrigExe ] || {
   		Echo "$0: '$OrigExe' have not executable permission."
   		return 1
	}

	[ -r $OrigExe ] || {
   		Echo "$0: '$OrigExe ' have not read permission."
   		return 1
	}
	return 0
}

function GetElfClass
{
	local Func=GetElfClass
	[ $# -ne 1 -o "x$1" = "x" ] && {
		Echo "$0: Usage $Func <OrigExe>"
		return 1
	}

	local OrigExe=$1
	local res
	res="`readelf --file-header $OrigExe`" || return 
	echo "$res" | awk '{
		if ($NF == "ELF32") { print "32"; exit 0;}
		if ($NF == "ELF64") { print "64"; exit 0;}
	}' || return
	return 0
}

function Main
{
	set -e
		source $OPTION_SRC || return
	set +e
	Sanity $opt_orig_exe || return

	local ElfClass
	ElfClass=`GetElfClass $opt_orig_exe`
	[ "x$ElfClass" = "x" ] && {
		Echo "$0: Can't determine ELF CLASS for the '$opt_orig_exe'"
		return 1
	}

	D=$D/$ElfClass
	[ -d $D ] || {
		Echo "$0: ElfClass '$ElfClass' do not supported on this system."
		return 1
	}

	# Do it
	$D/statifier_common.sh         $WORK_DIR || return
	$D/statifier_before_dump.sh    $WORK_DIR || return
	$D/statifier_dump.sh           $WORK_DIR || return
	$D/statifier_before_starter.sh $WORK_DIR || return
	$D/statifier_create_starter.sh $WORK_DIR || return
	$D/statifier_create_exe.sh     $WORK_DIR || return
	return 0
}

#################### Main Part ###################################
# Temporary Work Directory
WORK_DIR="${TMPDIR:-/tmp}/statifier.tmpdir.$$"
#WORK_DIR="./.statifier"

# Where Look For Other Programs
D=`dirname $0`                 || exit
source $D/statifier_lib.src    || exit
source $D/statifier_parser.src || exit

CommandLineParsing "$@"        || exit

SetVariables $WORK_DIR         || exit 
rm -rf $WORK_DIR               || exit
PrepareDirectoryStructure      || exit
SaveOptions > $OPTION_SRC      || exit

Main
st=$? 
rm -rf $WORK_DIR || exit
exit $st
