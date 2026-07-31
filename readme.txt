                      ELENA Language Project V. 1.5.0
                        (C)2005-2009  By Alex Rakov

The project includes ELENA documentation, standard library source code, 
samples, command-line compiler, IDE.

Currently only Win32-i386 (2000/XP/Vista/7) platform is supported.

For more information see at http://elenalang.sourceforge.net/

****************************************************************************
* Features
****************************************************************************

- Pure polymorphic object oriented language
- Changeable object behavior routine ("shift" technology)
- Dynamic "class mutation" ("annex / cast" handler)
- ELENA Virtual machine (in developing)
- Command line 32-bit compiler
- GUI IDE & Debugger
- Unicode support
- Complete source code
- Number of samples, including a card game Up'N'Down (in development)
- Getting started tutorial
- Simple Assembler compiler

****************************************************************************
* Minimum requirements
****************************************************************************

x86 processor, 
Win32:
 - Win2000/XP/Vista
 - 16 MB RAM 

****************************************************************************
* License
****************************************************************************

The compiler and executables distributed in this package fall under The Apache 
License V2.0e, for more information read the file LICENSE.TXT.

****************************************************************************
* Documentation
****************************************************************************

Visit the project web size for the latest info on the project: 
http://elenalang.sourceforge.net/

The documentation is available as HTML pages, and text. 
These are all available on http://sourceforge.net/projects/elenalang
(see Docs page) and in <app root>\doc

There you also can find Getting Started tutorial

****************************************************************************
* Suggestions, Help, Bug reporting, snapshots,  ...
****************************************************************************

Suggestions, Help ...
---------------------

elenalanguage@yandex.ru  
    - any questions about ELENA Language, IDE, compiler and so on

http://sourceforge.net/forum/forum.php?forum_id=585652 
    - general ELENA related questions

http://sourceforge.net/forum/forum.php?forum_id=581071 
    - help on IDE and compiler usage

****************************************************************************
* ELENA Language modules and programs
****************************************************************************
                                                
The complete source code of ELENA Standard library could be found
in the folder <app root>\src.

The complete source code of ELENA samples could be found
in the folder <app root>\examples.

****************************************************************************
* ELENA Language API & Documentation
****************************************************************************

ELENA API Documentation could be found in the folder <app root>\doc\api.
Number of other documents (todo list, known bugs, road map) are located in
<app root>\doc. For any suggestion, comments or correction please contact
the author - elenalanguage@yandex.ru.

****************************************************************************
* ELENA Project Source code
****************************************************************************

The project source code is compiled with CodeBlocks and Mingw32

The project files could be found in <app root>\elenasrc folder.

There could be some problems with paths to linked dlls: - libshlwapi and 
libcomctl32. You may open elide.dev project file and replace [project]\linker 
option to the correct one.

Visual studio projects are available as well.

****************************************************************************
* ELENA Installation / Run
****************************************************************************

To install just unzip all the files into a directory you want.

To open, compile or debug the programs and libraries use ELENA GUI IDE 
(<app root>\bin\elide.exe) or ELENA Command Line Compiler 
(<app root>\bin\elc.exe)

In ELENA IDE you may select File-Open-Open Project option and open an 
appropriate project file (*.prj). Then select Project-Compile option to 
compile the project and Project-Debug to debug it.
