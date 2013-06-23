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

float OCLLearn::getBestRoute
(float& best_route, route_vec_t& best_chromosome, unsigned int ii)
{
    float* min_route;

    const static unsigned int results_size = GLOBAL_SIZE;

    float results_host[results_size];
    float avg = 0.0;
    unsigned int jj;

    // find fitness (length) of routes and copy back
    try
    {
        queue.finish();

        fitness_kernel.setArg(0, buffers.at("parents"));
        queue.enqueueNDRangeKernel(fitness_kernel,
            cl::NullRange, global, local);

        queue.finish();
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in fitness kernel" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        throw e;
    }

    try
    {
        queue.enqueueReadBuffer(buffers.at("results"),
            CL_TRUE, 0,
            sizeof(float) * GLOBAL_SIZE,
            results_host);

        queue.finish();
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
        results_host + results_size);

    if(*min_route < best_route)
    {
        // if its better than previous route copy back
        // read from device to buffer one chromosome at a time
        best_route = *min_route;

        uint load_from = min_route - results_host;

        queue.enqueueReadBuffer(buffers.at("parents"), CL_TRUE,
            // position in chromosome
            load_from
            // size of 1 chromosome
            * (all_chrom_size / GLOBAL_SIZE),
            (all_chrom_size / GLOBAL_SIZE),
            &best_chromosome.at(0));

        #ifdef VERBOSE
        std::cout << std::endl;
        unsigned int real_sz = best_chromosome.size();
        for(jj = 0; jj < real_sz; jj++)
        {
            std::cout << best_chromosome.at(jj) << " ";
        }
        std::cout << std::endl;
        std::cout << *min_route;
        std::cout << " at iteration ";
        std::cout << ii;
        std::cout << std::endl;
        #endif
    }

    avg = std::accumulate(results_host,
        results_host + results_size, 0.0f)/(results_size);

    return avg;
}

alg_result_t OCLLearn::run
(void)
{
    unsigned int ii, lb, ub;
    double t_0, t_1;

    cl::NDRange doub_local(GROUP_SIZE * 2);
    cl::NDRange doub_global(GLOBAL_SIZE * 2);

    t_0 = MPI_Wtime();

    // all valid routes created
    std::vector<route_vec_t> all_routes;

    genChromosomes(all_routes);
    #if 0
    // can't randomise because number of trucks is not guarnateeable
    route_vec_t r(node_coords.size());
    for (ii = 0; ii < r.size(); ii++)
    {
        r[ii] = ii+1;
    }
    all_routes = std::vector<route_vec_t>(GLOBAL_SIZE, r);
    for (ii = 0; ii < GLOBAL_SIZE; ii++)
    {
        std::random_shuffle(all_routes[ii].begin(),
                            all_routes[ii].end());
    }
    #endif

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
    route_vec_t best_chromosome = all_routes.back();

    try
    {
        // TSP
        TSP_kernel.setArg(0, buffers.at("parents"));
        queue.enqueueNDRangeKernel(TSP_kernel,
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
            ii = 0;
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
            CHECK_SIZE(starts_kernel, 4);
            CHECK_SIZE(crossover_kernel, 5);
            CHECK_SIZE(fe_kernel, 6);

            int max_wg_sz = *std::max_element(max_sizes, max_sizes+7);

            // set new local size
            local = cl::NDRange(max_wg_sz/2);
            doub_local = cl::NDRange(max_wg_sz);

            std::cout << "Overriding work group size of " << GROUP_SIZE;
            std::cout << " with work group size of " << max_wg_sz << std::endl;
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
    try
    {
        // TSP
        TSP_kernel.setArg(0, buffers.at("parents"));
        queue.enqueueNDRangeKernel(TSP_kernel,
            cl::NullRange, global, local);

        // fitness
        fitness_kernel.setArg(0, buffers.at("parents"));
        queue.enqueueNDRangeKernel(fitness_kernel,
            cl::NullRange, global, local);

        // sort - always do no elitist on first round
        ne_sort_kernel.setArg(1, buffers.at("parents"));
        queue.enqueueNDRangeKernel(ne_sort_kernel,
            cl::NullRange, global, local);

        // set the top GLOBAL_SIZE to be the next parents
        queue.enqueueCopyBuffer(
            buffers.at("sorted"),
            buffers.at("parents"),
            0, 0, all_chrom_size);
    }
    catch(cl::Error e)
    {
        std::cout << "\nError in initial runs" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << e.err() << std::endl;
        throw e;
    }

    /********************/

    for(ii = 1; ii < NUM_ITER + 1; ii++)
    {
        #ifdef VERBOSE
        std::cout << "\rNow on iteration " << ii << "/";
        std::cout << NUM_ITER << " " << std::flush;
        #endif
        /*
        *   algo
        *   1.  do TSP on routes
        *   2.  sort parents by fitness
        *   3.  breed
        *   4.  TSP
        *   5.  sort all by fitness, elitist or non elitist
        *   6.  copy best back into parents, discard the rest
        *   7.  goto 2
        */

        /********************/

        // do foreign exchange
        try
        {
            // swap best chromosome in each workgroup with another one
            queue.enqueueNDRangeKernel(fe_kernel,
                cl::NullRange, global, local);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in exchange kernel" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        // upper bounds and lower bounds for mutation
        lb = (rand() % (info.node_coords.size() - 3)) + 1;
        ub = (rand() % (info.node_coords.size() - lb - 2)) + lb + 1;
        // make sure they are sane numbers
        if(lb == ub 
        ||lb < 1 || lb > info.node_coords.size()-2
        ||ub < 1 || ub > info.node_coords.size()-2)
        {
            std::cout << std::endl;
            std::cout << lb << " " << ub;
            std::cout << std::endl;
            exit(1);
        }

        // do crossover
        try
        {
            crossover_kernel.setArg(7, lb);
            crossover_kernel.setArg(8, ub);
            queue.enqueueNDRangeKernel(crossover_kernel,
                cl::NullRange, global, local);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in ga kernel" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        // do TSP on children to shorten them
        try
        {
            // TSP
            TSP_kernel.setArg(0, buffers.at("children"));
            queue.enqueueNDRangeKernel(TSP_kernel,
                cl::NullRange, global, local);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in TSP kernel" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        // calculate fitness of children
        try
        {
            fitness_kernel.setArg(0, buffers.at("children"));
            queue.enqueueNDRangeKernel(fitness_kernel,
                cl::NullRange, global, local);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in fitness kernel" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        // sort all chromosomes based on elitism
        try
        {
            // if not elitist, only sort children
            if(sort_strategy == ELITIST)
            {
                static cl::NDRange fitness_offset(GLOBAL_SIZE);

                // fitness of parents for elitist sorting
                // second half of results contains
                fitness_kernel.setArg(0, buffers.at("parents"));
                queue.enqueueNDRangeKernel(fitness_kernel,
                    fitness_offset, global, local);

                // needs doulbe the local range for sorting
                queue.enqueueNDRangeKernel(e_sort_kernel,
                    cl::NullRange, doub_global, doub_local);
            }
            else
            {
                ne_sort_kernel.setArg(1, buffers.at("children"));
                queue.enqueueNDRangeKernel(ne_sort_kernel,
                    cl::NullRange, global, local);
            }

            // set the top GLOBAL_SIZE to be the next parents
            queue.enqueueCopyBuffer(
                buffers.at("sorted"),
                buffers.at("parents"),
                0, 0, all_chrom_size);
        }
        catch(cl::Error e)
        {
            std::cout << "\nError in sort kernel" << std::endl;
            std::cout << e.what() << std::endl;
            std::cout << e.err() << std::endl;
            throw e;
        }

        /********************/

        // see if a better route has been created
        getBestRoute(best_route, best_chromosome, ii);
    }

    queue.finish();
    t_1 = MPI_Wtime();
    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "took " << t_1-t_0 << " secs" << std::endl;
    #endif

    return std::pair<float, route_vec_t>(best_route, best_chromosome);
}

