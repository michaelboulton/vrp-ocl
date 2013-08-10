
all: $(FILES)
	$(CXX) $(CXXFLAGS) $(FILES) -o ../vrp-ocl $(LDFLAGS) $(LDLIBS)

%.o: %.cpp Makefile common_header.hpp ../Makefile
	$(CXX) $(CXXFLAGS) \
	-DCL_USE_DEPRECATED_OPENCL_1_1_APIS \
	-c $<

clean:
	rm -f *.o *.mod *genmod* *.s

