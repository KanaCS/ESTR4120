UNAME := $(shell uname -s)
all:
ifeq ($(UNAME),Linux)
	make -f Makefile.linux
else
ifeq ($(UNAME),Darwin)
	make -f Makefile.linux
else
	gmake -f Makefile.sunos
endif
endif

clean:
ifeq ($(UNAME),Linux)
	make clean -f Makefile.linux
else
ifeq ($(UNAME),Darwin)
	make clean -f Makefile.linux
else
	gmake clean -f Makefile.sunos
endif
endif
