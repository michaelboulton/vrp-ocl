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

#if 0
void TBBRouteMaker::operator()
(route_vec_t& route) const
//(const tbb::blocked_range<size_t>& r) const
{
    unsigned int ii, jj, kk;

    // random numbers that decide whether to use a connection or not
    unsigned int loop_rand = 0;

    #ifdef VERBOSE
    std::cout << std::endl;
    std::cout << "Generating " << GLOBAL_SIZE << " random routes ... 0" << std::flush;
    #endif

    // TODO this while loop is the bit that needs to be task parallelised
    // while we still dont have enough chromosomes
    while (1)
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

            // FIXME TODO XXX just choose the pair, dont look for another second item (??? why did i do this)
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
                            total_route,
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
        while (kk && cur_vehicles <= NUM_TRUCKS);

        // no nodes left to add
        if (!kk
        // right number of trucks being used the route
        && cur_vehicles == NUM_TRUCKS
        // haven't already generated it
        && all_routes.end() == std::find(all_routes.begin(),
                                         all_routes.end(),
                                         total_route))
        {
            all_routes.push_back(total_route);
            #ifdef VERBOSE
            std::cout << "\rGenerating " << GLOBAL_SIZE << " random routes ... " << all_routes.size() <<std::flush;
            #endif
            break;
        }
    }
}
#endif

TBBRouteMaker::TBBRouteMaker
(const point_info_vec_t& CWS_pair_list_in,
 const node_map_t& node_coords_in,
 const demand_vec_t& node_demands_in)
:CWS_pair_list(CWS_pair_list_in),
 node_coords(node_coords_in),
 node_demands(node_demands_in)
{
    ;
}

