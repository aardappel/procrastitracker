// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifdef _DEBUG
    #define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
    #define new DEBUG_NEW
#endif

// Allow use of features specific to Windows Vista or later.
#ifndef WINVER				
#define WINVER _WIN32_WINNT_VISTA                  		
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <Psapi.h>
#include <ddeml.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <oleacc.h>
#include <Xinput.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
// #include <new.h>
#include <math.h>
#include <assert.h>

#include <string>
#include <algorithm>

#include "zlib.h"

const int MAXTMPSTR = 1000;

const int MAXCTRLS = 3; // XInput controller indexes are 0-3

// TODO: reference additional headers your program requires here
