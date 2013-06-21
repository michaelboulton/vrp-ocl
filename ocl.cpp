
#define __CL_ENABLE_EXCEPTIONS
#include "common_header.hpp"

#include "omp.h"

#define KNL_FILE "knl.cl"

// print out a load of stuff at various stages
//#define VERBOSE

// how to solve TSP
enum {KOPT_2 = 2, KOPT_3 = 3, DJ, SIMPLE, NONE};
const static int tsp_strategy = SIMPLE;
// which mutation to use
enum {REVERSE, SWAP};
const static int mutate_strategy = SWAP;
// mutation rate - out of 100%
const static int mutrate = 25;
// whether to be elitist or not
enum {ELITIST, NONELITIST};
const static int sort_strategy = ELITIST;
// pmx or crossover
enum {TWOPOINT, PMX, CX};
const static int breed_strategy = CX;

// now passed in in makefile
// number of iterations
// if k opt is 3, it will converge quicker, so doesnt need to run as long
//const static unsigned int NUM_ITER = 100;
// number of random runs to do - MAX 512
//const static unsigned int GLOBAL_SIZE = 512;
// size of workgroup
//const static unsigned int GROUP_SIZE = 16;

// number of trucks in route
// 7 seems optimal
const static unsigned int NUM_TRUCKS = 7;
// max number of stops per route
const static unsigned int STOPS_PER_ROUTE = 14;
// min capacity before route making will give up
const static int MIN_CAPACITY = 5;
// % chance of taking a pair when making initial routes
// too high and GA does nothing, too low and GA doesnt converge
// 95 seems good - not deterministic, but produces good initial results
const static int RAND_THRESHOLD = 80;

// returns number of nodes not yet in the total route
int OCLLearn::nodesLeft
(route_vec_t const& total_route, route_vec_t& trace_diff_result)
{
    // measuring how many nodes are left to add to route
    route_vec_t trace_count;
    unsigned int kk = 0;
    unsigned int jj = 0;

    // iterators for the various things
    route_vec_t::iterator tc_it;
    route_vec_t::iterator tr_it;
    route_vec_t::iterator td_it;
    route_vec_t::iterator last_elem;

    // sort and make unique, then remove any duplicates
    trace_count = total_route;
    std::sort(trace_count.begin(), trace_count.end());
    last_elem = std::unique(trace_count.begin(), trace_count.end());
    if(last_elem != trace_count.end())
    {
        for(jj=trace_count.end() - last_elem; jj; --jj)
        {
            trace_count.pop_back();
        }

        // remove the last element which will not be unique (???)
        trace_count.pop_back();
    }

    // get all elements that have not yet been added to route
    std::set_difference(
        all_stops.begin(), all_stops.end(),
        trace_count.begin(), trace_count.end(),
        trace_diff_result.begin());

    // the number of nodes which have not yet been visited
    kk = node_demands.size()-trace_count.size();

    // if only '0' is left, then all ones have been used up
    //if(trace_diff_result.at(0) == 0)kk;

    return kk;
}

void OCLLearn::addNode
(route_vec_t& route, route_vec_t const& total_route,
int& current_capacity, const unsigned int ii,
unsigned int pair_first, unsigned int pair_second)
{
    unsigned int kk;
    unsigned int jj;

    if(route.size() > 1)
    {
        // if the first one of the pair is the same as the first in the
        // current route (After the depot) then add the second one
        // to the beginning if it wont overdo the capacity
        if((pair_first) == route.at(1)
        && std::find(route.begin(), route.end(), (pair_second)) == route.end())
        {
            if((node_demands.at(pair_second).second) > current_capacity)
            {
                //route.push_back(0);
            }
            else
            {
                // create new route, add the new node to the begin
                // of the route, then add the rest on again
                route.insert(route.begin()+1, (pair_second));
                current_capacity -= (node_demands.at(pair_second)).second;
            }
        }
        // if the second in the pair is at the beginning of the route
        else if((pair_second) == route.at(1)
        && std::find(route.begin(), route.end(), (pair_first)) == route.end())
        {
            if((node_demands.at(pair_first)).second > current_capacity)
            {
                //route.push_back(0);
            }
            else
            {
                // do the same, but add the FIRST of the pair this time
                route.insert(route.begin()+1, (pair_first));
                current_capacity -= (node_demands.at(pair_first)).second;
            }
        }
        // if the first int he pair is at the end of the route
        else if((pair_first) == route.at(route.size()-1)
        && std::find(route.begin(), route.end(), (pair_second)) == route.end())
        {
            if((node_demands.at(pair_second)).second > current_capacity)
            {
                //route.push_back(0);
            }
            else
            {
                // add it to the end of the current route
                route.push_back((pair_second));
                current_capacity -= (node_demands.at(pair_second)).second;
            }
        }
        // if the second in the pair is at the end of the route
        else if((pair_second) == route.at(route.size()-1)
        && std::find(route.begin(), route.end(), (pair_first)) == route.end())
        {
            if((node_demands.at(pair_first).second) > current_capacity)
            {
                //route.push_back(0);
            }
            else
            {
                // add it to the end of the current route
                route.push_back((pair_first));
                current_capacity -= (node_demands.at(pair_first)).second;
            }
        }
    }
    else
    {
        // new route - start at beginning and find earliest pair that isn't in route already
        jj = 0;
        for(kk=jj;kk<CWS_pair_list.size();kk++)
        {
            pair_first = CWS_pair_list.at(kk).first_index;
            pair_second = CWS_pair_list.at(kk).second_index;

            // if the pair at the index kk isn't already in the route
            if(std::find(route.begin(), route.end(), pair_first) == route.end()
            && std::find(route.begin(), route.end(), pair_second) == route.end()
            && std::find(total_route.begin(), total_route.end(), pair_first) == total_route.end()
            && std::find(total_route.begin(), total_route.end(), pair_second) == total_route.end())
            {
                route.push_back(pair_first);
                current_capacity -= (node_demands.at(pair_first)).second;
                route.push_back(pair_second);
                current_capacity -= (node_demands.at(pair_second)).second;
                break;
            }
        }
    }
}

// returns vector of valid routes
void OCLLearn::genChromosomes
(std::vector<route_vec_t>& all_routes)
{
    unsigned int kk = 0;
    unsigned int jj = 0;
    unsigned int ii = 0;

    int current_capacity;

    // number of vehicles
    unsigned int oo = 0;

    // temp to test result of tree making
    route_vec_t route;
    route_vec_t total_route;

    // initialise to be all stops - ie, none in route yet
    route_vec_t trace_diff_result;

    // random numbers that decide whether to use a connection or not
    unsigned int rand_chance;
    unsigned int loop_rand = 0;

    // values of pairs
    unsigned int pair_first, pair_second;

    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "Generating " << GLOBAL_SIZE << " random routes ... 0" << std::flush;
    #endif

    // while we still dont have enough chromosomes
    while(all_routes.size() < GLOBAL_SIZE)
    {
        // reset total route
        total_route.clear();
        //total_route.push_back(0);

        oo=0;
        trace_diff_result = all_stops;

        // route to take
        current_capacity = capacity;

        rand_chance = (random() % RAND_THRESHOLD);

        // create a new vehicles every time capacity has been reached
        do{
            // increase nmumber of vehicles used
            oo++;

            // reset counters
            current_capacity = capacity;

            // reset inner route
            route.clear();
            //route.push_back(0);

            // start at a random node
            unsigned int rand_start;
            do
            {
                rand_start = (rand() % (node_coords.size()-1))+1;
            }
            // that isn't already in the total route
            while(std::find(total_route.begin(), total_route.end(),
            rand_start) != total_route.end()
            && rand_start);

            pair_first = (rand_start);
            // find the first pair
            for(ii = 0; ii < CWS_pair_list.size(); ii++)
            {
                if(CWS_pair_list.at(ii).first_index == pair_second
                && std::find(total_route.begin(), total_route.end(),
                CWS_pair_list.at(ii).first_index) != total_route.end())
                {
                    pair_first = CWS_pair_list.at(ii).second_index;
                    break;
                }
                else if(CWS_pair_list.at(ii).second_index == pair_first
                && std::find(total_route.begin(), total_route.end(),
                CWS_pair_list.at(ii).second_index) != total_route.end())
                {
                    pair_second = CWS_pair_list.at(ii).first_index;
                    break;
                }
            }

            ii = 0;

            // while the vehicle still has space
            while (current_capacity > MIN_CAPACITY
            // and we haven't run out of nodes
            && ++ii < CWS_pair_list.size() - 1
            // and the route isn't over the set limit
            && route.size() < STOPS_PER_ROUTE)
            {
                loop_rand = random() % 100;

                // random chance to take it, and make sure it isn't already in the route
                if(loop_rand < rand_chance
                && std::find(total_route.begin(), total_route.end(),
                (pair_second)) == total_route.end()
                && std::find(total_route.begin(), total_route.end(),
                (pair_first)) == total_route.end())
                {
                        addNode (route, total_route,
                        current_capacity, ii,
                        pair_first, pair_second);
                }

                pair_second = (CWS_pair_list.at(ii+1).second_index);
                pair_first = (CWS_pair_list.at(ii+1).first_index);
            }

            // push route onto the back of total route and remove any duplicate 1s that were on the end of it
            for(jj=0, kk=route.size();jj<kk;jj++)
            {
                total_route.push_back(route.back());
                route.pop_back();
            }
            for(jj=total_route.end() - std::unique(total_route.begin(),
            total_route.end());jj;jj--)
            {
                total_route.pop_back();
            }

            // number of nodes left to add to route
            kk = nodesLeft(total_route, trace_diff_result)-1;

        // if >10 vehicles used, its not a good route
        // if ii > number of pairs, all have been exhausted, return
        // if kk == 0, all nodes have been visited
        }while(kk && oo<=NUM_TRUCKS);

        // all nodes in the current total route
        if(!kk && oo == NUM_TRUCKS
        && std::find(all_routes.begin(), all_routes.end(),
        total_route) == all_routes.end())
        {
            all_routes.push_back(total_route);
            #ifdef VERBOSE
            std::cout << "\rGenerating " << GLOBAL_SIZE << " random routes ... " << all_routes.size() <<std::flush;
            #endif
        }
    }
}

// creates a program and checks it works
cl::Program OCLLearn::createProg
(std::string const& filename, std::string const& options)
{
    cl::Program program;

    std::ifstream source_file(filename.c_str());

    std::basic_string<char> source_code(std::istreambuf_iterator<char>(source_file),
        (std::istreambuf_iterator<char>()) );

    cl::Program::Sources source(1, std::make_pair(source_code.c_str(), source_code.length()+1));
    program = cl::Program(context, source);

    std::stringstream errstream;
    std::vector<cl::Device>::iterator dev_it;

    try
    {
        program.build( devices, options.c_str() );
        for(dev_it = devices.begin(); dev_it < devices.end(); dev_it++)
        {
            errstream << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*dev_it);
        }
        std::string errs( errstream.str() );
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
    std::cout << (DT==CL_DEVICE_TYPE_CPU ? "CPU" : "GPU") << ": ";
    std::cout << std::endl;
    #endif

    for(ii=0;ii<platforms.size();ii++)
    {
        try
        {
            platforms.at(ii).getInfo(CL_PLATFORM_NAME, &plat_info);
            #ifdef VERBOSE
            std::cout << plat_info << std::endl;
            #endif

            cl_context_properties properties[3] = {CL_CONTEXT_PLATFORM,
                (cl_context_properties)(platforms.at(ii))(), 0};

            context = cl::Context(DT, properties);
            devices = context.getInfo<CL_CONTEXT_DEVICES>();

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

    if(devices.size() > 1)
    {
        queue = cl::CommandQueue(context, devices.at(1));
    }
    else
    {
        queue = cl::CommandQueue(context, devices.at(0));
    }

    /****   options   *******/

    std::stringstream options;
    options << "-cl-fast-relaxed-math ";
    options << "-cl-strict-aliasing ";
    options << "-cl-mad-enable ";
    options << "-cl-no-signed-zeros ";
    //options << "-cl-opt-disable ";

    // disable testing values
    options << "-DNOTEST ";

    // extra options
    options << "-DNUM_PAIRS=" << CWS_pair_list.size() << " ";
    options << "-DNUM_COORDS=" << node_coords.size() << " ";
    options << "-DNUM_TRUCKS=" << NUM_TRUCKS * 2 << " ";
    options << "-DMAX_PER_ROUTE=" << STOPS_PER_ROUTE << " ";
    options << "-DCAPACITY=" << capacity << " ";
    options << "-DGROUP_SIZE=" << GROUP_SIZE << " ";
    options << "-DGLOBAL_SIZE=" << GLOBAL_SIZE << " ";
    options << "-DROUTE_STOPS=" << all_chrom_size / (GLOBAL_SIZE * sizeof(int)) << " ";
    options << "-DK_OPT=" << tsp_strategy << " ";

    // mutation rate
    options << "-DMUT_RATE=" << mutrate << " ";

    // which mutation to use
    if (mutate_strategy == REVERSE)
    {
        #ifdef VERBOSE
        std::cout << "Using random range reverse for mutation" << std::endl;
        #endif
        options << "-DMUT_REVERSE ";
    }
    else if (mutate_strategy == SWAP)
    {
        #ifdef VERBOSE
        std::cout << "Using random range swap for mutation" << std::endl;
        #endif
        options << "-DMUT_SWAP ";
    }

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

    // holds arrays of length NUM_TRUCKS corresponding to where sub routes begin and and in each route
    cl_buf_t route_starts_buf(context,
        CL_MEM_READ_WRITE,
        // max 2 times original length
        NUM_TRUCKS * 2 * GLOBAL_SIZE * sizeof(int));
    buffers["starts"] = route_starts_buf;

    // node coordinates
    cl_buf_t node_coord_buf(context,
        CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
        sizeof(point_t) * node_coords.size(),
        &node_coords.at(0));
    queue.enqueueWriteBuffer(node_coord_buf, CL_TRUE, 0,
        sizeof(point_t) * node_coords.size(),
        &node_coords.at(0));
    buffers["coords"] = node_coord_buf;

    // demands for nodes
    cl_buf_t node_demands_buf(context,
        CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
        sizeof(point_t) * node_demands.size(),
        &node_demands.at(0));
    queue.enqueueWriteBuffer(node_demands_buf, CL_TRUE, 0,
        sizeof(point_t) * node_demands.size(),
        &node_demands.at(0));
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
        rand_vec.push_back((cl_uint2) {{rand(), rand()}} );
    }
    queue.enqueueWriteBuffer(rand_state_buf, CL_TRUE, 0,
        GLOBAL_SIZE * sizeof(cl_uint2),
        &rand_vec.at(0));
    buffers["rand_state"] = rand_state_buf;

    // test rand num
    cl_buf_t rand_out_buf(context,
        CL_MEM_READ_WRITE,
        all_chrom_size * 2);
    buffers["rand_out"] = rand_out_buf;

    /****   kernels   *******/

    // fitness kernel
    try
    {
        fitness_kernel = cl_knl_t(all_program, "fitness");

        fitness_kernel.setArg(0, buffers.at("parents"));
        fitness_kernel.setArg(1, buffers.at("results"));
        fitness_kernel.setArg(2, buffers.at("coords"));
        fitness_kernel.setArg(3, buffers.at("demands"));
        fitness_kernel.setArg(4, buffers.at("starts"));
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
        if(tsp_strategy == DJ)
        {
            TSP_kernel = cl_knl_t(all_program, "djikstra");
        }
        else if(tsp_strategy == SIMPLE)
        {
            TSP_kernel = cl_knl_t(all_program, "simpleTSP");
        }
        else if(tsp_strategy == NONE)
        {
            TSP_kernel = cl_knl_t(all_program, "noneTSP");
        }
        else
        {
            TSP_kernel = cl_knl_t(all_program, "kOpt");
        }

        TSP_kernel.setArg(0, buffers.at("parents"));
        TSP_kernel.setArg(1, buffers.at("starts"));
        TSP_kernel.setArg(2, buffers.at("coords"));
        TSP_kernel.setArg(3, buffers.at("demands"));
        TSP_kernel.setArg(4, buffers.at("rand_state"));
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
        ne_sort_kernel.setArg(1, buffers.at("parents"));
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
        if(breed_strategy == PMX)
        {
            crossover_kernel = cl_knl_t(all_program, "pmx");
            #ifdef VERBOSE
            std::cout << "Using pmx crossover" << std::endl;
            #endif
        }
        else if(breed_strategy == TWOPOINT)
        {
            crossover_kernel = cl_knl_t(all_program, "twoPointCrossover");
            #ifdef VERBOSE
            std::cout << "Using two point crossover" << std::endl;
            #endif
        }
        else if(breed_strategy == CX)
        {
            crossover_kernel = cl_knl_t(all_program, "cx");
            #ifdef VERBOSE
            std::cout << "Using cx crossover" << std::endl;
            #endif
        }

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
    local = cl::NDRange(GROUP_SIZE);
}

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

    t_0 = omp_get_wtime();

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

    ii = 0;

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
        if(e.err() == -55)
        {
            local = cl::NDRange(64);
            doub_local = cl::NDRange(128);
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

    double s0, s1;

    for(ii = 1; ii < NUM_ITER + 1; ii++)
    {
        #ifdef VERBOSE
        std::cout << "\rNow on iteration " << ii << "/";
        std::cout << NUM_ITER << " " << std::flush;
        #endif
        /*
        *   algo
        *   1.  do TSP on routes
        *   2.  sort by fitness
        *   3.  breed
        *   4.  TSP
        *   5.  sort by fitness
        *   6.  copy best 1000 back into parents
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
        lb = (rand() % (node_coords.size() - 3)) + 1;
        ub = (rand() % (node_coords.size() - lb - 2)) + lb + 1;
        // make sure they are sane numbers
        if(lb == ub 
        ||lb < 1 || lb > node_coords.size()-2
        ||ub < 1 || ub > node_coords.size()-2)
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
    t_1 = omp_get_wtime();
    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "took " << t_1-t_0 << " secs" << std::endl;
    #endif

    return std::pair<float, route_vec_t>(best_route, best_chromosome);
}

OCLLearn::OCLLearn
(RunInfo const& run_info, int dt)
: node_coords(run_info.node_coords),
node_demands(run_info.node_demands),
CWS_pair_list(run_info.CWS_pair_list),
capacity(run_info.capacity),
DT(dt),
all_chrom_size((node_coords.size() - 1) * GLOBAL_SIZE * sizeof(int))
{
    srand(time(NULL));

    initOCL();

    all_stops = route_vec_t(node_coords.size(), 0);

    unsigned int ii;
    for (ii = 0; ii < node_coords.size(); ii++)
    {
        all_stops.at(ii) = ii;
    }
}

