# ============================================================================
#  HDDHealth Monitor - Build configuration
#  ---------------------------------------------------------------------------
#  100% Free and Open Source Software (FOSS).
#
#  Author  : Ari Sohandri Putra
#  Company : ARImetic Inc.
#  Sponsor : https://github.com/sponsors/arisohandriputra/
#  License : MIT
#
#  Build targets:
#    - $(OUTDIR)/HDDHealth.exe   : The monitor application itself.
# ============================================================================

ifeq ($(OS),Windows_NT)
    CC      = g++
    WINDRES = windres
else
    CC      = x86_64-w64-mingw32-g++
    WINDRES = x86_64-w64-mingw32-windres
endif

SRCDIR  = src
OBJDIR  = obj
OUTDIR  = bin
TARGET  = $(OUTDIR)/HDDHealthMonitor.exe
SRCS    = $(SRCDIR)/main.cpp \
          $(SRCDIR)/mainwnd.cpp \
          $(SRCDIR)/smart.cpp \
          $(SRCDIR)/smart_history.cpp \
          $(SRCDIR)/donate.cpp

OBJS    = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRCS))
RES_O   = $(OBJDIR)/app_res.o

# Compiler flags:
#   -mwindows          : GUI subsystem (no console window)
#   -O2                : Optimized release build
#   -DWIN32 ...        : Win32 platform defines expected by the source
#   -Wall ... -fpermissive : Reasonable warnings, with pragmatic suppressions
#                          for the existing C-style Win32 code base.
CFLAGS  = -mwindows -O2 \
          -DWIN32 -D_WIN32 -D_WINDOWS -DNDEBUG \
          -I$(SRCDIR) \
          -Wall -Wno-unused-function -Wno-unused-parameter \
          -Wno-unused-variable -Wno-format -Wno-cast-function-type \
          -fpermissive

# Linker flags:
#   -static*           : Statically link the C/C++ runtime so the .exe
#                        has no external runtime DLL dependencies.
#   -lcomctl32 ...     : Win32 system libraries used by the UI and by
#                        the S.M.A.R.T. / SetupAPI code paths.
LDFLAGS = -mwindows \
          -static -static-libgcc -static-libstdc++ \
          -lcomctl32 -lmsimg32 -lshell32 \
          -luser32 -lgdi32 -lkernel32 -ladvapi32 -lole32 -luuid \
          -lgdiplus -lshlwapi -lsetupapi -lcfgmgr32


.PHONY: all clean

all: $(OUTDIR) $(OBJDIR) $(TARGET)
	@echo ""
	@echo "  Build complete: $(TARGET)"
	@echo "  100% Free Open Source Software - have fun!"
	@echo ""

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "  CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Resource compilation: the .rc file references the .manifest and .ico,
# so the .o depends on all three.
$(RES_O): $(SRCDIR)/app.rc $(SRCDIR)/app.manifest $(SRCDIR)/app.ico | $(OBJDIR)
	@echo "  RC  $(SRCDIR)/app.rc"
	@$(WINDRES) --include-dir=$(SRCDIR) $(SRCDIR)/app.rc -o $(RES_O)

$(TARGET): $(OBJS) $(RES_O) | $(OUTDIR)
	@echo "  LD  $@"
	@$(CC) $(OBJS) $(RES_O) $(LDFLAGS) -o $@

$(OBJDIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
else
	@mkdir -p $(OBJDIR)
endif

$(OUTDIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
else
	@mkdir -p $(OUTDIR)
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OBJDIR) rmdir /s /q $(OBJDIR)
	@if exist $(OUTDIR) rmdir /s /q $(OUTDIR)
else
	@rm -rf $(OBJDIR) $(OUTDIR)
endif
	@echo "  Clean."
