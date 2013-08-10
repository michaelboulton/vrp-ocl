CXXFLAGS=-O3 -std=c++98 -pedantic -Wall -Wextra -Wno-unused-variable -Werror=format
LDLIBS=-lOpenCL -lmpi -lgomp

FILES=\
	ocl_init.o \
	ocl_run.o \
	parse.o \
	vrp-ocl.o \
	cws.o

all: Makefile objs

.EXPORT_ALL_VARIABLES: 
.PHONY: objs

objs:
	make -C src

run: $(OUTFILE)
	./$(OUTFILE)

clean:
	$(MAKE) clean -C src
	rm -f vrp-ocl

