/***************************************************************************
    NWNX Profiler - Responsible for the actual hooking
	Copyright (C) 2003 Ingmar Stieger (Papillon, papillon@blackdagger.com)
    Copyright (C) 2006 Ingmar Stieger (Papillon, papillon@nwnx.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***************************************************************************/

#if !defined(HOOK_H_INCLUDED)
#define HOOK_H_INCLUDED

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <detours/detours.h>
#include "../plugin.h"
#include "../../misc/log.h"

typedef unsigned long dword;
typedef unsigned short int word;
typedef unsigned char byte;

void HookRunScript();


#endif