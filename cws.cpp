#include "common_header.hpp"

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
int& current_capacity,
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
                        current_capacity,
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
