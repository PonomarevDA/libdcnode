# Copyright (c) 2023 Dmitry Ponomarev
# Distributed under the MIT License, available in the file LICENSE.
# Author: Dmitry Ponomarev <ponomarevda96@gmail.com>

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR:=$(ROOT_DIR)/build
BUILD_EXAMPLES_DIR:=$(BUILD_DIR)/src/examples

define build_sitl
	$(info Build example $(1)...)
	mkdir -p $(BUILD_EXAMPLES_DIR)/$(1)
	cd $(BUILD_EXAMPLES_DIR)/$(1) && cmake $(ROOT_DIR)/examples/$(1) && make -s
endef

define run_sitl
	$(info Run example $(1)...)
	$(BUILD_EXAMPLES_DIR)/$(1)/ubuntu
endef

.PHONY: ubuntu ubuntu-build ubuntu-run

ubuntu-build:
	$(call build_sitl,ubuntu)

ubuntu-run:
	@if [ ! -x "$(BUILD_EXAMPLES_DIR)/ubuntu/ubuntu" ]; then \
		$(MAKE) ubuntu-build; \
	fi
	$(call run_sitl,ubuntu)

ubuntu: ubuntu-build
	$(call run_sitl,ubuntu)

clean:
	rm -rf build/examples/

code_style: cpplint cppcheck crlf
astyle:
	./scripts/code_style/check_astyle.py src include --astylerc scripts/code_style/astylerc
cpplint:
	cpplint src/*.c include/application/*.h include/serialization/*.h include/serialization/*/*/*.h include/serialization/*/*/*/*.h
cppcheck:
	./scripts/code_style/cppcheck.sh
crlf:
	./scripts/code_style/check_crlf.sh
distclean:
	rm -rf build/
