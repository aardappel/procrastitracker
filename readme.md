ProcrastiTracker
================

This is the source code repository for ProcrastiTracker, a Windows time tracking
application available at
[http://strlen.com/procrastitracker/](<http://strlen.com/procrastitracker/>)

It is a small application that sits in your tray and checks which
application/url you’re on every 10 seconds, and then collects that into a custom
tree-database that allows you to view statistics on your computer use.

For more information on how it works, see
[http://strlen.com/procrastitracker/](<http://strlen.com/procrastitracker/>)

Building
--------

Use the included project files (in the `procastitracker` directory) for Visual
Studio 2015 (the free Community Edition works fine). This is not a portable
application, sorry, see below.

Distributing
------------

To create an installer, first make sure a Release mode build produced an
executable in the `PT` directory. Delete any other build cruft Visual Studio may
have generated there.

Create an installer script using `pt.nsi` (right click -\> Compile NSIS Script).
This needs [NSIS](<http://nsis.sourceforge.net/Main_Page>) installed.
Alternatively just zip up the `PT` directory.

Source code
-----------

While pretty much all code I’ve ever written is portable, this is an exception.
It makes use of Windows specifics such as hooks and DDE to gather its
information, and for some reason, to keep the app small and fast, I decided to
write its UI directly using Win32 controls (ugh!), so it is very much
non-portable. Would be a fair bit of work to change that, you’re welcome to try
:)

Code is generally old and crufty, didn’t feel like cleaning it up for its open
source release, sorry. That said, this code has been running on quite a few
peoples computers for many years, it appears to be at least very stable.

The code uses some features that are available only from Windows Vista onwards.

Licence
-------

The code and all their associated files are released under the [Apache v2
license](<http://www.apache.org/licenses/LICENSE-2.0>).

