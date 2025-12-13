SHELL = /bin/bash -O globstar

urpc: build.cmake/urpc | build.cmake/
	if ! [ -f $@ ]; then ln -s build.cmake/urpc urpc; done

build.cmake/urpc: FORCE
	$(MAKE) build.cmake/Makefile
	cd build.cmake; $(MAKE)

build.cmake/Makefile: CMakeLists.txt ShynurUrpcProcessor.idl | build.cmake/
	cd build.cmake;  \
	CXXFLAGS='-O0 -g3 '$$CXXFLAGS CMAKE_GENERATOR='Unix Makefiles' cmake -DCMAKE_BUILD_TYPE=Debug ..

%/:
	mkdir -p $@

.PHONY: FORCE

.PHONY: clean
clean:
	rm -f -- ?*~ \#?*\# .\#?*\#
	rm -rf build.cmake
	for idl in *.idl; do rm -f $${idl%.idl}*.{{i,h}pp,cxx}; done
