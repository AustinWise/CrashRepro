// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

/*++



Module Name:

    palinternal.h

Abstract:

    Rotor Platform Adaptation Layer (PAL) header file used by source
    file part of the PAL implementation. This is a wrapper over 
    unix/inc/pal.h. It allows avoiding name collisions when including 
    system header files, and it allows redirecting calls to 'standard' functions
    to their PAL counterpart

Details :

A] Rationale (see B] for the quick recipe)
There are 2 types of namespace collisions that must be handled.

1) standard functions declared in pal.h, which do not need to be 
   implemented in the PAL because the system's implementation is sufficient.

   (examples : memcpy, strlen, fclose)

   The problem with these is that a prototype for them is provided both in 
   pal.h and in a system header (stdio.h, etc). If a PAL file needs to 
   include the files containing both prototypes, the compiler may complain 
   about the multiple declarations.

   To avoid this, the inclusion of pal.h must be wrapped in a 
   #define/#undef pair, which will effectiveily "hide" the pal.h 
   declaration by renaming it to something else. this is done by palinternal.h
   in this way :

   #define some_function DUMMY_some_function
   #include <pal.h>
   #undef some_function

   when a PAL source file includes palinternal.h, it will see a prototype for 
   DUMMY_some_function instead of some_function; so when it includes the 
   system header with the "real" prototype, no collision occurs.

   (note : technically, no functions should ever be treated this way, all 
   system functions should be wrapped according to method 2, so that call 
   logging through ENTRY macros is done for all functions n the PAL. However 
   this reason alone is not currently considered enough to warrant a wrapper)

2) standard functions which must be reimplemented by the PAL, because the 
   system's implementation does not offer suitable functionnality.
   
   (examples : widestring functions, networking)
   
   Here, the problem is more complex. The PAL must provide functions with the 
   same name as system functions. Due to the nature of Unix dynamic linking, 
   if this is done, the PAL's implementation will effectively mask the "real" 
   function, so that all calls are directed to it. This makes it impossible for
   a function to be implemented as calling its counterpart in the system, plus 
   some extra work, because instead of calling the system's implementation, the
   function would only call itself in an infinitely recursing nightmare. Even 
   worse, if by bad luck the system libraries attempt to call the function for 
   which the PAL provides an implementation, it is the PAL's version that will 
   be called.
   It is therefore necessary to give the PAL's implementation of such functions
   a different name. However, PAL consumers (applications built on top of the 
   PAL) must be able to call the function by its 'official' name, not the PAL's 
   internal name. 
   This can be done with some more macro magic, by #defining the official name 
   to the internal name *in pal.h*. :

   #define some_function PAL_some_function

   This way, while PAL consumer code can use the official name, it is the 
   internal name that wil be seen at compile time.
   However, one extra step is needed. While PAL consumers must use the PAL's 
   implementation of these functions, the PAL itself must still have access to
   the "real" functions. This is done by #undefining in palinternal.h the names
   #defined in pal.h :

   #include <pal.h>
   #undef some_function.

   At this point, code in the PAL implementation can access *both* its own 
   implementation of the function (with PAL_some_function) *and* the system's 
   implementation (with some_function)

    [side note : for the Win32 PAL, this can be accomplished without touching 
    pal.h. In Windows, symbols in in dynamic libraries are resolved at 
    compile time. if an application that uses some_function is only linked to 
    pal.dll, some_function will be resolved to the version in that DLL, 
    even if other DLLs in the system provide other implementations. In addition,
    the function in the DLL can actually have a different name (e.g. 
    PAL_some_function), to which the 'official' name is aliased when the DLL 
    is compiled. All this is not possible with Unix dynamic linking, where 
    symbols are resolved at run-time in a first-found-first-used order. A 
    module may end up using the symbols from a module it was never linked with,
    simply because that module was located somewhere in the dependency chain. ]

    It should be mentionned that even if a function name is not documented as 
    being implemented in the system, it can still cause problems if it exists. 
    This is especially a problem for functions in the "reserved" namespace 
    (names starting with an underscore : _exit, etc). (We shouldn't really be 
    implementing functions with such a name, but we don't really have a choice)
    If such a case is detected, it should be wrapped according to method 2

    Note that for all this to work, it is important for the PAL's implementation
    files to #include palinternal.h *before* any system files, and to never 
    include pal.h directly.

B] Procedure for name conflict resolution :

When adding a function to pal.h, which is implemented by the system and 
which does not need a different implementation :

- add a #define function_name DUMMY_function_name to palinternal.h, after all 
  the other DUMMY_ #defines (above the #include <pal.h> line)
- add the function's prototype to pal.h (if that isn't already done)
- add a #undef function_name to palinternal.h near all the other #undefs 
  (after the #include <pal.h> line)
  
When overriding a system function with the PAL's own implementation :

- add a #define function_name PAL_function_name to pal.h, somewhere 
  before the function's prototype, inside a #ifndef _MSCVER/#endif pair 
  (to avoid affecting the Win32 build)
- add a #undef function_name to palinternal.h near all the other #undefs 
  (after the #include <pal.h> line)
- implement the function in the pal, naming it PAL_function_name
- within the PAL, call PAL_function_name() to call the PAL's implementation, 
function_name() to call the system's implementation



--*/

#ifndef _PAL_INTERNAL_H_
#define _PAL_INTERNAL_H_


#ifdef DEBUG
#define _ENABLE_DEBUG_MESSAGES_ 1
#else
#define _ENABLE_DEBUG_MESSAGES_ 0
#endif

#include "pal.h"

#define _DONT_USE_CTYPE_INLINE_
#if HAVE_RUNETYPE_H
#include <runetype.h>
#endif
#include <ctype.h>

// Don't use C++ wrappers for stdlib.h
// https://gcc.gnu.org/ml/libstdc++/2016-01/msg00025.html 
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1

#define _WITH_GETLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>


#endif /* _PAL_INTERNAL_H_ */
