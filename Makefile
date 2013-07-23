CXXFLAGS=-O3 -g -std=c++98 -pedantic -Wall -Wextra -Wno-unused-variable -Werror=format
LDLIBS=-lgomp -lOpenCL -lmpi

# factors affecting how it runs
VERB=-DVERBOSE

#-DOCL_TYPE=CL_DEVICE_TYPE_$(TYPE) \
 -DNUM_ITER=$(NUM_ITER) \
 -DGLOBAL_SIZE=$(GLOBAL_SIZE) \
 -DGROUP_SIZE=$(GROUP_SIZE) \

FILES=\
	ocl_init.o \
	ocl_run.o \
	tbb_cws.o \
	parse.o \
	learning.o \
	cws.o

OUTFILE=vrp-ocl

# make gpu by default
$(OUTFILE): Makefile link

link: $(FILES)
	$(CXX) $(CXXFLAGS) -o $(OUTFILE) $(FILES) $(LDFLAGS) $(LDLIBS)

%.o: %.cpp Makefile common_header.hpp
	$(CXX) $(CXXFLAGS) \
	-DCL_USE_DEPRECATED_OPENCL_1_1_APIS \
	$(VERB) \
	-c $<

big: $(OUTFILE)
	./$(OUTFILE) -s 50 -g 32 -f 560.vrp

run: $(OUTFILE)
	./$(OUTFILE)

clean:
	rm -f *.o *.mod *genmod* *.s $(OUTFILE)

# include opencl library and kernels in this?
executable: 
	rm -f $(EXEC_OUT)
	echo "g++ -o $(EXTRACT_DIR)/$(PROJNAME)_carma.exe *.o \$$LDLIBS -lcudart -lgfortran -lgomp" > $(OBJDIR)/link.sh
	chmod +x $(OBJDIR)/link.sh
	makeself $(OBJDIR) $(EXEC_OUT) "unlinked $(PROJNAME)" ./link.sh --nox11

