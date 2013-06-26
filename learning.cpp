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

#define INPUT_FILE "fruitybun-data.vrp"

float euclideanDistance
(point_t first, point_t second)
{
    unsigned int x1, y1, x2, y2;

    x1 = first.first;
    y1 = first.second;
    x2 = second.first;
    y2 = second.second;

    return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}

// function for sorting points
bool cmp_distance
(point_info_t first, point_info_t second)
{
    return first.distance < second.distance;
}

// get data from file
void RunInfo::parseInput
(std::string const& settings_file)
{
    std::string file_line;
    std::string sub;
    size_t str_return;
    size_t substr_pos;
    int x, y;
    int depot, demand;

    std::ifstream file_stream(settings_file.c_str(), std::ifstream::in);

    if (!file_stream.is_open())
    {
        std::cout << "Could not open " << INPUT_FILE << std::endl;
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

    std::getline(file_stream, file_line);

    // read in node coordinates
    while (file_stream.good())
    {
        std::getline(file_stream, file_line);
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
        point_t coord = {x, y};

        node_coords[depot] = coord;
    }

#if 0
    // read in demand for each node
    while (file_line.find("DEPOT") == std::string::npos && file_stream.good())
    {
        std::getline(file_stream, file_line);
        std::istringstream stream(file_line);

        stream >> sub;
        depot = atoi(sub.c_str())-1;
        stream >> sub;
        demand = atoi(sub.c_str());
        point_t demand_pair(depot, demand);

        node_demands.push_back(demand_pair);
    }
    node_demands.pop_back();
#endif

    // read in demand for each node
    while (file_stream.good())
    {
        std::getline(file_stream, file_line);
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

// get the sorted list of all nodes
void RunInfo::genSortedCWS
(void)
{
    unsigned int jj;
    unsigned int ii;

    for (ii = depot_node; ii < node_coords.size(); ii++)
    {
        for (jj = ii+1; jj < node_coords.size(); jj++)
        {
            point_info_t new_info;
            new_info.first_index = ii+1;
            new_info.second_index = jj+1;
            new_info.distance = euclideanDistance(node_coords.at(ii),
                                                  node_coords.at(jj));
            CWS_pair_list.push_back(new_info);
        }
    }

    std::sort(CWS_pair_list.begin(), CWS_pair_list.end(), cmp_distance);
}

void printRoute
(const route_vec_t& route,
 const RunInfo& info)
{
    // track routes
    std::stringstream best_route;
    float best_distance = 10000.0f;

    // auto find best solution in range
    uint max_cap, max_stops;

    for (max_cap = MIN_CAPACITY; max_cap < info.capacity; max_cap++)
    for (max_stops = NUM_SUBROUTES; max_stops < STOPS_PER_ROUTE; max_stops++)
    {
        // initial counts
        unsigned int cur_capacity = 0;
        float total_distance = 0.0f;
        unsigned int stops_taken = 1;
        unsigned int ii, jj;
        std::stringstream cur_route;

        // initial jaunt from depot to first node
        total_distance += euclideanDistance(info.node_coords.at(1),
                                            info.node_coords.at(route.at(0)));
        cur_capacity += info.node_demands.at(route.at(1));

        cur_route << "1";
        // for all the remaining nodes, go through and add
        for (ii = 0, jj = 1; ii < route.size() - 1 && jj < route.size();ii++, jj++)
        {
            cur_capacity += info.node_demands.at(route.at(jj));

            // check to see if route has to go back to depot
            if (cur_capacity > max_cap || stops_taken > max_stops)
            {
                // add in a trip back to the depot and then to the next point
                total_distance +=
                    euclideanDistance(info.node_coords.at(route.at(ii)),
                                      info.node_coords.at(1));
                total_distance +=
                    euclideanDistance(info.node_coords.at(1),
                                      info.node_coords.at(route.at(jj)));

                // add to path
                cur_route << "->" << route.at(ii);
                cur_route << "->1";
                cur_route << std::endl;
                cur_route << "1";

                // reset stops taken and capacity
                stops_taken = 1;
                cur_capacity = info.node_demands.at(route.at(jj));
            }
            else
            {
                // add point to route
                total_distance +=
                    euclideanDistance(info.node_coords.at(route.at(ii)),
                                      info.node_coords.at(route.at(jj)));
                cur_route << "->" << route.at(ii);
            }
        }
        // add last trip from point to depot and finish route
        total_distance +=
            euclideanDistance(info.node_coords.at(route.back()),
                              info.node_coords.at(1));

        cur_route << "->1";
        cur_route << std::endl;

        // update best route if this one was better
        if (total_distance < best_distance)
        {
            best_distance = total_distance;
            best_route.str("");
            best_route.str(cur_route.str());
        }
    }

    std::cout << "cost " << best_distance << std::endl;
    std::cout << best_route.str();
}

#if 0
void printRoute
(const RunInfo& r, const alg_result_t& result)
{
    // track routes
    std::stringstream best_route;
    float best_distance = 10000.0f;

    // auto find best solution in range
    uint max_cap, max_stops;
    max_cap = 220;
    max_stops = 15;

    // see if its possible to find a better route
    for(max_cap = 100; max_cap <= 220; max_cap++) {
    for(max_stops = 7; max_stops <= 15; max_stops++) {

        // initial counts
        unsigned int cur_capacity = 0;
        float total_distance = 0.0f;
        unsigned int stops_taken = 1;
        unsigned int ii, jj;
        std::stringstream cur_route;

        total_distance += euclideanDistance(
            r.node_coords.at(0),
            r.node_coords.at(result.second.at(0)));
        cur_capacity = r.node_demands.at(result.second.at(0)).second;

        cur_route << "1";
        for (ii = 0, jj = 1;
        ii < result.second.size() - 1 && jj < result.second.size();
        ii++, jj++)
        {
            cur_capacity +=
                r.node_demands.at(result.second.at(jj)).second;
            if(cur_capacity > max_cap || stops_taken > max_stops)
            {
                stops_taken = 1;

                total_distance += euclideanDistance(
                    r.node_coords.at(result.second.at(ii)),
                    r.node_coords.at(0));
                total_distance += euclideanDistance(
                    r.node_coords.at(0),
                    r.node_coords.at(result.second.at(jj)));

                cur_capacity =
                    r.node_demands.at(result.second.at(jj)).second;

                cur_route << "->" << result.second.at(ii) + 1;
                cur_route << "->1";
                cur_route << std::endl;
                cur_route << "1";
            }
            else
            {
                total_distance += euclideanDistance(
                    r.node_coords.at(result.second.at(ii)),
                    r.node_coords.at(result.second.at(jj)));
                cur_route << "->" << result.second.at(ii) + 1;
            }
        }
        cur_route << "->1";
        cur_route << std::endl;

        total_distance += euclideanDistance(
            r.node_coords.at(result.second.at(r.node_coords.size()-2)),
            r.node_coords.at(0));

        if(total_distance < best_distance)
        {
            best_distance = total_distance;
            best_route.str("");
            best_route.str(cur_route.str());
        }
    }}

    std::cout << "cost " << result.first << std::endl;
    std::cout << best_route.str();
}
#endif

void parseArgs
(int argc, char* argv[])
{
    // option specifiers for getopt_long
    static struct option long_options[] =
    {
        {"arena_size", required_argument, 0, 'a'},
        {"min_capacity", required_argument, 0, 'c'},
        {"select_strategy", required_argument, 0, 'e'},
        {"per_generation", required_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {"iterations", required_argument, 0, 'i'},
        {"mutstrat", required_argument, 0, 'm'},
        {"num_trucks", required_argument, 0, 'n'},
        {"per_population", required_argument, 0, 'p'},
        {"mutrate", required_argument, 0, 'r'},
        {"stops_per_route", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    // help strings corresponding to each option - keep in same order!
    static const char* help_strings[] = 
    {
        "Arena size - size of arena to do arena selection. Setting to 0 means parents are chosen at random",
        "Capacity - how much capacity left in each truck will cause route creation to go back to depot",
        "Selection strategy - whether to keep the best of parents and children (ELITIST) or just the children (NONELITIST)",
        "Total size - how many chromosomes in total in all populations",
        "Print help",
        "Iterations - number of generations to go through",
        "Mutation strategy - what kind of mutation strategy to use. Either SWAP or REVERSE",
        "Number of trucks - number of subroutes in a total route to create routes for",
        "Population size - size per population in the total number of populations",
        "Mutation rate - percentage chance of a chromosome being mutated",
        "Stops per route - number of stops before route creation will go back to depot",
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
        if (-1 == (c = getopt_long(argc, argv, "a:c:e:g:hi:m:n:p:r:s:v",
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
            if (std::string::npos != read_arg.find("ELITIST"))
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
            else if (std::string::npos != read_arg.find("SWAP"))
            {
                mutate_strategy = SWAP;
            }
            else
            {
                fprintf(stderr,
                        "Unknown mutation strategy '%s' passed to -m\n",
                        optarg);
                exit(1);
            }
            break;

        case 'p':
            CONV_CHECK(-p, 2);
            LOCAL_SIZE = converted;
            break;

        case 'r':
            CONV_CHECK(-r, 0);
            mutrate = converted;
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

int main
(int argc, char* argv[])
{
    parseArgs(argc, argv);

    // TODO change input from command line
    RunInfo r;
    r.parseInput(INPUT_FILE);
    r.genSortedCWS();

    OCLLearn ocl_learner(r);

    alg_result_t result;

    result = ocl_learner.run();

    printRoute(result.second, r);

    return 0;
}

