 #####
 # Project: OpenPilot
 #
 #
 # Makefile for OpenPilot project build PiOS and the AP.
 #
 # The OpenPilot Team, http://www.openpilot.org, Copyright (C) 2009.
 # Tau Labs, http://taulabs.org, Copyright (C) 2013-2014
 # dRonin, http://dronin.org Copyright (C) 2015-2016
 #
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation; either version 3 of the License, or
 # (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful, but
 # WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 # or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 # for more details.
 #
 # You should have received a copy of the GNU General Public License along
 # with this program; if not, see <http://www.gnu.org/licenses/>
 #
 # Additional note on redistribution: The copyright and license notices above
 # must be maintained in each individual source file that is a derivative work
 # of this source file; otherwise redistribution is prohibited.
 #####

include $(MAKE_INC_DIR)/firmware-defs.mk

# Set developer code and compile options
# Set to YES to compile for debugging
DEBUG ?= NO

# Include objects that are just nice information to show
WDG_STATS_DIAGNOSTICS ?= NO
DIAG_TASKS ?= YES

#Or just turn on all the above diagnostics. WARNING: This consumes massive amounts of memory.
ALL_DIAGNOSTICS ?=NO

# Set to YES to build a FW version that will erase all flash memory
ERASE_FLASH ?= NO
# Set to YES to use the Servo output pins for debugging via scope or logic analyser
ENABLE_DEBUG_PINS ?= NO

# List of modules to include
MODULES += ComUsbBridge
MODULES += Telemetry
MODULES += RadioComBridge
MODULES += UAVOTaranis
MODULES += FirmwareIAP

# Paths
OPUAVOBJINC = $(OPUAVOBJ)/inc
OPTESTS  = ./Tests
FLIGHTLIBINC = $(FLIGHTLIB)/inc
PIOSINC = $(PIOS)/inc
PIOSSTM32F10X = $(PIOS)/STM32F10x
PIOSCOMMON = $(PIOS)/Common
PIOSCOMMONLIB = $(PIOSCOMMON)/Libraries
MATHLIB = $(FLIGHTLIB)/math
MATHLIBINC = $(FLIGHTLIB)/math
APPLIBDIR = $(PIOSSTM32F10X)/Libraries
STMLIBDIR = $(APPLIBDIR)
STMSPDDIR = $(STMLIBDIR)/STM32F10x_StdPeriph_Driver
STMUSBDIR = $(STMLIBDIR)/STM32_USB-FS-Device_Driver
STMSPDSRCDIR = $(STMSPDDIR)/src
STMSPDINCDIR = $(STMSPDDIR)/inc
STMUSBSRCDIR = $(STMUSBDIR)/src
STMUSBINCDIR = $(STMUSBDIR)/inc
CMSISDIR  = $(STMLIBDIR)/CMSIS/Core/CM3
MSDDIR  = $(APPLIBDIR)/msd

# List C source files here. (C dependencies are automatically generated.)
# use file-extension c for "c-only"-files

## OPENPILOT CORE:
SRC += ${OPMODULEDIR}/System/systemmod.c
SRC += ${OPMODULEDIR}/System/rgbleds.c
SRC += chibi_main.c
SRC += pios_board.c
SRC += $(OPUAVOBJ)/uavobjectmanager.c

## PIOS Hardware (STM32F10x)
SRC += pios_sys.c
SRC += pios_iap.c
SRC += pios_usart.c
SRC += pios_flash_internal.c
SRC += pios_rtc.c
SRC += pios_tim.c
SRC += pios_bl_helper.c

# PIOS USB related files (seperated to make code maintenance more easy)
SRC += pios_usb.c
SRC += pios_usbhook.c
SRC += pios_usb_hid.c
SRC += pios_usb_cdc.c
SRC += pios_usb_hid_istr.c
SRC += pios_usb_hid_pwr.c
SRC += pios_usb_board_data.c

## PIOS Hardware (Common)
include $(PIOS)/pios_flight_library.mk

SRC += printf-stdarg.c

## Libraries for flight calculations
SRC += taskmonitor.c

## Radio
SRC += pios_openlrs.c
SRC += pios_openlrs_rcvr.c

## CMSIS for STM32
include $(PIOSCOMMONLIB)/CMSIS/library.mk

# List C source files here which must be compiled in ARM-Mode (no -mthumb).
# use file-extension c for "c-only"-files
## just for testing, timer.c could be compiled in thumb-mode too
SRCARM =

# List C++ source files here.
# use file-extension .cpp for C++-files (not .C)
CPPSRC =

# List C++ source files here which must be compiled in ARM-Mode.
# use file-extension .cpp for C++-files (not .C)
#CPPSRCARM = $(TARGET).cpp
CPPSRCARM =

# List Assembler source files here.
# Make them always end in a capital .S. Files ending in a lowercase .s
# will not be considered source files but generated files (assembler
# output from the compiler), and will be deleted upon "make clean"!
# Even though the DOS/Win* filesystem matches both .s and .S the same,
# it will preserve the spelling of the filenames, and gcc itself does
# care about how the name is spelled on its command-line.
ASRC =

# List Assembler source files here which must be assembled in ARM-Mode..
ASRCARM =

# List any extra directories to look for include files here.
#    Each directory must be seperated by a space.
EXTRAINCDIRS  += $(SHAREDAPIDIR)
EXTRAINCDIRS  += $(OPUAVOBJ)
EXTRAINCDIRS  += $(OPUAVOBJINC)
EXTRAINCDIRS  += $(OPUAVSYNTHDIR)
EXTRAINCDIRS  += $(PIOS)
EXTRAINCDIRS  += $(PIOSINC)
EXTRAINCDIRS  += $(MATHLIBINC)
EXTRAINCDIRS  += $(FLIGHTLIBINC)
EXTRAINCDIRS  += $(PIOSSTM32F10X)
EXTRAINCDIRS  += $(PIOSSTM32F10X)/inc
EXTRAINCDIRS  += $(PIOSCOMMON)
EXTRAINCDIRS  += $(STMSPDINCDIR)
EXTRAINCDIRS  += $(STMUSBINCDIR)
EXTRAINCDIRS  += $(CMSISDIR)
EXTRAINCDIRS  += $(MSDDIR)
EXTRAINCDIRS  += $(RTOSINCDIR)
EXTRAINCDIRS  += $(APPLIBDIR)
EXTRAINCDIRS  += $(RTOSPORTDIR)/portable/GCC/ARM_CM3

EXTRAINCDIRS += ${foreach MOD, ${OPTMODULES} ${MODULES}, ${OPMODULEDIR}/${MOD}/inc} ${OPMODULEDIR}/System/inc


# List any extra directories to look for library files here.
# Also add directories where the linker should search for
# includes from linker-script to the list
#     Each directory must be seperated by a space.
EXTRA_LIBDIRS =

# Extra Libraries
#    Each library-name must be seperated by a space.
#    i.e. to link with libxyz.a, libabc.a and libefsl.a:
#    EXTRA_LIBS = xyz abc efsl
# for newlib-lpc (file: libnewlibc-lpc.a):
#    EXTRA_LIBS = newlib-lpc
EXTRA_LIBS =

# Path to Linker-Scripts
LINKERSCRIPTPATH = $(PIOSSTM32F10X)

# Output format. (can be ihex or binary or both)
#  binary to create a load-image in raw-binary format i.e. for SAM-BA,
#  ihex to create a load-image in Intel hex format
#LOADFORMAT = ihex
#LOADFORMAT = binary
LOADFORMAT = both

# Debugging format.
DEBUGF = dwarf-2

# Place project-specific -D (define) and/or
# -U options for C here.
#
include $(PIOS)/STM32F10x/library_chibios.mk

CDEFS += -DHSE_VALUE=$(OSCILLATOR_FREQ)
CDEFS += -DSYSCLK_FREQ=$(SYSCLK_FREQ)
CDEFS += -DUSE_STDPERIPH_DRIVER
CDEFS += -DUSE_$(BOARD)
ifeq ($(ENABLE_DEBUG_PINS), YES)
CDEFS += -DPIOS_ENABLE_DEBUG_PINS
endif
ifeq ($(ERASE_FLASH), YES)
CDEFS += -DERASE_FLASH
endif

CDEFS += -DEE_BANK_BASE=$(EE_BANK_BASE)
CDEFS += -DEE_BANK_SIZE=$(EE_BANK_SIZE)

CDEFS += -DPIOS_COM_TELEM_USB_RX_BUF_LEN=256
CDEFS += -DPIOS_COM_TELEM_USB_TX_BUF_LEN=256

# Declare all non-optional modules as built-in to force inclusion
CDEFS += $(foreach MOD, $(notdir $(MODULES)), -DMODULE_$(MOD)_BUILTIN)

# Place project-specific -D and/or -U options for
# Assembler with preprocessor here.
#ADEFS = -DUSE_IRQ_ASM_WRAPPER
ADEFS = -D__ASSEMBLY__

# Compiler flag to set the C Standard level.
# c89   - "ANSI" C
# gnu89 - c89 plus GCC extensions
# c99   - ISO C99 standard (not yet fully implemented)
# gnu99 - c99 plus GCC extensions
CSTANDARD = -std=gnu99

#-----

# Compiler flags.

#  -g*:          generate debugging information
#  -O*:          optimization level
#  -f...:        tuning, see GCC manual and avr-libc documentation
#  -Wall...:     warning level
#  -Wa,...:      tell GCC to pass this to the assembler.
#    -adhlns...: create assembler listing
#
# Flags for C and C++ (arm-elf-gcc/arm-elf-g++)

ifeq ($(DEBUG),YES)
CFLAGS += -DDEBUG
endif

ifneq (,$(filter YES,$(WDG_STATS_DIAGNOSTICS) $(ALL_DIAGNOSTICS)))
CFLAGS += -DWDG_STATS_DIAGNOSTICS
endif

ifneq (,$(filter YES,$(DIAG_TASKS) $(ALL_DIAGNOSTICS)))
CFLAGS += -DDIAG_TASKS
endif

CFLAGS += -g$(DEBUGF)
CFLAGS += -Os -fconserve-stack
CFLAGS += -mcpu=$(MCU)
CFLAGS += $(CDEFS)
CFLAGS += -I. $(patsubst %,-I%,$(EXTRAINCDIRS))

#CFLAGS += -fno-cprop-registers -fno-defer-pop -fno-guess-branch-probability -fno-section-anchors
#CFLAGS += -fno-if-conversion -fno-if-conversion2 -fno-ipa-pure-const -fno-ipa-reference -fno-merge-constants
#CFLAGS += -fno-split-wide-types -fno-tree-ccp -fno-tree-ch -fno-tree-copy-prop -fno-tree-copyrename
#CFLAGS += -fno-tree-dce -fno-tree-dominator-opts -fno-tree-dse -fno-tree-fre -fno-tree-sink -fno-tree-sra
#CFLAGS += -fno-tree-ter
#CFLAGS += -g$(DEBUGF) -DDEBUG

CFLAGS += -mapcs-frame
CFLAGS += -fomit-frame-pointer
CFLAGS += -ffunction-sections -fdata-sections

CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wa,-adhlns=$(addprefix $(OUTDIR)/, $(notdir $(addsuffix .lst, $(basename $<))))
# Compiler flags to generate dependency files:
CFLAGS += -MD -MP -MF $(OUTDIR)/dep/$(@F).d

# flags only for C
#CONLYFLAGS += -Wnested-externs
CONLYFLAGS += $(CSTANDARD)

# Assembler flags.
#  -Wa,...:    tell GCC to pass this to the assembler.
#  -ahlns:     create listing
ASFLAGS  = -mcpu=$(MCU) -I. -x assembler-with-cpp
ASFLAGS += $(ADEFS)
ASFLAGS += -Wa,-adhlns=$(addprefix $(OUTDIR)/, $(notdir $(addsuffix .lst, $(basename $<))))
ASFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS))

MATH_LIB = -lm

LIBS += $(UAVOBJLIB)
LIBS += $(FLIGHTLIBFILE)

# Linker flags.
#  -Wl,...:     tell GCC to pass this to linker.
#    -Map:      create map file
#    --cref:    add cross reference to  map file
LDFLAGS = -nostartfiles -Wl,-Map=$(OUTDIR)/$(TARGET).map,--cref,--gc-sections
LDFLAGS += $(patsubst %,-L%,$(EXTRA_LIBDIRS))
LDFLAGS += -lc
LDFLAGS += $(patsubst %,-l%,$(EXTRA_LIBS))
LDFLAGS += $(MATH_LIB)
LDFLAGS += -lc_nano -lgcc
LDFLAGS += -Wl,--warn-common
LDFLAGS += -Wl,--fatal-warnings

# Set linker-script name depending on selected submodel name
LDFLAGS += -T$(LINKERSCRIPTPATH)/link_$(BOARD)_memory.ld
LDFLAGS += -T$(LINKERSCRIPTPATH)/sections_chibios.ld

NO_AUTO_UAVO=YES

include $(MAKE_INC_DIR)/firmware-common.mk
