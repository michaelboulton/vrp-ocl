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

float euclideanDistance
(point_t first, point_t second)
{
    unsigned int x1, y1, x2, y2;

    x1 = first.s[0];
    y1 = first.s[1];
    x2 = second.s[0];
    y2 = second.s[1];

    return sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
}

// FIXME this doesn't really do it right? prints out route longer than it actually is
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

int main
(int argc, char* argv[])
{
    parseArgs(argc, argv);

    RunInfo r;
    r.parseInput();
    r.genSortedCWS();

    OCLLearn ocl_learner(r);

    alg_result_t result;

    result = ocl_learner.run();

    printRoute(result.second, r);

    return 0;
}

