#
# -*- coding: utf-8 -*-
#
# author: Anselm Kruis, <a.kruis@science-computing.de>
#
# This file has no copyright assigned and is placed in the Public Domain.
# No warranty is given.
#

import ctypes.wintypes
import warnings
from os.path import ( abspath, dirname, normpath, normcase, join, pardir, pathsep, basename, splitext, isdir )
from os import environ, _exit, putenv
import sys
import site


#
#  DLL loading
#
dir = unicode(dirname(abspath(sys.executable)))
if 0 == ctypes.windll.kernel32.SetDllDirectoryW(dir):
     warnings.warn("Failed to set DLL directory, code %s" % (ctypes.windll.kernel32.GetLastError(), ), RuntimeWarning)

#
#  Watchdog for our parent process
#
if "SCWRAPPER_HANDLE" in environ:
    HANDLE = ctypes.wintypes.HANDLE
    DWORD = ctypes.wintypes.DWORD
    BOOL = ctypes.wintypes.BOOL
    
    hParentInheritable = HANDLE(int(environ["SCWRAPPER_HANDLE"], 16))
    del environ["SCWRAPPER_HANDLE"]
    
    # it is necessary to duplicate the handle, to get 
    # a not inheritable version
    DUPLICATE_SAME_ACCESS = 0x00000002
    GetCurrentProcess = ctypes.windll.kernel32.GetCurrentProcess
    GetCurrentProcess.restype = HANDLE
    DuplicateHandle = ctypes.windll.kernel32.DuplicateHandle
    DuplicateHandle.argtypes = [HANDLE,
                                HANDLE,
                                HANDLE,
                                ctypes.POINTER(HANDLE),
                                DWORD,
                                BOOL,
                                DWORD]
    DuplicateHandle.restype = BOOL
    
    CloseHandle = ctypes.windll.kernel32.CloseHandle
    CloseHandle.argtypes = [HANDLE]
    CloseHandle.restype = BOOL
    
    WaitForSingleObject = ctypes.windll.kernel32.WaitForSingleObject
    WaitForSingleObject.argtypes = [HANDLE,
                                   DWORD]
    WaitForSingleObject.restype = DWORD

    hInstance = GetCurrentProcess()
    hParent = HANDLE(-1)
    if not DuplicateHandle(hInstance, # source process handle
                           hParentInheritable, # handle to be duplicated
                           hInstance, # target process handle
                           ctypes.byref(hParent), # result
                           0, # access - ignored due to options value
                           BOOL(False), # inherited by child procs?
                           DUPLICATE_SAME_ACCESS): # options
        warnings.warn("Failed to duplicate parent process handle, code %s" % (ctypes.windll.kernel32.GetLastError(), ), RuntimeWarning)
    else:
        if not CloseHandle(hParentInheritable):
            warnings.warn("Failed to close parent process handle, code %s" % (ctypes.windll.kernel32.GetLastError(), ), RuntimeWarning)
        import threading
        def watchdog(h=hParent):
            INFINITE = -1
            WAIT_OBJECT_0 = 0
            if WAIT_OBJECT_0 == WaitForSingleObject(h, INFINITE):
                _exit(3) # same as abort.
        watchdogThread = threading.Thread(target=watchdog, args=(hParent,))
        watchdogThread.name = "ParentProcessWatchdog"
        watchdogThread.daemon = True
        watchdogThread.start()
        hParent = None

# Environment
# ===========
# 
# Set up the environment for various extension modules.
# Most variables are used to make this python installation
# relocatable.
#
# Note: we use os.putenv, because we do not want our variable settings to show up
#       in os.environ. 
#       
        
#
# XDG Base Directory Specification
# See: http://freedesktop.org/wiki/Standards/basedir-spec?action=show
# 

# $XDG_DATA_DIRS defines the preference-ordered set of base directories to 
# search for data files in addition to the $XDG_DATA_HOME base directory.
putenv("XDG_DATA_DIRS", pathsep.join((normpath(join(dir, pardir, "share")),
                                      normpath(join(dir, pardir, pardir, pardir, "share")))))

# $XDG_CONFIG_DIRS defines the preference-ordered set of base directories to 
# search for configuration files in addition to the $XDG_CONFIG_HOME base 
# directory. 
putenv("XDG_CONFIG_DIRS", pathsep.join((normpath(join(dir, pardir, "etc")),
                                      normpath(join(dir, pardir, pardir, pardir, "etc")))))

# GLIB environment variables. See http://library.gnome.org/devel/glib/stable/glib-running.html
# LIBCHARSET_ALIAS_DIR.   Allows to specify a nonstandard location for the 
# charset.aliases file that is used by the character set conversion routines. 
# The default location is the libdir specified at compilation time.
putenv("LIBCHARSET_ALIAS_DIR", normpath(join(dir, pardir, "lib")))

# TZDIR. Allows to specify a nonstandard location for the timezone data files 
# that are used by the #GDateTime API. The default location is under 
# /usr/share/zoneinfo. For more information, also look at the tzset manual page.

#
# GIO environment variables. See http://library.gnome.org/devel/gio/stable/ch03.html
#

# currently nothing

#
# GTK+ variables. See http://library.gnome.org/devel/gtk/stable/gtk-running.html
#

# GTK_IM_MODULE_FILE. Specifies the file listing the IM modules to load. This 
# environment variable overrides the im_module_file specified in the RC files, 
# which in turn overrides the default value sysconfdir/gtk-2.0/gtk.immodules 
# (sysconfdir is the sysconfdir specified when GTK+ was configured, usually 
# /usr/local/etc.) 
putenv("GTK_IM_MODULE_FILE", normpath(join(dir, pardir, "etc", "gtk-2.0", "gtk.immodules")))

# GTK2_RC_FILES. Specifies a list of RC files to parse instead of the default 
# ones. An application can cause GTK+ to parse a specific RC file by calling 
# gtk_rc_parse(). In addition to this, certain files will be read at the end 
# of gtk_init(). Unless modified, the files looked for will be 
# <SYSCONFDIR>/gtk-2.0/gtkrc and .gtkrc-2.0 in the users home directory. 
# (<SYSCONFDIR> defaults to /usr/local/etc.)
putenv("GTK2_RC_FILES", normpath(join(dir, pardir, "etc", "gtk-2.0", "gtkrc")))

# GTK_EXE_PREFIX. If set, GTK+ uses $GTK_EXE_PREFIX/lib instead of the libdir 
# configured when GTK+ was compiled. 
putenv("GTK_EXE_PREFIX", normpath(join(dir, pardir)))

# GTK_DATA_PREFIX. If set, makes GTK+ use $GTK_DATA_PREFIX  instead of the 
# prefix configured when GTK+ was compiled. 
putenv("GTK_DATA_PREFIX", normpath(join(dir, pardir)))

# GDK_PIXBUF_MODULE_FILE. Specifies the file listing the GdkPixbuf loader 
# modules to load. This environment variable overrides the default value 
# sysconfdir/gtk-2.0/gdk-pixbuf.loaders  (sysconfdir is the sysconfdir 
# specified when GTK+ was configured, usually /usr/local/etc.) 
#
# Normally, the output of gdk-pixbuf-queryloaders  is written to 
# libdirgdk-pixbuf-2.0/2.10.0/loaders.cache, where gdk-pixbuf looks for it by 
# default. If it is written to some other location, the environment variable 
# GDK_PIXBUF_MODULE_FILE can be set to point gdk-pixbuf at the file. 
putenv("GDK_PIXBUF_MODULE_FILE", normpath(join(dir, pardir, "etc", "gtk-2.0", "gdk-pixbuf.loaders")))


# FONTCONFIG_FILE See: http://www.xfree86.org/current/fonts2.html
# putenv("FONTCONFIG_FILE", normpath(join(dir, pardir, "etc", "fonts", "fonts.conf")))

