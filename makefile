SHELL = /bin/bash -O globstar

.PHONY: all
all:  build/Makefile  \
      include/udds/udds.hpp include/udds/broadcast.hpp  \
	  include/udds/udds-broadcast-client.hpp  \
	  src/example.use-udds-header-file/send.cpp src/example.use-udds-header-file/receive.cpp  \
	  src/udds-broadcast-cli.cpp  \
	  src/example.use-individual-process/easily-send.cpp src/example.use-individual-process/easily-receive.cpp
	cd build; make -j
	rm -f build/udds-broadcast-cli
	ln -s `pwd -P`/build/udds-broadcast-cli.d/udds-broadcast-cli udds-broadcast-cli
	sudo bash -c  \
	"rm -f /{bin,usr/bin}/udds-broadcast-cli; ln -s `pwd -P`/build/udds-broadcast-cli /bin/"

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
