CXXFLAGS=-O3 -march=native -std=c++11 -Wall -Wextra -Wno-unused-variable -Werror=format -ggdb
LDLIBS=-lOpenCL -lmpi -lstdc++ -lm -lgomp

.EXPORT_ALL_VARIABLES: 
.PHONY: clean run

FILES=\
	ocl_init.o \
	ocl_run.o \
	parse.o \
	vrp-ocl.o \
	cws.o
SF=$(addprefix src/,$(FILES))

vrp-ocl: $(SF)
	$(CXX) $(SF) -o vrp-ocl $(LDFLAGS) $(LDLIBS)

$(SF): Makefile

run: vrp-ocl
	./vrp-ocl

clean:
	rm -f vrp-ocl src/*.o

