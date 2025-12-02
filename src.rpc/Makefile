SHELL = /bin/bash -O globstar

urpc: FORCE
	$(MAKE) build.cmake/Makefile
	cd build.cmake; $(MAKE)
	mv -f build.cmake/urpc .

build.cmake/Makefile: CMakeLists.txt ShynurUrpcProcessor.idl | build.cmake/
	cd build.cmake;  \
	CXXFLAGS+=' -O0 -g3' CMAKE_GENERATOR='Unix Makefiles' cmake -DCMAKE_BUILD_TYPE=Debug ..

%/:
	mkdir -p $@

.PHONY: FORCE

.PHONY: clean
clean:
	for f in ./* ./.*; do  \
		f=`basename -- $$f`;  \
		case "$$f" in  \
			shynur-polyfill.hpp | nlohmann | Makefile | CMakeLists.txt | .gitignore | a.cpp | urpc.hpp | ShynurUrpcProcessor.idl | .vscode | compile_flags.txt) :;;  \
			*) rm -rf "./$$f";;  \
		esac;  \
	done
