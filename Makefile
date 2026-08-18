# Copyright (c) 2023 Dmitry Ponomarev
# Distributed under the MPL v2.0 License, available in the file LICENSE.
# Author: Dmitry Ponomarev <ponomarevda96@gmail.com>

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR:=$(ROOT_DIR)/build
BUILD_EXAMPLES_DIR:=$(BUILD_DIR)/src/examples
BUILD_TESTS_DIR:=$(BUILD_DIR)/tests

define build_sitl
	$(info Build example $(1)...)
	mkdir -p $(BUILD_EXAMPLES_DIR)/$(1)
	cd $(BUILD_EXAMPLES_DIR)/$(1) && cmake $(ROOT_DIR)/examples/$(1) && make -s
endef

define run_sitl
	$(info Run example $(1)...)
	$(BUILD_EXAMPLES_DIR)/$(1)/ubuntu
endef

.PHONY: ubuntu ubuntu-build ubuntu-run tests

ubuntu-build:
	$(call build_sitl,ubuntu)

ubuntu-run:
	@if [ ! -x "$(BUILD_EXAMPLES_DIR)/ubuntu/ubuntu" ]; then \
		$(MAKE) ubuntu-build; \
	fi
	$(call run_sitl,ubuntu)

ubuntu: ubuntu-build
	$(call run_sitl,ubuntu)

tests:
	cmake -S $(ROOT_DIR) -B $(BUILD_TESTS_DIR) -DLIBDCNODE_BUILD_TESTS=ON
	cmake --build $(BUILD_TESTS_DIR) --target libdcnode_pub_sub_test
	ctest --test-dir $(BUILD_TESTS_DIR) --output-on-failure

clean:
	rm -rf build/examples/

code_style: cpplint cppcheck crlf
astyle:
	./scripts/code_style/check_astyle.py src include --astylerc scripts/code_style/astylerc
cpplint:
	cpplint src/*.cpp tests/*.cpp include/libdcnode/*.h include/libdcnode/*.hpp
cppcheck:
	./scripts/code_style/cppcheck.sh
crlf:
	./scripts/code_style/check_crlf.sh
distclean:
	rm -rf build/
