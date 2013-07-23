/*
 *  Copyright (c) 2013 Michael Boulton
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

#include <fstream>
#include <sstream>
#include <iostream>

#include <map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

// GNU extension for getting options - ok?
#include <getopt.h>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#define KNL_FILE "knl.cl"

class RunInfo;
class OCLLearn;

//typedef std::pair<int, int> point_t;
// essentially the same thing
//typedef struct point {
//    int first;
//    int second;
//} point_t;
typedef cl_float2 point_t;

// index of 2 points and the distance between them
typedef struct point_info {
    unsigned int first_index;
    unsigned int second_index;
    float distance;
} point_info_t;

// vector of all points
typedef std::vector< point_t > point_vec_t;
// vector of vector of points
typedef std::vector< point_info_t > point_info_vec_t;
// vector of indexes of points
typedef std::vector< unsigned int > route_vec_t;
// combination of the length + the route
typedef std::pair< float, route_vec_t > alg_result_t;
// mapping the demand of each node
typedef std::map< cl_uint, cl_uint > demand_vec_t;
// node coords 
typedef std::map< unsigned int, point_t > node_map_t;

float euclideanDistance (point_t, point_t);
void printRoute (const route_vec_t& route, const RunInfo& info);
void parseArgs (int argc, char* argv[]);

class RunInfo
{
public:
    // capacity of each truck
    unsigned int capacity;
    // depot node - not to be included in routes
    unsigned int depot_node;
    // cartesian coordinates of each node
    node_map_t node_coords;
    // demand for each node
    demand_vec_t node_demands;
    // list of all possible pairs of points sorted by their distance to each other
    point_info_vec_t CWS_pair_list;

    // generate sorted list of pairs
    void genSortedCWS (void);

    // get data from file
    void parseInput (void);
};

class OCLLearn{

    typedef cl::Buffer cl_buf_t;
    typedef cl::Kernel cl_knl_t;

private:

    // opencl stuff
    cl::Context context;
    std::vector<cl::Device> devices;
    cl::Device device;
    cl::CommandQueue queue;
    cl::Program all_program;

    // info
    RunInfo info;

    //0, 1, 2, 3, 4, ...., 75
    route_vec_t all_stops;

    // kernels
    cl_knl_t fitness_kernel;
    cl_knl_t frs_kernel;
    cl_knl_t TSP_kernel;
    cl_knl_t e_sort_kernel;
    cl_knl_t ne_sort_kernel;
    cl_knl_t crossover_kernel;
    cl_knl_t fe_kernel;
    cl_knl_t mutate_kernel;

    // size in bytes of all the chromosomes
    size_t all_chrom_size;

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
    (route_vec_t& route,
    unsigned int& current_capacity,
    unsigned int pair_first, unsigned int pair_second);

    int nodesLeft
    (route_vec_t const& total_route,
    route_vec_t & remaining);

    float getBestRoute
    (float&, route_vec_t&);

    // enqueue a kernel
    void enqueueKernel
    (cl::Kernel const& kernel,
    int line, const char* file,
    const cl::NDRange offset,
    const cl::NDRange global_range,
    const cl::NDRange local_range);

    #define ENQUEUE(knl)                                    \
        OCLLearn::enqueueKernel(knl, __LINE__, __FILE__,    \
                                cl::NullRange,              \
                                GLOBAL_SIZE,                \
                                LOCAL_SIZE);

    // for profiling
    std::map<std::string, double> kernel_times;

    // for seeing which populations have gone stale
    std::vector<float> pop_routes;

public:

    alg_result_t run
    (void);

    OCLLearn
    (const RunInfo& run_info);

};

#if 0
#include "tbb/task_scheduler_init.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_do.h"
#include "tbb/concurrent_vector.h"
#endif

class TBBRouteMaker
{
private:
    const point_info_vec_t& CWS_pair_list;
    const node_map_t& node_coords;
    const demand_vec_t& node_demands;

    //const RunInfo info;
    //const route_vec_t all_stops;

public:
    TBBRouteMaker
    (const point_info_vec_t& CWS_pair_list,
     const node_map_t& node_coords,
     const demand_vec_t& node_demands);

    void addNode
    (route_vec_t& route, route_vec_t& total_route,
    unsigned int& current_capacity,
    unsigned int pair_first, unsigned int pair_second) const;

    void operator()
    (route_vec_t& route) const;
    //(const tbb::blocked_range<size_t>& r) const;
};

// how to solve TSP
enum tsp_e {SIMPLE, NONE};
// which mutation to use
enum mut_e {REVERSE, SWAP, SLIDE};
// whether to be elitist or not
enum sort_e {ELITIST, NONELITIST};
// pmx or crossover
enum breed_e {CX, PMX, O1};

extern tsp_e tsp_strategy;
extern mut_e mutate_strategy;
extern sort_e sort_strategy;
extern breed_e breed_strategy;

// mutation rate - out of 100%
extern int MUTRATE;
// number of trucks in whole route - 7 or 8
extern unsigned int NUM_SUBROUTES;
// max number of stops per route
extern unsigned int STOPS_PER_ROUTE;
// min capacity before route making will give up
extern unsigned int MIN_CAPACITY;
// % chance of taking a pair when making initial routes
// too high and GA does nothing, too low and GA doesnt converge
// 95 seems good - not deterministic, but produces good initial results
extern unsigned int RAND_THRESHOLD;
// number of parents to consider in arena selection
extern unsigned int ARENA_SIZE;
// iterations
extern size_t GENERATIONS;

// whether to print everything out
extern int VERBOSE_OUTPUT;

// opencl params
extern int DEVICE_TYPE;
extern size_t GLOBAL_SIZE;
extern size_t LOCAL_SIZE;

// file to read input from
extern std::string INPUT_FILE;

// max time to run for
extern double MAX_TIME;

// whether to profile kernels
extern bool PROFILER_ON;

