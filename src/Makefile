# cpp: IdleTracker.cpp inputdlg.cpp procrastitracker.cpp stdafx.cpp
# h: IdleTracker.h away.h day.h ddeutil.h inputdlg.h node.h
# h: nodedb.h prefview.h regview.h slaballoc.h statview.h stdafx.h
# h: timercallback.h tools.h win32tools.h

# need _WIN32_WINNT >= 0x0501, or ICC_STANDARD_CLASSES is undefined
COPT=-DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -D_WIN32_IE=0x500 -DMINGW32_BUG

pt.exe: procrastitracker.o IdleTracker.o inputdlg.o stdafx.o	\
	 ../procrastitracker/pt-resource.o
	g++ procrastitracker.o IdleTracker.o inputdlg.o stdafx.o	\
	 ../procrastitracker/pt-resource.o  -o pt.exe			\
	 -lz -lpsapi -lcomctl32 -lcomdlg32 -lshlwapi -loleacc		\
	 -lole32 -lgdi32 -lstdc++ -Wl,--subsystem,windows
# -lwin32k no; + psapi comctl32 comdlg32 shlwapi
# + oleacc ole32 gdi32 stdc++
IdleTracker.o: Makefile IdleTracker.cpp
	g++ $(COPT) -Wno-write-strings -c IdleTracker.cpp

inputdlg.o: Makefile inputdlg.cpp inputdlg.h
	g++ $(COPT) -fpermissive -c inputdlg.cpp

procrastitracker.o: Makefile procrastitracker.cpp stdafx.h resource.h		\
	 IdleTracker.h win32tools.h inputdlg.h slaballoc.h tools.h day.h node.h	\
	 nodedb.h ddeutil.h timercallback.h statview.h prefview.h regview.h away.h
	g++ $(COPT) -Wno-multichar -Wno-write-strings -c procrastitracker.cpp

stdafx.o: Makefile stdafx.cpp stdafx.h
	g++ $(COPT) -c stdafx.cpp

resource.h: ../procrastitracker/resource.h
	cp -a ../procrastitracker/resource.h ./

../procrastitracker/pt-resource.o: ../procrastitracker/procrastitracker.rc	\
	../procrastitracker/resource.h
	cd ..\\procrastitracker; windres $(COPT) procrastitracker.rc -o pt-resource.o
