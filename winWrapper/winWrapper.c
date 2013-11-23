/*
//
//  winWrapper.c: wrap python(w).exe
//  Copyright (C) 2012 science+computing ag
//  Author: Anselm Kruis
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, see <http://www.gnu.org/licenses/>.
//
// 
*/

#ifndef _WIN32_WINNT            
#define _WIN32_WINNT 0x0502     // Specifies that the minimum required platform is XP SP2
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>


#define LIBEXECDIR "libexec\\"
#define INTERPRETER_NAME "python.exe"
#define INTERPRETERW_NAME "pythonw.exe"
#define INTERPRETER_SCRIPT_DIR "libexec\\"
#define INTERPRETER_SCRIPT_EXT_DEFAULT ".pyc"
#define INTERPRETER_SCRIPT_EXT_VAR "SCWRAPPER_INTERPRETER_EXT"
#define INTERPRETER_OPTIONS_ENV_PREFIX "SCWRAPPER_INTERPRETER_OPTIONS_"
#define WRAPPED_PROGRAMM_VAR "SCWRAPPER_EXECUTABLE"
#define WRAPPED_HANDLE_VAR "SCWRAPPER_HANDLE"

static int gui_available = 0;

void ErrorExit(LPTSTR lpszFunction) 
{ 
    /* Retrieve the system error message for the last-error code */

    LPVOID lpMsgBuf;
    LPTSTR lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    /* Display the error message and exit the process */

    lpDisplayBuf = (LPTSTR)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+80)*sizeof(TCHAR)); 
    _sntprintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf),
        TEXT("%s failed with error %d: %s%s"), 
		lpszFunction, dw, lpMsgBuf, gui_available ? "": "\n"); 

	if (gui_available) {
		MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
	} else {
		_fputts(lpDisplayBuf, stderr);
	}

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw); 
}

static LPTSTR getExe() {
	size_t execNameSize = 100;
	DWORD len;
	LPTSTR exeName;
	LPTSTR env;

	/*** get the name and path of this executable ***/
	while(1) {
		exeName = (LPTSTR)malloc(sizeof(TCHAR) * execNameSize);
		if (!exeName) {
			ErrorExit(TEXT("malloc (exeName)"));
		}
		len = GetModuleFileName(NULL, exeName, execNameSize);
		if (0 == len) {
			ErrorExit(TEXT("GetModuleFileName"));
		}
		if (len < execNameSize)
			break;
		free(exeName);
		execNameSize +=100;
	}

	env = _tgetenv(TEXT(WRAPPED_PROGRAMM_VAR));
	if (!(env && 0 == _tcscmp(env, exeName))) {
		env = malloc((_tcslen(TEXT(WRAPPED_PROGRAMM_VAR)) + 1 + _tcslen(exeName) + 1 ) * sizeof(TCHAR));
		if (!env) {
			ErrorExit(TEXT("malloc (env)"));
		}
		_tcscpy(env, TEXT(WRAPPED_PROGRAMM_VAR));
		_tcscat(env, TEXT("="));
		_tcscat(env, exeName);
		_tputenv(env);
		free(env);
	}

	return exeName;
}

static int run(LPCTSTR appl, LPTSTR cmdline) {
	LPTSTR env;
	HANDLE hProcess;
	DWORD exitCode;

	STARTUPINFO si;
    PROCESS_INFORMATION pi;

	/* Add a handle to the current process to env */
	hProcess = GetCurrentProcess(); 
	if (!DuplicateHandle(hProcess, 
                    hProcess, 
                    hProcess,
                    &hProcess, 
                    SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                    TRUE,
					0)) {
		ErrorExit(TEXT("DuplicateHandle"));
	}
	env = malloc((_tcslen(TEXT(WRAPPED_HANDLE_VAR)) + 1 + 33 + 1 ) * sizeof(TCHAR));
	if (!env) {
		ErrorExit(TEXT("malloc (env handle)"));
	}
	_tcscpy(env, TEXT(WRAPPED_HANDLE_VAR));
	_tcscat(env, TEXT("="));
	_ultot((unsigned long)hProcess, env + _tcslen(TEXT(WRAPPED_HANDLE_VAR)) + 1, 16);
	_tputenv(env);
	free(env);

	/* Start the child process */
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

	GetStartupInfo(&si);
	if (!CreateProcess(appl, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		ErrorExit(TEXT("CreateProcess"));
	}
	CloseHandle( hProcess );
    CloseHandle( pi.hThread );

	/* Ignore CTRL+C */
	SetConsoleCtrlHandler(NULL, TRUE);

    /* Wait until child process exits. */
	if(WAIT_FAILED == WaitForSingleObject( pi.hProcess, INFINITE )) {
		ErrorExit(TEXT("WaitForSingleObject"));
	}

	if(!GetExitCodeProcess( pi.hProcess, &exitCode )) {
		ErrorExit(TEXT("GetExitCodeProcess"));
	}

    /* Close process and thread handles.  */
    CloseHandle( pi.hProcess );

	return exitCode;
}

#ifndef INTERPRETER_WRAPPER

static int runCommand(void)
{
	LPTSTR name2;
	LPTSTR p;
    LPCTSTR baseName;
	LPCTSTR exeName;

    /* _putts(TEXT("Bitte Taste")); _gettc(stdin); */

	/*** get the name and path of this executable ***/
	exeName = getExe();
	
	/*** build the new executable name ***/

	name2 = (LPTSTR) malloc((_tcslen(exeName) + _tcslen(TEXT(LIBEXECDIR)) + 1) * sizeof(TCHAR));
	if (!name2) {
		ErrorExit(TEXT("malloc (2nd)"));
	}
	p = _tcsrchr(exeName, TEXT('\\'));
	if (!p) {
		ErrorExit(TEXT("_tcsrchr"));
	}
    baseName=p+1;
    *p = TEXT('\0');
	p = _tcsrchr(exeName, TEXT('\\'));
	if (!p) {
		ErrorExit(TEXT("_tcsrchr 2"));
	}
    *(p+1) = TEXT('\0');

	/* copy up and including to the last '\\' */
    _tcscpy(name2, exeName);
	/* append libexec */
	_tcscat(name2, TEXT(LIBEXECDIR));
	/* append the rest of the filename */
	_tcscat(name2, baseName);

	/* _fputts(name2, stdout); */

	/* create the process */
	return run(name2, GetCommandLine());
}

#else

const LPCTSTR getInterpreter(void) {
	return gui_available ? TEXT(INTERPRETERW_NAME) : TEXT(INTERPRETER_NAME);
}


static size_t quotedlen(LPCTSTR arg) {
	const TCHAR *p;
	size_t  bCount;
	size_t len = _tcslen(arg);

	if (len == _tcscspn(arg, TEXT(" \t\"")))
		return len;

	len += 2; /* start and closing quote */
	for(p=arg, bCount=0; *p; p++) {
		if (TEXT('"') == *p) {
			if (bCount) {
				len += bCount; /* double the number of backslashes */
			}
			len += 1;  /* escape the quotation mark */
		}

		if (TEXT('\\') == *p) {
			bCount++;
		} else {
			bCount = 0;
		}
	}
	if (bCount) {
		len += bCount; /* double the number of trailing backslashes */
	}
	return len;
}

static void quotedcat(LPTSTR dest, LPCTSTR arg) {
	const TCHAR *p;
	size_t  bCount;

	if (_tcslen(arg) == _tcscspn(arg, TEXT(" \t\""))) {
		_tcscat(dest, arg);
		return;
	}
	dest += _tcslen(dest);
	*dest++ = TEXT('"');

	for(p=arg, bCount=0; *p; p++) {
		if (TEXT('"') == *p) {
			if (bCount) {
				/* double the number of backslashes */
				size_t i;
				for(i=0; i<bCount; i++) {
					*dest++ = TEXT('\\');
				}
			}
			/* escape the quotation mark */
			*dest++ = TEXT('\\');
		}

		*dest++ = *p;

		if (TEXT('\\') == *p) {
			bCount++;
		} else {
			bCount = 0;
		}
	}
	if (bCount) {
		/* double the number of trailing backslashes */
		size_t i;
		for(i=0; i<bCount; i++) {
			*dest++ = TEXT('\\');
		}
	}
	*dest++ = TEXT('"');
	*dest++ = TEXT('\0');
}

static LPTSTR ArgvToCommandLine(LPCTSTR arg0, int argc,const TCHAR* argv[]) {
	size_t len;
	int i;
	LPTSTR cmdLine;
	int arg0NeedsQuotes=0;

	len=0;  
	if (arg0) {
		size_t l = _tcslen(arg0);
		len += l;
		arg0NeedsQuotes = l != _tcscspn(arg0, TEXT(" \t\""));
		if (arg0NeedsQuotes)
			len += 2;
	}
	if (argv) {
		if(len)
			len++;
		for(i=0; i<argc; i++) {
			len += quotedlen(argv[i]);
			if (i+1 < argc)
				len += 1;
		}
	}
	len++; /* trailing 0 */

	cmdLine = LocalAlloc(LMEM_FIXED, len * sizeof(TCHAR));
	if (!cmdLine)
		return NULL;
	*cmdLine = TEXT('\0');


	if (arg0) {
		if (arg0NeedsQuotes)
			_tcscat(cmdLine, TEXT("\""));
		_tcscat(cmdLine, arg0);
		if (arg0NeedsQuotes)
			_tcscat(cmdLine, TEXT("\""));
	}

	if (argv) {
		if(_tcslen(cmdLine))
			_tcscat(cmdLine, TEXT(" "));

		for(i=0; i<argc; i++) {
			quotedcat(cmdLine, argv[i]);
			if (i+1 < argc)
				_tcscat(cmdLine, TEXT(" "));
		}
	}

	return cmdLine;
}

static int runCommand(void)
{
	LPTSTR name2;
	LPTSTR p;
	LPTSTR exeName;
	LPTSTR baseName;  /* basename of exe with extention */
	LPTSTR baseNameWoExt;  /* basename of exe without extention */
	LPTSTR interpreterOptionsName; /* name of the environment variable containing options for the interpreter cmd line */
	LPCTSTR interpreterOptions; /* name of the environment variable containing options for the interpreter cmd line */
	LPCTSTR interpreterScriptExt;
	LPTSTR script;
	LPWSTR *argv;   /* splited command line */
	int argc;
	LPTSTR cmdLine; /* new cmd line */
	LPCTSTR arg0;
	LPCTSTR args;

	/*** get the name and path of this executable ***/
	exeName = getExe();
	
	/*** build the new executable name ***/

	name2 = (LPTSTR) malloc((_tcslen(exeName) + _tcslen(TEXT(LIBEXECDIR)) + _tcslen(getInterpreter()) + 1) * sizeof(TCHAR));
	if (!name2) {
		ErrorExit(TEXT("malloc (2nd)"));
	}
	p = _tcsrchr(exeName, TEXT('\\'));
	if (!p) {
		ErrorExit(TEXT("_tcsrchr"));
	}
	baseName = p+1;
	*p = TEXT('\0');
	p = _tcsrchr(exeName, TEXT('\\'));
	if (!p) {
		ErrorExit(TEXT("_tcsrchr 2"));
	}
    *(p+1) = TEXT('\0');

    /* copy up and including to the last '\\' */
    _tcscpy(name2, exeName);
	/* append libexec */
	_tcscat(name2, TEXT(LIBEXECDIR));
	/* append the rest of the filename */
	_tcscat(name2, getInterpreter());

	/* build the dirname and basename.
	   exeName becomes the dirname */
	baseNameWoExt = _tcsdup(baseName);
	if (!baseNameWoExt) {
		ErrorExit(TEXT("_tcsdup"));
	}
	p = _tcsrchr(baseNameWoExt, TEXT('.'));
	if (p)
		*p = TEXT('\0');

	/* build the options string */
	interpreterOptionsName = (LPTSTR) malloc((_tcslen(TEXT(INTERPRETER_OPTIONS_ENV_PREFIX)) + _tcslen(baseNameWoExt) + 1) * sizeof(TCHAR));
	_tcscpy(interpreterOptionsName, TEXT(INTERPRETER_OPTIONS_ENV_PREFIX));
	_tcscat(interpreterOptionsName, baseNameWoExt);
	interpreterOptions = _tgetenv(interpreterOptionsName);
	if (!interpreterOptions)
		interpreterOptions = TEXT("");

	/* build the interpreter script extention */
	interpreterScriptExt = _tgetenv(TEXT(INTERPRETER_SCRIPT_EXT_VAR));
	if (!interpreterScriptExt) {
		interpreterScriptExt = TEXT(INTERPRETER_SCRIPT_EXT_DEFAULT);
	}

	/* build the script */
	script = (LPTSTR) malloc((_tcslen(exeName) + _tcslen(TEXT(INTERPRETER_SCRIPT_DIR)) + 
		_tcslen(baseNameWoExt) + _tcslen(interpreterScriptExt) + 1) * sizeof(TCHAR));
	if (!script) {
		ErrorExit(TEXT("malloc (script)"));
	}
	_tcscpy(script, exeName);
	_tcscat(script, TEXT(INTERPRETER_SCRIPT_DIR));
	_tcscat(script, baseNameWoExt);
	_tcscat(script, interpreterScriptExt);

	/* build the command line: cmd */
	arg0 = ArgvToCommandLine(name2, 0, NULL);

	/* build the command line: args */
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) {
		ErrorExit(TEXT("CommandLineToArgvW"));
	}
	argv[0] = script;
	args = ArgvToCommandLine(NULL, argc, argv);
	LocalFree(argv); argv=NULL;

	/* finally build it all */
	cmdLine = (LPTSTR) malloc(( _tcslen(arg0) + 1
		+ _tcslen(interpreterOptions) + 1 
		+ _tcslen(args) + 1) 
		* sizeof(TCHAR));
	if (!cmdLine) {
		ErrorExit(TEXT("malloc (cmdLine)"));
	}
	/* argv[0] */
	_tcscpy(cmdLine, arg0);
	_tcscat(cmdLine, TEXT(" "));

	/* options */
	if (_tcslen(interpreterOptions)) {
		_tcscat(cmdLine, interpreterOptions);
		_tcscat(cmdLine, TEXT(" "));
	}

	/* script */
	_tcscat(cmdLine, args);

	/* create the process */
	return run(name2, cmdLine);
}



#endif

int _tmain(int argc, _TCHAR* argv[]) {
    UINT exitCode;
    exitCode = runCommand();
    TerminateProcess(GetCurrentProcess(), exitCode);
    return exitCode;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    UINT exitCode;
	gui_available = 1;
    exitCode = runCommand();
    TerminateProcess(GetCurrentProcess(), exitCode);
    return exitCode;
}

