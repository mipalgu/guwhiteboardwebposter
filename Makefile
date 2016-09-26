#
# GU whiteboard web server poster module Makefile
#
BIN=guwhiteboardwebposter

ALL_TARGETS=host robot

NEW_WHITEBOARD_SRCS+=guwhiteboardgetter.cpp guwhiteboardposter.cpp

CPP_SRCS=main.cpp

.include "../../mk/c++11.mk"        # needed for libc++ and C++11
.include "../../mk/dispatch.mk"      # blocks runtime and libdispatch
.include "../../mk/util.mk"
.include "../../mk/whiteboard.mk"
.include "../../mk/mipal.mk"		# comes last!

