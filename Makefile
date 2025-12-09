# Makefile for TechnoMachine (Oneiroi)

PATCHNAME = TechnoMachine
OWL_DIR = ../OwlProgram
PATCHSOURCE = $(shell pwd)
TOOLROOT = /Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin/

# Flags to pass to the sub-make
MAKE_FLAGS = PATCHNAME=$(PATCHNAME) \
             PATCHSOURCE=$(PATCHSOURCE) \
             PLATFORM=OWL3 \
             CONFIG=Release \
             TOOLROOT=$(TOOLROOT)

all:
	$(MAKE) -C $(OWL_DIR) -f compile.mk $(MAKE_FLAGS) libs
	$(MAKE) -C $(OWL_DIR) $(MAKE_FLAGS) patch

clean:
	$(MAKE) -C $(OWL_DIR) $(MAKE_FLAGS) clean

load:
	$(MAKE) -C $(OWL_DIR) $(MAKE_FLAGS) load

store:
	$(MAKE) -C $(OWL_DIR) $(MAKE_FLAGS) store SLOT=1

native:
	$(MAKE) -C $(OWL_DIR) -f native.mk $(MAKE_FLAGS) TOOLROOT= native

run: native
	$(OWL_DIR)/Build/Test/patch
