# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024-2025 Waves Vision Ltd. <charlesb@wavesvision.com>
#
# Adapted from  https://github.com/Kurokesu/imx585-rpi-driver
#     Copyright (c) 2026, UAB Kurokesu. All rights reserved.

SRC_DIR   := $(shell pwd)

DRV_SRC   := ox05b1s.c
DRV_NAME  := ox05b1s
DTS       := ox05b1s-overlay.dts
DTBO      := ox05b1s.dtbo
DTC       := /usr/bin/dtc
DTC_FLAGS := -Wno-interrupts_property -Wno-unit_address_vs_reg -@ -I dts -O dtb

KDIR      ?= /lib/modules/$(shell uname -r)/build
CCFLAGS   := -Werror

.PHONY: all obj dtbo module clean

all: $(DTBO) $(DRV_NAME).ko

obj: $(DRV_NAME).o

dtbo: $(DTBO)

module: $(DRV_NAME).ko

$(DTBO):
	@cpp -nostdinc -I . -undef -x assembler-with-cpp $(DTS) >$(DTS).cpp
	@$(DTC) $(DTC_FLAGS) -o $@ $(DTS).cpp

Kbuild:
	@echo "ccflags-y += $(CCFLAGS)" > $@
	@echo "obj-m += $(DRV_NAME).o" >> $@

$(DRV_NAME).o: $(DRV_SRC) Kbuild
	@$(MAKE) -C $(KDIR) M=$(SRC_DIR) $(DRV_NAME).o

$(DRV_NAME).ko: $(DRV_SRC) Kbuild
	@$(MAKE) -C $(KDIR) M=$(SRC_DIR) modules

clean:
	@rm -rf ..module* .Module* .module* .ox05b1s* *.dtbo *.dts.cpp Kbuild Module.symvers modules.order *.ko *.mod *.mod.c *.mod.o *.o
