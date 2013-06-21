#include "omp.h"
#include "common_header.hpp"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <limits>
#include <vector>

// number of runs to do to probe the average results of each test
const static unsigned int num_tests = 80;
// number of threads to use
const static unsigned int num_threads = 8;
// number of runs to find a route
const static unsigned int num_runs = 25;

class CVPR
{
 
private:

    //0, 1, 2, 3, 4, ...., 75
    route_vec_t all_stops;
    route_vec_t best_route;

    float best_length;

    //paramaters
    unsigned int capacity;
    point_vec_t node_coords;
    point_vec_t node_demands;
    point_info_vec_t CWS_pair_list;

    // find length of route
    float routeLength(route_vec_t& total_route);

    // returns number of nodes not yet in the total route
    int nodesLeft
    (route_vec_t const& total_route, route_vec_t& trace_diff_result);

    // find the next good node to add
    void addNode
    (route_vec_t& route, route_vec_t const& total_route,
    int& current_capacity, const unsigned int ii,
    unsigned int pair_first, unsigned int pair_second);

    // recurses to parse tree, when it reaches the end then it returns the 
    // lengh of the entire route
    float traceRoute
    (route_vec_t const& cur_route, route_vec_t const& cur_total_route,
    unsigned int ii, int current_capacity);

    // top level function to create the tree
    void parseTree
    (void);

public:

    CVPR
    (RunInfo const& run_info);

    // run the learning stage
    void run
    (void);

};

float CVPR::routeLength
(route_vec_t& total_route)
{
    float total = 0;
    int ii;
    for(ii = 0; ii < static_cast<int>(total_route.size())-1; ii++)
    {
        total += euclideanDistance(node_demands.at(total_route.at(ii)), node_demands.at(total_route.at(ii+1)));
    }

    return total;
}

// returns number of nodes not yet in the total route
int CVPR::nodesLeft
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

    trace_count = total_route;
    std::sort(trace_count.begin(), trace_count.end());
    last_elem = std::unique(trace_count.begin(), trace_count.end());
    // remove stuff
    if(last_elem != trace_count.end())
    {
        for(jj=trace_count.end() - last_elem;jj;jj--)
        {
            trace_count.pop_back();
        }
    }
    // get all elements that have not yet been added to route
    std::set_difference(
        all_stops.begin(), all_stops.end(),
        trace_count.begin(), trace_count.end(),
        trace_diff_result.begin());

    // the number of nodes which have not yet been visited
    kk = node_demands.size()-trace_count.size();
    // if only '0' is left, then all ones have been used up
    if(trace_diff_result.at(0) == 0)kk--;

    return kk;
}

void CVPR::addNode
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
                route.push_back(0);
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
                route.push_back(0);
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
                route.push_back(0);
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
                route.push_back(0);
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

// recurses to parse tree, when it reaches the end then it returns the 
// lengh of the entire route
float CVPR::traceRoute
(route_vec_t const& cur_route, route_vec_t const& cur_total_route,
unsigned int ii, int current_capacity)
{
    unsigned int kk = 0;
    unsigned int jj = 0;

    // number of vehicles
    unsigned int oo = 0;

    // temp to test result of tree making
    route_vec_t route = cur_route;
    route_vec_t total_route = cur_total_route;

    // initialise to be all stops - ie, none in route yet
    route_vec_t trace_diff_result;
    trace_diff_result = all_stops;

    // random numbers that decide whether to use a connection or not
    int rand_chance = (random() % 50);
    int loop_rand = 0;

    // values of pairs
    unsigned int pair_first, pair_second;

    // route to take
    current_capacity = capacity;
    // always start at depot
    //route.insert(route.begin(), 0);
    //route.push_back(0);

    // create a new vehicles every time capacity has been reached
    do{
        // increase nmumber of vehicles used
        oo++;

        // reset counters
        ii=0;
        current_capacity = capacity;
        route.clear();

        // while the vehicle still has space
        while (current_capacity > 0 && ++ii < CWS_pair_list.size())
        {
            loop_rand = random() % 100;

            pair_first = (CWS_pair_list.at(ii).first_index);
            pair_second = (CWS_pair_list.at(ii).second_index);

            // random chance to take it, and make sure it isn't already in the route
            if(loop_rand % 100 > rand_chance
            && std::find(total_route.begin(), total_route.end(),
            (pair_second)) == total_route.end()
            && std::find(total_route.begin(), total_route.end(),
            (pair_first)) == total_route.end())
            {
                addNode (route, total_route,
                current_capacity, ii,
                pair_first, pair_second);
            }
        }

        // push route onto the back of total route and remove any duplicate 1s that were on the end of it
        for(jj=0, kk=route.size();jj<kk;jj++)
        {
            total_route.push_back(route.back());
        }
        for(jj=total_route.end() - std::unique(total_route.begin(), total_route.end());jj;jj--)
        {
            total_route.pop_back();
        }

        // number of nodes left to add to route
        kk = nodesLeft(total_route, trace_diff_result);

    // if >10 vehicles used, its not a good route
    // if ii > number of pairs, all have been exhausted, return
    // if kk == 0, all nodes have been visited
    }while(ii < CWS_pair_list.size() && kk && oo<=10);

    if(oo > 10)
    {
        // not a good route
        return 3000;
    }
    else
    {
        // length of route
        return routeLength(total_route);
    }
}

void CVPR::parseTree
(void)
{
    unsigned int kk = 0;
    unsigned int jj = 0;
    unsigned int ii = 0;

    // counter for how many times it iwll try to find a route
    unsigned int oo = 0;

    // track how much each branch gets on average
    float taken_count = 0;
    float not_taken_count = 0;

    // capacity of truck on route
    int current_capacity = capacity;

    // used for making route
    route_vec_t route;
    route_vec_t taken_test_route;
    route_vec_t total_route;

    // initialise to be all stops - ie, none in route yet
    route_vec_t trace_diff_result;
    trace_diff_result = all_stops;

    // make code cleaner
    unsigned int pair_second;
    unsigned int pair_first;

    /*********************/

    // make a new route until all nodes have been filled
    do{
        // clear current real route and add 1 to it to siginify it starts at depot
        route.clear();
        route.push_back(0);

        // increment counter
        ++oo;

        // reset counters + capacity for truck
        current_capacity = capacity;
        ii=0;

        // find the next thing to go on the end of the route
        do{
            // first and second indexes of the pair
            pair_first = CWS_pair_list.at(ii).first_index;
            pair_second = CWS_pair_list.at(ii).second_index;

            if(
            // if the route has at least one stop and the current pair to look at isn't already in the route
            (((route.size()>1
            && (std::find(route.begin(), route.end(), pair_first) != route.end()
            || std::find(route.begin(), route.end(), pair_second) != route.end()))
            // or it's just the initial depot stop
            || (route.size() < 2))
            // and it's not in the total route
            && std::find(total_route.begin(), total_route.end(), pair_first) == total_route.end()
            && std::find(total_route.begin(), total_route.end(), pair_second) == total_route.end())
            )
            {
                // reset route length counters
                taken_count = 0;
                not_taken_count = 0;

                // run num_tests runs takin the pair or not taking the pair, then sum lenghts of all routes generated to guess whether it would be good to take the route not not
    #pragma omp parallel for reduction(+:taken_count)
                for(kk=0;kk<num_tests;kk++)
                {
                    taken_count += traceRoute(route, total_route, ii, current_capacity);
                }
    #pragma omp parallel for reduction(+:not_taken_count)
                for(kk=0;kk<num_tests;kk++)
                {
                    not_taken_count += traceRoute(route, total_route, ii+1, current_capacity);
                }
                if(taken_count < not_taken_count)
                {
                    addNode (route, total_route,
                    current_capacity, ii,
                    pair_first, pair_second);
                }
            }
        }while(current_capacity > 0 && ++ii < CWS_pair_list.size());

        // if route is not just depot, add
        if(route.size()>1)
        {
            for(kk=0;kk<route.size();kk++){total_route.push_back(route.at(kk));}
            for(jj=total_route.end() - std::unique(total_route.begin(), total_route.end());jj;jj--)
            {
                total_route.pop_back();
            }
        }

        jj = nodesLeft(route, trace_diff_result);

        std::cout << oo <<std::endl;

    }while(jj && oo<=10);

    total_route.push_back(0);

    float cur_length =  routeLength(total_route);

    if(cur_length < best_length)
    {
        // check if route is valid - should not be needed?
        bool route_valid = true;
        for (jj=0;jj<all_stops.size();jj++)
        {
            if (std::find(total_route.begin(), total_route.end(), all_stops.at(jj)) == total_route.end())
            {
                route_valid = false;
                break;
                std::cout << all_stops.at(jj) << " NOT FOUND IN ROUTE!" << std::endl;
                std::cout << std::endl;
            }
        }
        if (route_valid)
        {
            best_length = cur_length;
            std::cout << "new best length is " << best_length << std::endl;
            for(jj=0;jj<total_route.size();jj++){std::cout << total_route.at(jj) << " ";}
            #pragma omp critical
            {
                best_route = total_route;
            }
            std::cout << std::endl;
        }
    }
}

CVPR::CVPR
(RunInfo const& run_info)
: node_coords(run_info.node_coords),
node_demands(run_info.node_demands),
CWS_pair_list(run_info.CWS_pair_list),
capacity(run_info.capacity)
{
    all_stops = route_vec_t(node_coords.size(), 0);
    unsigned int ii;
    for (ii=0;ii<node_coords.size();ii++){all_stops.at(ii) = ii;}

    best_length = std::numeric_limits<float>::max();
}

void CVPR::run
(void)
{
    double t_0, t_1;

    std::cout << num_runs << " runs" << std::endl;

    omp_set_num_threads(num_threads);
    std::cout << "starting with this many threads: " << num_threads << std::endl;

    t_0 = omp_get_wtime();

    unsigned int kk;
    //#pragma omp parallel for
    for(kk=0;kk<num_runs;kk++)
    {
        std::cout <<  kk <<std::endl;
        parseTree();
    }

    std::cout << std::endl;
    std::cout << "best route - length " << routeLength(best_route) << std::endl;
    route_vec_t::iterator best_it;
    for(best_it = best_route.begin(); best_it < best_route.end(); best_it++)
    {
        std::cout << *best_it << " ";
    }
    std::cout <<std::endl;

    t_1 = omp_get_wtime();
    std::cout << "took " << t_1-t_0 << " secs" << std::endl;
}

