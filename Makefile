# TODO do some way to check if this exists
TBB_INC_FLAG=
TBB_LIB_FLAG=

CXXFLAGS=-O3 -std=c++98 -pedantic -Wall -Wextra -Wno-unused-variable -Werror=format $(TBB_INC_FLAG)
LDLIBS=-lgomp -lOpenCL -lmpi $(TBB_LIB_FLAG)

# factors affecting how it runs
VERB=-DVERBOSE

FILES=\
	ocl_init.o \
	ocl_run.o \
	tbb_cws.o \
	parse.o \
	learning.o \
	cws.o

OUTFILE=vrp-ocl

$(OUTFILE): Makefile link

tbb:
	@echo Making with TBB support
	@$(MAKE) TBB_INC_FLAG=-DCVRP_USE_TBB TBB_LIB_FLAG=-ltbb

link: $(FILES)
	$(CXX) $(CXXFLAGS) -o $(OUTFILE) $(FILES) $(LDFLAGS) $(LDLIBS)

%.o: %.cpp Makefile common_header.hpp
	$(CXX) $(CXXFLAGS) \
	-DCL_USE_DEPRECATED_OPENCL_1_1_APIS \
	$(VERB) \
	-c $<

run: $(OUTFILE)
	./$(OUTFILE)

clean:
	rm -f *.o *.mod *genmod* *.s $(OUTFILE)

