CXXFLAGS=-O2 -g -fopenmp -ansi -pedantic -Wall
LDLIBS=-lgomp -lOpenCL

# factors affecting how it runs
TYPE=GPU
NUM_ITER=1000
GLOBAL_SIZE=4096
GROUP_SIZE=512
VERB=-DVERBOSE
BLOCKING=-DBLOCK_CALLS

FILES=\
	ocl_init.o \
	ocl_run.o \
	learning.o \
	cws.o

OUTFILE=out.exe

# make gpu by default
$(OUTFILE): Makefile link

link: $(FILES)
	$(CXX) $(CXXFLAGS) -o $(OUTFILE) $(FILES) $(LDLIBS)

%.o: %.cpp Makefile common_header.hpp
	$(CXX) $(CXXFLAGS) \
	-DOCL_TYPE=CL_DEVICE_TYPE_$(TYPE) \
	-DCL_USE_DEPRECATED_OPENCL_1_1_APIS \
	-DNUM_ITER=$(NUM_ITER) \
	-DGLOBAL_SIZE=$(GLOBAL_SIZE) \
	-DGROUP_SIZE=$(GROUP_SIZE) \
	$(VERB) \
	-c $<

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

