SHELL = /bin/bash -O globstar

.PHONY: all
all:  build/Makefile  \
      include/udds.hpp include/broadcast.hpp  \
	  src/send.cpp src/receive.cpp  \
	  src/udds-broadcast-cli.cpp
	cd build; make -j

build/Makefile: CMakeLists.txt $(wildcard protos/*.idl)
	make clean
	cd protos;  \
	for f_idl in *.idl; do  \
		fastddsgen $$f_idl;  \
	done
	mkdir -p build; cd build; cmake ..

.PHONY: clean
clean:
	cd protos;  \
	for f_idl in *.idl; do  \
		rm -f $${f_idl%.idl}{.hpp,CdrAux.{hpp,ipp},{PubSubTypes,TypeObjectSupport}.{cxx,hpp}};  \
	done
	rm -rf build
	rm -f ./**/?*~ ./**/.?*~

.PHONY: git
git:
	git add .
	git commit -m ';'
	git push
