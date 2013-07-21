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

#include "common_header.hpp"

// creates a program and checks it works
cl::Program OCLLearn::createProg
(std::string const& filename, std::string const& options)
{
    cl::Program program;

    std::ifstream source_file(filename.c_str());

    std::basic_string<char> source_code(std::istreambuf_iterator<char>(source_file),
        (std::istreambuf_iterator<char>()) );

    cl::Program::Sources source(1, std::make_pair(source_code.c_str(),
                                                  source_code.length()));
    program = cl::Program(context, source);

    std::stringstream errstream;
    std::vector<cl::Device>::iterator dev_it;

    try
    {
        program.build( devices, options.c_str() );
        for(dev_it = devices.begin(); dev_it < devices.end(); dev_it++)
        {
            errstream << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*dev_it);
            //std::string errs( errstream.str() );
            std::cout << errstream.str() << std::endl;
        }
    }
    catch(cl::Error e)
    {
        for(dev_it = devices.begin(); dev_it < devices.end(); dev_it++)
        {
            errstream << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*dev_it);
        }
        std::string errs( errstream.str() );

        //if(strstr(errs.c_str(), "error"))
        {
            std::cout << std::endl;
            std::cout << errs;
            std::cout << std::endl;
            std::cout << "Exiting due to errors" << std::endl;
            exit(1);
        }
    }

    return program;
}

void OCLLearn::initOCL
(void)
{
    std::vector<cl::Platform> platforms;
    try
    {
        cl::Platform::get(&platforms);
    }
    catch(cl::Error e)
    {
        std::cout << "error getting platform ids" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    std::string plat_info;
    unsigned int ii;

    #ifdef VERBOSE
    std::cout << platforms.size() << " platforms";
    std::cout << std::endl;
    std::cout << "looking for devices of type ";
    std::cout << (DEVICE_TYPE==CL_DEVICE_TYPE_CPU ? "CPU" : "GPU") << ": ";
    std::cout << std::endl;
    #endif

    for(ii=0;ii<platforms.size();ii++)
    {
        try
        {
            platforms.at(ii).getInfo(CL_PLATFORM_NAME, &plat_info);
            #ifdef VERBOSE
            std::cout << plat_info;
            #endif

            cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM,
                (cl_context_properties)(platforms.at(ii))(), 0};

            context = cl::Context(DEVICE_TYPE, properties);
            devices = context.getInfo<CL_CONTEXT_DEVICES>();

            std::cout << std::endl;
            break;
        }
        catch(cl::Error e)
        {
            if (devices.size() < 1)
            {
                #ifdef VERBOSE
                std::cout << " - invalid type" << std::endl;
                #endif
            }
            else
            {
                std::cout << "\nError in creating context" << std::endl;
                std::cout << e.what() << std::endl;
                exit(1);
            }
        }
    }

    switch (devices.size())
    {
    case 1:
        device = devices.at(0);
        break;
    case 0:
        std::cout << "No devices of specified type found" << std::endl;
        exit(1);
    default:
        device = devices.at(0);
    }
    queue = cl::CommandQueue(context, device);

    /****   options   *******/

    std::stringstream options;
    //options << "-cl-fast-relaxed-math ";
    //options << "-cl-mad-enable ";
    //options << "-cl-no-signed-zeros ";

    //options << "-cl-opt-disable ";

    // disable testing values
    options << "-DNOTEST ";

    // extra options
    options << "-DNUM_NODES=" << info.node_coords.size() - 1 << " ";
    options << "-DNUM_SUBROUTES=" << NUM_SUBROUTES * 2 << " ";
    options << "-DMAX_PER_ROUTE=" << STOPS_PER_ROUTE << " ";
    options << "-DMAX_CAPACITY=" << info.capacity << " ";
    options << "-DMIN_CAPACITY=" << MIN_CAPACITY << " ";
    options << "-DLOCAL_SIZE=" << LOCAL_SIZE << " ";
    options << "-DGLOBAL_SIZE=" << GLOBAL_SIZE << " ";
    options << "-DDEPOT_NODE=" << info.depot_node << " ";
    options << "-DARENA_SIZE=" << ARENA_SIZE << " ";

    // mutation rate
    options << "-DMUT_RATE=" << MUTRATE << " ";

    // which mutation to use
    if (mutate_strategy == REVERSE)
    {
        #ifdef VERBOSE
        std::cout << "Using random range reverse for mutation" << std::endl;
        #endif
        options << "-DMUT_REVERSE ";
    }
    else if (mutate_strategy == SLIDE)
    {
        #ifdef VERBOSE
        std::cout << "Using slide for mutation" << std::endl;
        #endif
        options << "-DMUT_SLIDE ";
    }
    else if (mutate_strategy == SWAP)
    {
        #ifdef VERBOSE
        std::cout << "Using random range swap for mutation" << std::endl;
        #endif
        options << "-DMUT_SWAP ";
    }

    // which breed algorithm to use
    if (breed_strategy == CX)
    {
        options << "-DBREED_CX ";
        #ifdef VERBOSE
        std::cout << "Using cx crossover" << std::endl;
        #endif
    }
    else if (breed_strategy == PMX)
    {
        options << "-DBREED_PMX ";
        #ifdef VERBOSE
        std::cout << "Using pmx crossover" << std::endl;
        #endif
    }
    else if (breed_strategy == O1)
    {
        options << "-DBREED_O1 ";
        #ifdef VERBOSE
        std::cout << "Using order 1 crossover" << std::endl;
        #endif
    }

    std::cout << "options:" << std::endl;
    std::cout << options.str() << std::endl;

    // compile it
    all_program = createProg(KNL_FILE, options.str());

    /****   buffers   *******/

    // parents for ga
    cl_buf_t chrom_buf(context,
                       CL_MEM_READ_WRITE,
                       all_chrom_size);
    buffers["parents"] = chrom_buf;

    // children of GA
    cl_buf_t children_buf(context,
                          CL_MEM_READ_WRITE,
                          all_chrom_size);
    buffers["children"] = children_buf;

    // holds arrays of length NUM_SUBROUTES corresponding to where sub routes begin and and in each route
    // max 2 times original length
    cl_buf_t route_starts_buf(context,
                              CL_MEM_READ_WRITE,
                              NUM_SUBROUTES * 2 * GLOBAL_SIZE * sizeof(int));
    buffers["starts"] = route_starts_buf;

    // start with size 1 - no node 0
    point_vec_t dev_coords(1);
    std::vector<cl_uint> dev_demands(1, 1000);

    // FIXME hacky?
    for (ii = 1; ii < info.node_coords.size()+1; ii++)
    {
        dev_coords.push_back(info.node_coords.at(ii));
        dev_demands.push_back(info.node_demands.at(ii));
    }

    // node coordinates
    cl_buf_t node_coord_buf(context,
                            CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                            sizeof(point_t) * dev_coords.size(),
                            &dev_coords.at(0));
    queue.enqueueWriteBuffer(node_coord_buf, CL_TRUE, 0,
                             sizeof(point_t) * dev_coords.size(),
                             &dev_coords.at(0));
    buffers["coords"] = node_coord_buf;

    // demands for nodes
    cl_buf_t node_demands_buf(context,
                              CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                              sizeof(cl_uint) * dev_demands.size(),
                              &dev_demands.at(0));
    queue.enqueueWriteBuffer(node_demands_buf, CL_TRUE, 0,
                             sizeof(cl_uint) * dev_demands.size(),
                             &dev_demands.at(0));
    buffers["demands"] = node_demands_buf;

    // length of route
    cl_buf_t results_buf(context,
                         CL_MEM_READ_WRITE,
                         2 * sizeof(float) * GLOBAL_SIZE);
    buffers["results"] = results_buf;

    // output of bitonic sort kernel
    cl_buf_t sorted_buf(context,
                        CL_MEM_READ_WRITE,
                        2 * all_chrom_size);
    buffers["sorted"] = sorted_buf;

    // hold state for random number generation
    cl_buf_t rand_state_buf(context,
                            CL_MEM_READ_WRITE,
                            GLOBAL_SIZE * sizeof(cl_uint2));
    // randomise seeds
    std::vector<cl_uint2> rand_vec;
    for(ii = 0; ii < GLOBAL_SIZE; ii++)
    {
        cl_uint2 randnum = {{rand(), rand()}};
        rand_vec.push_back(randnum);
    }
    queue.enqueueWriteBuffer(rand_state_buf,
                             CL_TRUE, 0,
                             GLOBAL_SIZE * sizeof(cl_uint2),
                             &rand_vec.at(0));
    buffers["rand_state"] = rand_state_buf;

    // test rand num
    cl_buf_t rand_out_buf(context,
                          CL_MEM_READ_WRITE,
                          all_chrom_size * 2);
    buffers["rand_out"] = rand_out_buf;

    /****   kernels   *******/

    // mutate kernel
    try
    {
        mutate_kernel = cl_knl_t(all_program, "mutate");

        mutate_kernel.setArg(0, buffers.at("children"));
        mutate_kernel.setArg(1, buffers.at("rand_state"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making mutate kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // find route starts kernel
    try
    {
        frs_kernel = cl_knl_t(all_program, "findRouteStarts");

        frs_kernel.setArg(1, buffers.at("demands"));
        frs_kernel.setArg(2, buffers.at("starts"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making frs kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // fitness kernel
    try
    {
        fitness_kernel = cl_knl_t(all_program, "fitness");

        fitness_kernel.setArg(1, buffers.at("results"));
        fitness_kernel.setArg(2, buffers.at("coords"));
        fitness_kernel.setArg(3, buffers.at("starts"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making fitness kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // TSP kernel
    try
    {
        if(tsp_strategy == SIMPLE || tsp_strategy == NONE)
        {
            TSP_kernel = cl_knl_t(all_program, "simpleTSP");
        }

        TSP_kernel.setArg(1, buffers.at("starts"));
        TSP_kernel.setArg(2, buffers.at("coords"));
        TSP_kernel.setArg(3, buffers.at("demands"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making TSP kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // non elitist sorting kernel
    try
    {
        ne_sort_kernel = cl_knl_t(all_program, "ParallelBitonic_NonElitist");

        ne_sort_kernel.setArg(0, buffers.at("results"));
        ne_sort_kernel.setArg(2, buffers.at("sorted"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making sort kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // elitist sorting kernel
    try
    {
        e_sort_kernel = cl_knl_t(all_program, "ParallelBitonic_Elitist");

        e_sort_kernel.setArg(0, buffers.at("results"));
        e_sort_kernel.setArg(1, buffers.at("parents"));
        e_sort_kernel.setArg(2, buffers.at("children"));
        e_sort_kernel.setArg(3, buffers.at("sorted"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making sort kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // breeding kernel
    try
    {
        crossover_kernel = cl_knl_t(all_program, "breed");

        crossover_kernel.setArg(0, buffers.at("parents"));
        crossover_kernel.setArg(1, buffers.at("children"));
        crossover_kernel.setArg(2, buffers.at("starts"));
        crossover_kernel.setArg(3, buffers.at("results"));
        crossover_kernel.setArg(4, buffers.at("coords"));
        crossover_kernel.setArg(5, buffers.at("demands"));
        crossover_kernel.setArg(6, buffers.at("rand_state"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making crossover kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // foreign exchange kernel
    try
    {
        fe_kernel = cl_knl_t(all_program, "foreignExchange");

        fe_kernel.setArg(0, buffers.at("parents"));
        fe_kernel.setArg(1, buffers.at("rand_state"));
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in making exchange kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        exit(1);
    }

    // sizes for workgroup
    global = cl::NDRange(GLOBAL_SIZE);
    local = cl::NDRange(LOCAL_SIZE);
}

OCLLearn::OCLLearn
(RunInfo const& run_info)
:info(run_info)
{
    srand(time(NULL));

    // size in bytes of entire population is
    std::cout << run_info.node_coords.size() << std::endl;
    all_chrom_size = (run_info.node_coords.size() - 1)
                     * GLOBAL_SIZE * sizeof(cl_uint);

    initOCL();

    all_stops = route_vec_t(run_info.node_coords.size(), 0);

    // initialise 'best route' to be 0, 1, 2, 3, 4, ...
    unsigned int ii;
    for (ii = 0; ii < run_info.node_coords.size(); ii++)
    {
        all_stops.at(ii) = ii + 2;
    }
}

