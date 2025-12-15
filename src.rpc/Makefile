SHELL := /bin/bash -O globstar
SHYNUR_ISA := $(shell ([ $$HOSTTYPE = x86_64 ]&&echo x64)||([ $$HOSTTYPE = aarch64 ]&&echo arm64)||echo unknown)
SHYNUR_UBUNTU_VER := $(shell cat /etc/os-release | grep VERSION_ID | cut -d '"' -f2 | tr -d '.')
CMAKE_BUILD_DIR := build.cmake.$(SHYNUR_ISA)-ubuntu$(SHYNUR_UBUNTU_VER)

a.out: $(CMAKE_BUILD_DIR)/a.out FORCE
	$(MAKE) $<
	ln -f -s $< $@

$(CMAKE_BUILD_DIR)/a.out: FORCE
	$(MAKE) `dirname $@`/Makefile
	cd `dirname $@`; $(MAKE)

$(CMAKE_BUILD_DIR)/Makefile: CMakeLists.txt $(wildcard *..idl) | $(CMAKE_BUILD_DIR)/
	cd `dirname $@`;  \
	CXXFLAGS='-O0 -g3 '$$CXXFLAGS CMAKE_GENERATOR='Unix Makefiles' cmake -DCMAKE_BUILD_TYPE=Debug ..

%/:
	mkdir -p $@

.PHONY: FORCE

.PHONY: clean
clean:
	rm -f -- ?*~ \#?*\# .\#?*\#
	rm -rf $(CMAKE_BUILD_DIR)
	for idl in *.idl; do rm -f $${idl%.idl}*.{{i,h}pp,cxx}; done
	rm -f a.out

.PHONY: print-vars
print-vars:
	echo SHYNUR_ISA=$(SHYNUR_ISA)
	echo SHYNUR_UBUNTU_VER=$(SHYNUR_UBUNTU_VER)
	echo CMAKE_BUILD_DIR=$(CMAKE_BUILD_DIR)
