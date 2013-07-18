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

extern "C" double MPI_Wtime();

std::string errToString(cl_int err)
{
    switch (err) {
        case CL_SUCCESS:                            return "Success!";
        case CL_DEVICE_NOT_FOUND:                   return "Device not found.";
        case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
        case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
        case CL_OUT_OF_RESOURCES:                   return "Out of resources";
        case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
        case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
        case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
        case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
        case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
        case CL_MAP_FAILURE:                        return "Map failure";
        case CL_INVALID_VALUE:                      return "Invalid value";
        case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
        case CL_INVALID_PLATFORM:                   return "Invalid platform";
        //case CL_INVALID_PROPERTY:                   return "Invalid property";
        case CL_INVALID_DEVICE:                     return "Invalid device";
        case CL_INVALID_CONTEXT:                    return "Invalid context";
        case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
        case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
        case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
        case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
        case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
        case CL_INVALID_SAMPLER:                    return "Invalid sampler";
        case CL_INVALID_BINARY:                     return "Invalid binary";
        case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
        case CL_INVALID_PROGRAM:                    return "Invalid program";
        case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
        case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
        case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
        case CL_INVALID_KERNEL:                     return "Invalid kernel";
        case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
        case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
        case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
        case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
        case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
        case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
        case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
        case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
        case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
        case CL_INVALID_EVENT:                      return "Invalid event";
        case CL_INVALID_OPERATION:                  return "Invalid operation";
        case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
        case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
        case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
        default: return "Unknown";
    }
}

void OCLLearn::enqueueKernel
(cl::Kernel const& kernel,
int line, const char* file,
const cl::NDRange offset_range,
const cl::NDRange global_range,
const cl::NDRange local_range)
{
    try
    {
        // just launch kernel
        queue.enqueueNDRangeKernel(kernel,
                                   offset_range,
                                   global_range,
                                   local_range);
    }
    catch (cl::Error e)
    {
        std::string func_name;
        kernel.getInfo(CL_KERNEL_FUNCTION_NAME, &func_name);

        fflush(stderr);
        fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@\n");
        fprintf(stderr, "Error in enqueueing kernel '%s' at line %d in %s\n",
                func_name.c_str(), line, file);
        fprintf(stderr, "Error in %s, code %d (%s) - exiting\n",
                e.what(), e.err(), errToString(e.err()).c_str());
        fprintf(stderr, "@@@@@@@@@@@@@@@@@@@@\n");
        fflush(stderr);

        //exit(e.err());
        throw e;
    }
}

float OCLLearn::getBestRoute
(float& best_route, route_vec_t& best_chromosome)
{
    float* min_route;

    float results_host[GLOBAL_SIZE];
    float avg = 0.0;
    unsigned int jj;

    // find fitness (length) of routes and copy back
    frs_kernel.setArg(0, buffers.at("parents"));
    ENQUEUE(frs_kernel);
    fitness_kernel.setArg(0, buffers.at("parents"));
    ENQUEUE(fitness_kernel);

    try
    {
        queue.finish();
        queue.enqueueReadBuffer(buffers.at("results"),
                                CL_TRUE, 0,
                                sizeof(float) * GLOBAL_SIZE,
                                results_host);

    }
    catch(cl::Error e)
    {
        std::cout << "\nError in reading results back" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        throw e;
    }

    // print out best route + length of it
    // could do on device for improved speed
    min_route = std::min_element(results_host,
                                 results_host + GLOBAL_SIZE);

    if(*min_route < best_route)
    {
        // if its better than previous route copy back
        // read from device to buffer one chromosome at a time
        best_route = *min_route;

        uint load_from = min_route - results_host;

        queue.enqueueReadBuffer(buffers.at("parents"), CL_TRUE,
                                // position in chromosome
                                load_from *
                                    // size of 1 chromosome
                                    (all_chrom_size / GLOBAL_SIZE),
                                (all_chrom_size / GLOBAL_SIZE),
                                &best_chromosome.at(0));
    }

    avg = std::accumulate(results_host,
                          results_host + GLOBAL_SIZE,
                          0.0f) / GLOBAL_SIZE;

    return avg;
}

alg_result_t OCLLearn::run
(void)
{
    unsigned int ii, jj;
    double t_0, t_1;

    cl::NDRange doub_local(LOCAL_SIZE * 2);
    cl::NDRange doub_global(GLOBAL_SIZE * 2);

    t_0 = MPI_Wtime();

    // all valid routes created
    std::vector<route_vec_t> all_routes;

    // generate a load of random routes
    genChromosomes(all_routes);

    std::random_shuffle(all_routes.begin(), all_routes.end());
    #ifdef VERBOSE
    std::cout << " done" << std::endl;
    #endif

    // need to write each one to the buffer sequentially
    for(ii=0; ii<all_routes.size(); ii++)
    {
        queue.enqueueWriteBuffer(buffers.at("parents"),
                                 CL_TRUE,
                                 ii * (all_chrom_size / GLOBAL_SIZE),
                                 (all_chrom_size / GLOBAL_SIZE),
                                 &all_routes.at(ii).at(0));
    }
    queue.finish();

    float best_route = std::numeric_limits<float>::max();
    float new_best = best_route;
    route_vec_t best_chromosome = all_routes.back();

    try
    {
        // TSP
        fitness_kernel.setArg(0, buffers.at("parents"));
        queue.enqueueNDRangeKernel(fitness_kernel,
            cl::NullRange, global, local);
        queue.finish();
    }
    catch(cl::Error e)
    {
        // if the work group size is invalid, its on AMD so 128 is the max wg size
        if(e.err() == -55)
        {
            // check to see what the maximum work group size is for all kernels
            int max_sizes[7];
            #define CHECK_SIZE(knl, idx) \
            cl::detail::errHandler( \
                clGetKernelWorkGroupInfo(knl(), \
                                         device(), \
                                         CL_KERNEL_WORK_GROUP_SIZE, \
                                         sizeof(size_t), \
                                         &max_sizes[idx], \
                                         NULL));

            CHECK_SIZE(fitness_kernel, 0);
            CHECK_SIZE(TSP_kernel, 1);
            CHECK_SIZE(e_sort_kernel, 2);
            CHECK_SIZE(ne_sort_kernel, 3);
            CHECK_SIZE(mutate_kernel, 4);
            CHECK_SIZE(crossover_kernel, 5);
            CHECK_SIZE(fe_kernel, 6);

            int max_wg_sz = *std::max_element(max_sizes, max_sizes+7);

            // set new local size
            local = cl::NDRange(max_wg_sz/2);
            doub_local = cl::NDRange(max_wg_sz);

            std::cout << "Overriding work group size of " << LOCAL_SIZE;
            std::cout << " with work group size of " << max_wg_sz << std::endl;

            ENQUEUE(fitness_kernel)
        }
        else
        {
            std::cout << "\nUnable to launch kernels on device" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }
    }

    // do initial TSP and sort in preparation for breeding
    TSP_kernel.setArg(0, buffers.at("parents"));
    ENQUEUE(TSP_kernel)

    frs_kernel.setArg(0, buffers.at("parents"));
    ENQUEUE(frs_kernel);

    fitness_kernel.setArg(0, buffers.at("parents"));
    ENQUEUE(fitness_kernel)

    // sort - always do no elitist on first round because there are no children
    ne_sort_kernel.setArg(1, buffers.at("parents"));
    ENQUEUE(ne_sort_kernel)

    try
    {
        // set the top GLOBAL_SIZE to be the next parents
        queue.finish();
        queue.enqueueCopyBuffer(buffers.at("sorted"),
                                buffers.at("parents"),
                                0, 0, all_chrom_size);
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in initial copy" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        throw e;
    }

    /********************/

    fprintf(stdout, "Now going for %zu iterations or %d seconds\n", GENERATIONS, int(MAX_TIME));

    for (ii = 1; ii < GENERATIONS + 1; ii++)
    {
        #ifdef VERBOSE
        std::cout << "\rNow on iteration " << ii << "/";
        std::cout << GENERATIONS << " " << std::flush;
        #endif
        /*
        *   algo
        *   1.  do foreign exchange
        *   2.  breed
        *   3.  mutate
        *   4.  TSP
        *   5.  calculate fitness
        *   6.  sort all by fitness, elitist or non elitist
        *   7.  copy best back into parents, discard the rest
        *   8.  sort parents by fitness
        *   9.  goto 1
        */

        /********************/

        // do foreign exchange
        ENQUEUE(fe_kernel)

        // breed
        ENQUEUE(crossover_kernel)

        // mutate
        ENQUEUE(mutate_kernel);

        // find route starts for children
        frs_kernel.setArg(0, buffers.at("children"));
        ENQUEUE(frs_kernel);

        // TSP
        //TSP_kernel.setArg(0, buffers.at("children"));
        //ENQUEUE(TSP_kernel)

        // calculae fitness
        fitness_kernel.setArg(0, buffers.at("children"));
        ENQUEUE(fitness_kernel)

        // sort all chromosomes based on elitism choice
        if(ELITIST == sort_strategy)
        {
            static cl::NDRange fitness_offset(GLOBAL_SIZE);

            // find route starts for parents
            frs_kernel.setArg(0, buffers.at("parents"));
            ENQUEUE(frs_kernel);

            // fitness of parents for elitist sorting
            fitness_kernel.setArg(0, buffers.at("parents"));
            this->enqueueKernel(fitness_kernel,
                                __LINE__, __FILE__,
                                fitness_offset,
                                global,
                                local);

            // needs doulbe the local range for sorting
            this->enqueueKernel(e_sort_kernel,
                                __LINE__, __FILE__,
                                cl::NullRange,
                                doub_global,
                                doub_local);
        }
        else
        {
            // if not elitist, only sort children
            ne_sort_kernel.setArg(1, buffers.at("children"));
            ENQUEUE(ne_sort_kernel)
        }

        try
        {
            // set the top GLOBAL_SIZE to be the next parents
            queue.finish();
            queue.enqueueCopyBuffer(buffers.at("sorted"),
                                    buffers.at("parents"),
                                    0, 0, all_chrom_size);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in copying top" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        /*
         *  TODO - if a population goes stale for ~50-100 generations, generate
         *  a new set of chromosomes, send to device, go again. Possibly use
         *  openmp task parallel to launch it asynchronously
         */

        // see if a better route has been created
        getBestRoute(new_best, best_chromosome);

        if (new_best < best_route)
        {
            best_route = new_best;
            #ifdef VERBOSE
            std::cout << std::endl;
            unsigned int real_sz = best_chromosome.size();
            for(jj = 0; jj < real_sz; jj++)
            {
                std::cout << best_chromosome.at(jj) << " ";
            }
            fprintf(stdout,
                    "\n%f at iteration %d after %.2lf secs\n",
                    best_route, ii, MPI_Wtime() - t_0);
            #endif
        }

        if (MPI_Wtime() - t_0 > MAX_TIME)
        {
            fprintf(stdout, "\n===============\n");
            fprintf(stdout, "\nHit clock limit at generation %d\n", ii);
            break;
        }
    }

    queue.finish();
    t_1 = MPI_Wtime();
    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "took " << t_1-t_0 << " secs" << std::endl;
    #endif

    return std::pair<float, route_vec_t>(best_route, best_chromosome);
}

