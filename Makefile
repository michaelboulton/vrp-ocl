CXX_COMPILER=g++
CFLAGS=-O3 -Wno-reorder -Wno-unused -Wno-comment -Werror=parentheses -fopenmp -g
LDFLAGS=-lgomp -lOpenCL

# factors affecting how it runs
TYPE=GPU
NUM_ITER=1000
GLOBAL_SIZE=4096
GROUP_SIZE=512
VERB=-DVERBOSE
BLOCKING=-DBLOCK_CALLS

FILES=ocl.o learning.o

OUTFILE=out.exe

# make gpu by default
all: Makefile link

link: $(FILES)
	$(CXX_COMPILER) -o $(OUTFILE) $(FILES) $(LDFLAGS)

%.o: %.cpp
	$(CXX_COMPILER) $(CFLAGS) \
	-DOCL_TYPE=CL_DEVICE_TYPE_$(TYPE) \
	-DCL_USE_DEPRECATED_OPENCL_1_1_APIS \
	-DNUM_ITER=$(NUM_ITER) \
	-DGLOBAL_SIZE=$(GLOBAL_SIZE) \
	-DGROUP_SIZE=$(GROUP_SIZE) \
	$(VERB) \
	-c $< -I$(CUDA_HOME)/include

run: all
	./$(OUTFILE)

rerun:
	./$(OUTFILE)

# sub to emerald
sub: all
	bsub -Is -n 1 bash	

clean:
	rm -f *.o *.mod *genmod* *.s $(OUTFILE)

test:
	ioc -input=knl.cl -asm=compiled.s -simd=avx

# include opencl library and kernels in this?
executable: 
	rm -f $(EXEC_OUT)
	echo "g++ -o $(EXTRACT_DIR)/$(PROJNAME)_carma.exe *.o \$$LDFLAGS -lcudart -lgfortran -lgomp" > $(OBJDIR)/link.sh
	chmod +x $(OBJDIR)/link.sh
	makeself $(OBJDIR) $(EXEC_OUT) "unlinked $(PROJNAME)" ./link.sh --nox11

