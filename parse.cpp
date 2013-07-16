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

/*
 *  Initial parameters
 *
 *  These are what I found to be the best based on simple testing
 */
tsp_e tsp_strategy = SIMPLE;
mut_e mutate_strategy = REVERSE;
sort_e sort_strategy = NONELITIST;
breed_e breed_strategy = PMX;

int MUTRATE = 25;
unsigned int NUM_SUBROUTES = 7;
unsigned int STOPS_PER_ROUTE = 14;
unsigned int MIN_CAPACITY = 5;
unsigned int RAND_THRESHOLD = 95;
unsigned int ARENA_SIZE = 8;
int DEVICE_TYPE = CL_DEVICE_TYPE_CPU;
size_t GLOBAL_SIZE = 2048;
size_t LOCAL_SIZE = 32;

// other params
std::string INPUT_FILE("fruitybun-data.vrp");
int VERBOSE_OUTPUT = 0;

// say max 10 minutes or 20k generations default
size_t GENERATIONS = 20000;
double MAX_TIME = 360;

void parseArgs
(int argc, char* argv[])
{
    // option specifiers for getopt_long
    static struct option long_options[] =
    {
        {"arena_size", required_argument, 0, 'a'},
        {"breed", required_argument, 0, 'b'},
        {"min_capacity", required_argument, 0, 'c'},
        {"type", required_argument, 0, 'd'},
        {"select_strategy", required_argument, 0, 'e'},
        {"input_file", required_argument, 0, 'f'},
        {"per_generation", required_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {"iterations", required_argument, 0, 'i'},
        {"mutstrat", required_argument, 0, 'm'},
        {"num_trucks", required_argument, 0, 'n'},
        {"per_population", required_argument, 0, 'p'},
        {"mutrate", required_argument, 0, 'r'},
        {"stops_per_route", required_argument, 0, 's'},
        {"time", required_argument, 0, 't'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    // help strings corresponding to each option - keep in same order!
    static const char* help_strings[] = 
    {
        "Arena size - size of arena to do arena selection. Setting to 0 means parents are chosen at random",
        "Breed strategy - either CX, PMX, or O1",
        "Capacity - how much capacity left in each truck will cause route creation to go back to depot",
        "Device type - CPU, GPU, ACCELERATOR",
        "Selection strategy - whether to keep the best of parents and children (YESELITIST) or just the children (NONELITIST)",
        "Input file - which file to read input from",
        "Total size - how many chromosomes in total in all populations",
        "Print help",
        "Iterations - number of generations to go through",
        "Mutation strategy - what kind of mutation strategy to use. Either SWAP, SLIDE, or REVERSE",
        "Number of trucks - number of subroutes in a total route to create routes for",
        "Population size - size per population in the total number of populations",
        "Mutation rate - percentage chance of a chromosome being mutated",
        "Stops per route - number of stops before route creation will go back to depot",
        "Time to run - max time to run, stopping early even if specified number of generations has not passed",
        "Verbose mode"
    };

    int option_index, c;
    long int converted;
    int ii = 0;
    // buffer for parsing string arguments
    std::string read_arg;

    #define CONV_CHECK(arg_name, min)                       \
        if (min > (converted = strtol(optarg, NULL, 10)))   \
        {                                                   \
            fprintf(stderr,                                 \
                    "Invalid argument %ld passed to %s\n",  \
                    converted, #arg_name);                  \
            exit(1);                                        \
        }

    while(1)
    {
        if (-1 == (c = getopt_long(argc, argv, "a:b:c:d:e:f:g:hi:m:n:p:r:s:t:v",
                                   long_options, &option_index)))
        {
            break;
        }

        switch (c)
        {
        case 0:
            if (0 != long_options[option_index].flag)
            {
                fprintf(stdout, "Got %s\n", long_options[option_index].name);
            }
            break;

        case 'a':
            CONV_CHECK(-a, 0);
            ARENA_SIZE = converted;
            break;

        case 'b':
            read_arg = std::string(optarg);
            std::transform(read_arg.begin(),
                           read_arg.end(),
                           read_arg.begin(),
                           toupper);
            if (std::string::npos != read_arg.find("CX"))
            {
                breed_strategy = CX;
            }
            else if (std::string::npos != read_arg.find("O1"))
            {
                breed_strategy = O1;
            }
            else if (std::string::npos != read_arg.find("PMX"))
            {
                breed_strategy = PMX;
            }
            else
            {
                fprintf(stderr,
                        "Unknown breed strategy '%s' passed to -b\n",
                        optarg);
                fprintf(stderr,
                        "Valid options are CX and PMX\n");
                exit(1);
            }
            break;


        case 'c':
            CONV_CHECK(-c, 0);
            MIN_CAPACITY = converted;
            break;

        case 'e':
            // make argument all uppercase
            read_arg = std::string(optarg);
            std::transform(read_arg.begin(),
                           read_arg.end(),
                           read_arg.begin(),
                           toupper);
            // TODO change to a more sensible name
            if (std::string::npos != read_arg.find("YESELITIST"))
            {
                sort_strategy = ELITIST;
            }
            else if (std::string::npos != read_arg.find("NONELITIST"))
            {
                sort_strategy = NONELITIST;
            }
            else
            {
                fprintf(stderr,
                        "Unknown selection strategy '%s' passed to -e\n",
                        optarg);
                exit(1);
            }
            break;

        case 'f':
            read_arg = std::string(optarg);
            INPUT_FILE = std::string(optarg);
            break;

        case 'g':
            CONV_CHECK(-g, 2);
            GLOBAL_SIZE = converted;
            break;

        // h is further down

        case 'i':
            CONV_CHECK(-i, 0);
            GENERATIONS = converted;
            break;

        case 'n':
            CONV_CHECK(-c, 1);
            NUM_SUBROUTES = converted;
            break;

        case 'm':
            // make argument all uppercase
            read_arg = std::string(optarg);
            std::transform(read_arg.begin(),
                           read_arg.end(),
                           read_arg.begin(),
                           toupper);
            if (std::string::npos != read_arg.find("REVERSE"))
            {
                mutate_strategy = REVERSE;
            }
            else if (std::string::npos != read_arg.find("SLIDE"))
            {
                mutate_strategy = SLIDE;
            }
            else if (std::string::npos != read_arg.find("SWAP"))
            {
                mutate_strategy = SWAP;
            }
            else
            {
                fprintf(stderr,
                        "Unknown mutation strategy '%s' passed to -m\n",
                        optarg);
                fprintf(stderr,
                        "Valid options are REVERSE and SWAP\n");
                exit(1);
            }
            break;

        case 'd':
            read_arg = std::string(optarg);
            std::transform(read_arg.begin(),
                           read_arg.end(),
                           read_arg.begin(),
                           toupper);
            if (std::string::npos != read_arg.find("CPU"))
            {
                DEVICE_TYPE=CL_DEVICE_TYPE_CPU;
            }
            else if (std::string::npos != read_arg.find("GPU"))
            {
                DEVICE_TYPE=CL_DEVICE_TYPE_GPU;
            }
            else if (std::string::npos != read_arg.find("ACCELERATOR"))
            {
                DEVICE_TYPE=CL_DEVICE_TYPE_ACCELERATOR;
            }
            else
            {
                fprintf(stderr,
                        "Unknown device type '%s' passed to -t\n",
                        optarg);
                fprintf(stderr,
                        "Valid options are CPU, GPU, and ACCELERATOR\n");
                exit(1);
            }
            break;

        case 'p':
            CONV_CHECK(-p, 2);
            LOCAL_SIZE = converted;
            break;

        case 'r':
            CONV_CHECK(-r, 0);
            MUTRATE = converted;
            break;

        case 's':
            CONV_CHECK(-s, 1);
            STOPS_PER_ROUTE = converted;
            break;

        // print help
        case 'h':
            fprintf(stdout, "Options:\n");
            do
            {
                if (required_argument == long_options[ii].has_arg)
                {
                    fprintf(stdout,
                            "  --%s=<...> / -%c <...>\n   ",
                            long_options[ii].name,
                            long_options[ii].val);
                }
                else
                {
                    fprintf(stdout,
                            "  --%s / -%c\n   ",
                            long_options[ii].name,
                            long_options[ii].val);
                }
                fprintf(stdout, "%s\n", help_strings[ii]);
            }
            while (long_options[++ii].name);
            exit(0);
            break;

        case 't':
            CONV_CHECK(-t, 1);
            MAX_TIME = converted;
            break;

        case 'v':
            VERBOSE_OUTPUT = 1;
            break;

        case '?':
            fprintf(stderr, "exiting.\n");
            exit(1);
            break;

        default:
            fprintf(stderr, "Unrecognised argument\n");
            exit(1);
        }
    }
}

// get data from file
void RunInfo::parseInput
(void)
{
    std::string file_line;
    std::string sub;
    size_t str_return;
    size_t substr_pos;
    int x, y;
    int depot, demand;

    std::ifstream file_stream(INPUT_FILE.c_str(), std::ifstream::in);

    if (!file_stream.is_open())
    {
        std::cout << "Could not open input file ";
        std::cout << "\"" << INPUT_FILE << "\"" << std::endl;
        exit(1);
    }

    // skip past the edge weight, dimension, etc
    do
    {
        std::getline(file_stream, file_line);
        str_return = file_line.find("CAPACITY");
    }
    while (str_return == std::string::npos && file_stream.good());

    // Extract capacity
    substr_pos = file_line.find(" : ");
    sub = file_line.substr(substr_pos + 3);
    capacity = std::atoi(sub.c_str());

    // read "NODE_COORD_SECTION"
    std::getline(file_stream, file_line);

    // read in node coordinates
    while (std::getline(file_stream, file_line), file_stream.good())
    {
        //std::getline(file_stream, file_line);
        std::istringstream stream(file_line);
        if (file_line.find("DEMAND") != std::string::npos)
        {
            break;
        }

        stream >> sub;
        depot = atoi(sub.c_str());
        stream >> sub;
        x = atoi(sub.c_str());
        stream >> sub;
        y = atoi(sub.c_str());
        point_t coord = {{x, y}};

        node_coords[depot] = coord;
    }

    // read in demand for each node
    while (std::getline(file_stream, file_line), file_stream.good())
    {
        //std::getline(file_stream, file_line);
        std::istringstream stream(file_line);
        if (file_line.find("DEPOT") != std::string::npos)
        {
            break;
        }

        stream >> sub;
        depot = atoi(sub.c_str());
        stream >> sub;
        demand = atoi(sub.c_str());

        // depot node has 0 demand
        if (!demand)
        {
            depot_node = depot;
        }

        node_demands[depot] = demand;
    }
}

