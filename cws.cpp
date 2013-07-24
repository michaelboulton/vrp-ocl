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

// returns number of nodes not yet in the total route
int OCLLearn::nodesLeft
(route_vec_t const& total_route,
 route_vec_t& stops_remaining)
{
    // measuring how many nodes are left to add to route
    route_vec_t trace_count;
    unsigned int kk = 0;
    unsigned int jj = 0;

    // make a reference so I don't need .info in front of it
    const demand_vec_t& node_demands = info.node_demands;

    // iterators for the various things
    route_vec_t::iterator tc_it;
    route_vec_t::iterator tr_it;
    route_vec_t::iterator td_it;
    route_vec_t::iterator last_elem;

    // copy
    trace_count = total_route;
    // sort
    std::sort(trace_count.begin(),
              trace_count.end());
    // make unique
    last_elem = std::unique(trace_count.begin(),
                            trace_count.end());

    // remove duplicates
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
    std::set_difference(all_stops.begin(),
                        all_stops.end(),
                        trace_count.begin(),
                        trace_count.end(),
                        stops_remaining.begin());

    // the number of nodes which have not yet been visited
    kk = node_demands.size()-trace_count.size();
    // if all stops have been visited
    if (stops_remaining.at(0) == 0) kk--;

    return kk;
}

void OCLLearn::addNode
(route_vec_t& subroute,
 unsigned int& current_capacity,
 unsigned int pair_first,
 unsigned int pair_second)
{
    // make a reference so I don't need .info in front of it
    const demand_vec_t& node_demands = info.node_demands;

    // if the first one of the pair is the same as the first in the
    // current route (After the depot) then add the second one
    // to the beginning if it wont overdo the capacity
    if (pair_first == subroute.at(0)
    && std::find(subroute.begin(),
                 subroute.end(),
                 pair_second) == subroute.end())
    {
        if(node_demands.at(pair_second) < current_capacity)
        {
            // create new route, add the new node to the begin
            // of the route, then add the rest on again
            subroute.insert(subroute.begin(), pair_second);
            current_capacity -= (node_demands.at(pair_second));
        }
    }
    // if the second in the pair is at the beginning of the route
    else if (pair_second == subroute.at(0)
    && std::find(subroute.begin(),
                 subroute.end(),
                 pair_first) == subroute.end())
    {
        if(node_demands.at(pair_first) < current_capacity)
        {
            // do the same, but add the FIRST of the pair this time
            subroute.insert(subroute.begin(), pair_first);
            current_capacity -= (node_demands.at(pair_first));
        }
    }
    // if the first int he pair is at the end of the route
    else if (pair_first == subroute.back()
    && std::find(subroute.begin(),
                 subroute.end(),
                 pair_second) == subroute.end())
    {
        if(node_demands.at(pair_second) < current_capacity)
        {
            // add it to the end of the current subroute
            subroute.push_back(pair_second);
            current_capacity -= (node_demands.at(pair_second));
        }
    }
    // if the second in the pair is at the end of the subroute
    else if (pair_second == subroute.back()
    && std::find(subroute.begin(),
                 subroute.end(),
                 pair_first) == subroute.end())
    {
        if(node_demands.at(pair_first) < current_capacity)
        {
            // add it to the end of the current subroute
            subroute.push_back(pair_first);
            current_capacity -= (node_demands.at(pair_first));
        }
    }
}

// returns vector of valid routes
void OCLLearn::genChromosomes
(std::vector<route_vec_t>& all_routes)
{
    unsigned int ii, jj, kk;

    // random numbers that decide whether to use a connection or not
    unsigned int loop_rand = 0;

    // refs
    const point_info_vec_t& CWS_pair_list = info.CWS_pair_list;
    const node_map_t& node_coords = info.node_coords;
    const demand_vec_t& node_demands = info.node_demands;

    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "Generating " << GLOBAL_SIZE << " random routes ... 0" << std::flush;
    #endif

    // TODO this while loop is the bit that needs to be task parallelised
    // while we still dont have enough chromosomes
    while (all_routes.size() < GLOBAL_SIZE)
    {
        // initialise route to take
        route_vec_t total_route;
        route_vec_t remaining = all_stops;
        unsigned int cur_vehicles = 0;

        unsigned int current_capacity;

        // create a new vehicles every time capacity has been reached
        do
        {
            // chance of taking a route
            unsigned int rand_chance = RAND_THRESHOLD;
            kk = 1000;

            // initialise subroute to take
            route_vec_t subroute;
            current_capacity = info.capacity;

            // values of pairs
            unsigned int pair_first=100000, pair_second=100000;

            do
            {
                // randomise, but make sure its bigger than depot
                pair_first = (rand() % (node_coords.size() - 1)) + 2;
            }
            // that isn't already in the total route
            while (total_route.end() != std::find(total_route.begin(),
                                                  total_route.end(),
                                                  pair_first)
            // and isn't the depot
            || pair_first == info.depot_node);

            // go through all pairs, randomly choose one from near the top
            for (ii = 0; ii < CWS_pair_list.size(); ii++)
            {
                // if the other half of the pair is not in the current route
                if (CWS_pair_list.at(ii).first_index == pair_first
                && total_route.end() == std::find(total_route.begin(),
                                                  total_route.end(),
                                                  CWS_pair_list.at(ii).second_index))
                {
                    if (random() % 100 < rand_chance)
                    {
                        pair_second = CWS_pair_list.at(ii).second_index;
                        //std::cout << euclideanDistance(node_coords.at(pair_first),
                        //                               node_coords.at(pair_second));
                        //std::cout << std::endl;
                        break;
                    }
                }
                else if (CWS_pair_list.at(ii).second_index == pair_first
                && total_route.end() == std::find(total_route.begin(),
                                                  total_route.end(),
                                                  CWS_pair_list.at(ii).first_index))
                {
                    if (random() % 100 < rand_chance)
                    {
                        pair_second = CWS_pair_list.at(ii).first_index;
                        //std::cout << euclideanDistance(node_coords.at(pair_first),
                        //                               node_coords.at(pair_second));
                        //std::cout << std::endl;
                        break;
                    }
                }
            }

            // add these first 2 stops
            subroute.push_back(pair_first);
            current_capacity -= (node_demands.at(pair_first));
            // if there's only 1 node left
            if (pair_second > node_coords.size())
            {
                break;
            }
            subroute.push_back(pair_second);
            current_capacity -= (node_demands.at(pair_second));

            ii = 0;

            // while the vehicle still has space
            while (current_capacity > MIN_CAPACITY
            // and we haven't run out of nodes
            && ++ii < CWS_pair_list.size()
            // and the route isn't over the set limit
            && subroute.size() <= STOPS_PER_ROUTE)
            {
                loop_rand = random() % 100;

                // choose a new pair to look at
                pair_second = (CWS_pair_list.at(ii).second_index);
                pair_first = (CWS_pair_list.at(ii).first_index);

                // random chance to take it
                if (loop_rand < rand_chance
                // and neither are already in the total route
                && std::find(total_route.begin(),
                             total_route.end(),
                             pair_second) == total_route.end()
                && std::find(total_route.begin(),
                             total_route.end(),
                             pair_first) == total_route.end()
                // and make sure ONLY one is in the subroute
                && (subroute.size() < 1
                   || (!(std::find(subroute.begin(),
                                  subroute.end(),
                                  pair_second) == subroute.end())
                   != !(std::find(subroute.begin(),
                                  subroute.end(),
                                  pair_first) == subroute.end())))
                )
                {
                    addNode(subroute,
                            current_capacity,
                            pair_first,
                            pair_second);
                }
            }

            size_t route_sz = subroute.size();
            // push route onto the back of total route
            for (jj = 0; jj < route_sz; jj++)
            {
                total_route.push_back(subroute.back());
                subroute.pop_back();
            }

            // remove any duplicates
            for (jj = total_route.end() - std::unique(total_route.begin(),
                                                      total_route.end());
            jj; jj--)
            {
                total_route.pop_back();
            }

            // number of nodes left to add to route
            //kk = nodesLeft(total_route, remaining);
            kk = node_coords.size() - total_route.size() - 1;

            cur_vehicles++;
        }
        // end when there are no nodes left to add or the route has gone over provision
        while (kk && cur_vehicles <= NUM_SUBROUTES);

        // no nodes left to add
        if (!kk
        // right number of trucks being used the route
        && cur_vehicles <= NUM_SUBROUTES
        // haven't already generated it
        && all_routes.end() == std::find(all_routes.begin(),
                                         all_routes.end(),
                                         total_route))
        {
            all_routes.push_back(total_route);
            #ifdef VERBOSE
            std::cout << "\rGenerating " << GLOBAL_SIZE << " random routes ... " << all_routes.size() <<std::flush;
            #endif
        }
    }
}

// function for sorting points
bool cmp_distance
(point_info_t first, point_info_t second)
{
    return first.distance < second.distance;
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

