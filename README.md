My 4th attempt at a good exception handling library over the years.

* Written in C. No particular reason other than it works best with Windows code.
* Project aims to follow some Hungarian / NT kernel programming conventions.
* Project source files use phnt.h, header files use Windows.h for friendliness.

Most, atleast all projects I've seen, make use of Microsoft's facilities
for implementing debug info. None of them unwind themselves, none of them
seem to have problems with dbghelp's bugs. Not only that, Microsoft's code
does not handle cases well at all if the CONTEXT's stack is smashed or other
bad issues; they do not call the user's handler function pointer.

This project will have, when finished, custom unwinding which will be able
to parse both [PDB/CodeView](https://llvm.org/docs/PDB/index.html) and [DWARF](https://dwarfstd.org/),
while being defensively programmed as to not cause crashes.

And it will *always* try to get you feedback, so that you know your program
has crashed. For reliability reasons, EH4 makes use of a watchdog agent that 
immediately gets notified via duplicated handle that the client process has 
crashed and will begin the crash handling process. If we try to handle the
crash while within the process and not deferring externally as soon as
possible, the higher the chance the process dies. And you're left wondering
why a user says they crashed with no report to back it up.

Check the `tests` folder for examples on how to use this project.
