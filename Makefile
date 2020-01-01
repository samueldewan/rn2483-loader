# Target file name (without extension).
TARGET = rn2483_loader

SRCDIR = src
# List C source files here. (C dependencies are automatically generated.)
SRC = $(wildcard $(SRCDIR)/*.c) 

OBJDIR = obj

# Optimization level, can be [0, 1, 2, 3, s]. 
#     0 = turn off optimization. s = optimize for size.
#     (Note: 3 is not always the best optimization level.)
OPT = 2

# List any extra directories to look for include files here.
EXTRAINCDIRS = $(SRCDIR)

# Compiler flag to set the C Standard level.
CSTANDARD = -std=gnu11

# Place -D or -U options here
CDEFS = 

# Place -I options here
CINCS = 

#---------------- Compiler Options ----------------
#  -g*: 			generate debugging information
#  -O*:          optimization level
#  -f...:        	tuning, see GCC manual
#  -Wall...:    warning level
#  -Wa,...:     tell GCC to pass this to the assembler.
#    -adhlns...: create assembler listing
CFLAGS += $(CDEFS) $(CINCS)
CFLAGS += -O$(OPT) -g3
CFLAGS += -funsigned-char -fno-strict-aliasing
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -gstrict-dwarf

# Enable many usefull warnings
# (see https://gcc.gnu.org/onlinedocs/gcc-6.3.0/gcc/Warning-Options.html)
CFLAGS += -Wall -Wextra -Wshadow -Wundef -Wformat=2 -Wfloat-equal -Wpedantic
CFLAGS += -Wbad-function-cast -Waggregate-return -Wstrict-prototypes -Wpacked
CFLAGS += -Wmissing-prototypes -Winit-self -Wmissing-declarations -Winline
CFLAGS += -Wmissing-format-attribute -Wunreachable-code -Wcast-align -Wvla
CFLAGS += -Wpointer-arith -Wwrite-strings -Wnested-externs  -Wlong-long
CFLAGS += -Wredundant-decls -Wcast-qual -Wdouble-promotion -Wnull-dereference
CFLAGS += -Werror=implicit-function-declaration -Wlogical-not-parentheses
CFLAGS += -Wold-style-definition -Wdisabled-optimization -Wmissing-include-dirs

CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS))
CFLAGS += $(CSTANDARD)

#---------------- Assembler Options ----------------
#  -Wa,...:   tell GCC to pass this to the assembler.
#  -ahlms:    create listing
#  -gstabs:   have the assembler create line number information; note that
#             for use in COFF files, additional information about filenames
#             and function names needs to be present in the assembler source
#             files -- see avr-libc docs [FIXME: not yet described there]
#  -listing-cont-lines: Sets the maximum number of continuation lines of hex 
#       dump that will be displayed for a given single line of source input.
ASFLAGS = -Wa,-adhlns=$(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.lst,$<),-gstabs,--listing-cont-lines=100

#---------------- Linker Options ----------------
LDFLAGS += -lm -lreadline --param max-inline-insns-single=500

#============================================================================

# Define programs and commands.
SHELL = sh
CC = cc
LD = cc
AS = cc
REMOVE = rm -f
COPY = cp

# Define all object files.
OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))

# Define all dependancy files.
DEP = $(OBJ:%.o=%.d)

# Define all list files.
LST = $(OBJ:%.o=%.lst)

# Combine all necessary flags and optional flags.
# Add target processor to flags.
ALL_CFLAGS = -I. $(CFLAGS)

# Define compilers
COMPILE.c = $(CC) $(ALL_CFLAGS) -c

# Default target.
build:  $(OBJDIR) $(OBJDIR)/$(TARGET)

$(OBJDIR):
	@mkdir -p $@

# Link: create ELF output file from object files.
.SECONDARY : $(OBJDIR)/$(TARGET)
.PRECIOUS : $(OBJ)
$(OBJDIR)/$(TARGET): $(OBJ)
	$(shell mkdir -p $(@D) >/dev/null)
	@echo
	@echo $(MSG_LINKING) $@
	$(LD) $^ --output $@ $(LDFLAGS)

# Compile: create object files from C source files.
$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(shell mkdir -p $(@D) >/dev/null)
	@echo
	@echo $(MSG_COMPILING) $<
	$(COMPILE.c) "$(abspath $<)" -o $@

$(OBJDIR)/.depend:  $(SRC) $(OBJDIR)
	$(COMPILE.c) -MM $(SRC)  | \
	sed -E 's#^(.*\.o: *)$(SRCDIR)/(.*/)?(.*\.(c|cpp|S))#$(OBJDIR)/\2\1$(SRCDIR)/\2\3#' > $@

-include $(OBJDIR)/.depend

# Target: clean project.
clean:
	@echo $(MSG_CLEANING)
	$(REMOVE) -rf $(OBJDIR)/*

# Listing of phony targets.
.PHONY : all gccversion build elf clean clean_list program debug upload reset
