SHELL = /bin/bash -O globstar

.PHONY: all
all:
	mkdir -p build.cmake
	cd build.cmake;  \
	CXX=g++-15 CXXFLAGS='-std=c++26 -O0 -g3' cmake ..;  \
	$(MAKE);  \
	mv -f urpc ..

.PHONY: clean
clean:
	for f in ./* ./.*; do  \
		f=`basename -- $$f`;  \
		case "$$f" in  \
			Makefile | CMakeLists.txt | .gitignore | a.cpp | urpc.hpp | ShynurUrpcProcessor.idl | .vscode) :;;  \
			*) rm -rf "./$$f";;  \
		esac;  \
	done
