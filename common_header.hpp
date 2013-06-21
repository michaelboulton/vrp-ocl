#include <fstream>
#include <sstream>
#include <iostream>

#include <limits>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "CL/cl.hpp"

//typedef std::pair<int, int> point_t;
// essentially the same thing
typedef struct point {
    int first;
    int second;
    point(int f, int s):first(f),second(s){}
}point_t;

typedef struct point_info {
    unsigned int first_index;
    unsigned int second_index;
    float distance;
} point_info_t;

typedef std::vector< point_t > point_vec_t;
typedef std::vector< point_info_t > point_info_vec_t;
typedef std::vector< unsigned int > route_vec_t;
typedef std::pair< const void*, size_t > bin_pair_t;

typedef std::pair< float, route_vec_t > alg_result_t;

float euclideanDistance (point_t, point_t);

class RunInfo
{
public:
    int capacity;
    point_vec_t node_coords;
    point_vec_t node_demands;
    point_info_vec_t CWS_pair_list;

    // get sorted list of pairs
    void getSortedCWS (void);

    // get data from file
    void parseInput (std::string const& settings_file);

    RunInfo
    (std::string const& input_file);
};

class OCLLearn{

    typedef cl::Buffer cl_buf_t;
    typedef cl::Kernel cl_knl_t;
    typedef cl::Image2D cl_img_t;

private:

    cl::Context context;
    std::vector<cl::Device> devices;
    cl::CommandQueue queue;
    cl::Program all_program;

    //0, 1, 2, 3, 4, ...., 75
    route_vec_t all_stops;

    // used for making initial routes
    int capacity;
    point_vec_t node_coords;
    point_vec_t node_demands;
    point_info_vec_t CWS_pair_list;

    // opencl device type
    int DT;

    // kernels
    cl_knl_t simple_swap_kernel;
    cl_knl_t fitness_kernel;
    cl_knl_t TSP_kernel;
    cl_knl_t DJ_kernel;
    cl_knl_t e_sort_kernel;
    cl_knl_t ne_sort_kernel;
    cl_knl_t starts_kernel;
    cl_knl_t crossover_kernel;
    cl_knl_t pmx_kernel;
    cl_knl_t fe_kernel;

    // size in bytes of all the chromosomes
    const size_t all_chrom_size;

    // work sizes
    cl::NDRange global;
    cl::NDRange local;

    // buffer map
    std::map<std::string, cl_buf_t> buffers;

    // creates a program and checks it works
    cl::Program createProg
    (std::string const& filename, std::string const& options);

    void initOCL
    (void);

    void genChromosomes
    (std::vector<route_vec_t>&);

    void addNode
    (route_vec_t& route, route_vec_t const& total_route,
    int& current_capacity, const unsigned int ii,
    unsigned int pair_first, unsigned int pair_second);

    int nodesLeft
    (route_vec_t const& total_route, route_vec_t& trace_diff_result);

    float getBestRoute
    (float&, route_vec_t&, unsigned int);

public:

    alg_result_t run
    (void);

    OCLLearn
    (RunInfo const& run_info, int dt);

};

