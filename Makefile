CXXFLAGS=-O3 -std=c++98 -Wall -Wextra -Wno-unused-variable -Werror=format
LDLIBS=-lOpenCL -lmpi -lstdc++ -lm -lgomp

.EXPORT_ALL_VARIABLES: 
.PHONY: objs clean run

FILES=\
	ocl_init.o \
	ocl_run.o \
	parse.o \
	vrp-ocl.o \
	cws.o
SF=$(addprefix src/,$(FILES))

vrp-ocl: $(SF)
	$(CXX) $(SF) -o vrp-ocl $(LDFLAGS) $(LDLIBS)

run: objs
	./vrp-ocl

clean:
	rm -f vrp-ocl src/*.o

