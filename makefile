SHELL = /bin/bash -O globstar

.PHONY: all
all:  cmake-build/Makefile  \
      include/udds/udds.hpp include/udds/broadcast.hpp  \
	  include/udds/udds-broadcast-client.hpp  \
	  src/send-receive.cpp  \
	  src/udds-broadcast-cli.cpp  \
	  src/easily-send-receive.cpp
	cd cmake-build; make -j
	rm -f bin/udds-broadcast-cli
	ln -s ./udds-broadcast-cli.d/udds-broadcast-cli bin/udds-broadcast-cli

.PHONY: install
install: all
	sudo bash -c  \
	"rm -f /{bin,usr/bin}/udds-broadcast-cli; ln -s `pwd -P`/bin/udds-broadcast-cli /bin/"

cmake-build/Makefile: CMakeLists.txt $(wildcard protos/*.idl)
	make clean
	cd protos;  \
	for f_idl in *.idl; do  \
		fastddsgen $$f_idl;  \
	done
	mkdir -p cmake-build; cd cmake-build; cmake -D'CMAKE_BUILD_TYPE=Release' ..

.PHONY: clean
clean:
	cd protos;  \
	for f_idl in *.idl; do  \
		rm -f $${f_idl%.idl}{.hpp,CdrAux.{hpp,ipp},{PubSubTypes,TypeObjectSupport}.{cxx,hpp}};  \
	done
	rm -rf cmake-build
	rm -f ./**/?*~ ./**/.?*~

.PHONY: git
git:
	git add .
	git commit -m ';'
	git push
