SHELL = /bin/bash

.PHONY: clean
clean:
	cd protos; for f_idl in *.idl; do  \
		rm -f $${f_idl%.idl}{.hpp,CdrAux.{hpp,ipp},{PubSubTypes,TypeObjectSupport}.{cxx,hpp}};  \
	done
	rm -rf build; mkdir -p build; touch build/.empty
