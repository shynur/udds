SHELL = /bin/bash -O globstar

.PHONY: all
all:
	_SHYNUR_URPC_CMAKE_DIR=`pwd` _SHYNUR_URPC_BUILD_DIR=/tmp/$$RANDOM;  \
	mkdir $$_SHYNUR_URPC_BUILD_DIR;  \
	cd $$_SHYNUR_URPC_BUILD_DIR;  \
	CXX=g++-15 CXXFLAGS='-std=c++26 -O0 -g3' cmake $$_SHYNUR_URPC_CMAKE_DIR;  \
	make -j;  \
	mv urpc $$_SHYNUR_URPC_CMAKE_DIR

.PHONY: clean
clean:
	for f in ./* ./.*; do  \
		f=`basename -- $$f`;  \
		case "$$f" in  \
			Makefile | CMakeLists.txt | .gitignore | a.cpp | urpc.hpp | ShynurUrpcProcessor.idl | .vscode) :;;  \
			*) rm -rf "./$$f";;  \
		esac;  \
	done
