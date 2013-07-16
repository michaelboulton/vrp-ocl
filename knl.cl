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

// for IOC
#ifndef NOTEST
#define MUT_RATE 25
#define MUT_SWAP
#define NUM_SUBROUTES 7
#define NUM_COORDS 75
#define CAPACITY 220
#define LOCAL_SIZE 256
#define GLOBAL_SIZE 256
#define MAX_PER_ROUTE 12
#define K_OPT 2
#endif

// for sorting kernels
typedef struct sort_info {
    // length of route - used to compare
    float route_length;
    // index of route in parent/children (could be either)
    __global const uint* idx;
} sort_t;

/*
 *  Random number generator
 *  source: http://cas.ee.ic.ac.uk/people/dt10/research/rngs-gpu-mwc64x.html
 */
inline uint MWC64X(__global uint2* const state)
{
    enum _dummy { A=4294883355U };
    //unsigned int A = 4294883355U ;
    uint x=(*state).x, c=(*state).y;  // Unpack the state
    uint res=x^c;                     // Calculate the result
    uint hi=mul_hi(x,A);              // Step the RNG
    x=x*A+c;
    c=hi+(x<c);
    *state=(uint2)(x,c);               // Pack the state back up
    return res;                       // Return the next result
}

/**************************************/

/*
 *  Returns euclidean distance between two points
 */
#if 0
inline float euclideanDistance
(uint2 first, uint2 second)
{
    /*
    float x1, y1, x2, y2;

    x1 = first.first;
    y1 = first.second;
    x2 = second.first;
    y2 = second.second;

    // native is a bit quicker?
    return native_sqrt(((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1)));

    const float2 a = (float2)(first.x, first.y);
    const float2 b = (float2)(second.x, second.y);
    */

    return fast_distance(convert_float2(first), convert_float2(second));
}
#else
#define euclideanDistance(first,second)     \
    (fast_distance(convert_float2(first),   \
                   convert_float2(second)))
#endif

/*
 *  Returns how long a subroute is
 */
float subrouteLength
(                 uint*  const __restrict route,
            const int                     route_stops,
 __constant const uint2* const __restrict node_coords)
{
    uint ii;
    float route_length = 0.0f;

    for(ii = 0; ii < route_stops; ii++)
    {
        route_length += euclideanDistance(node_coords[route[ii]],
                                          node_coords[route[ii + 1]]);
    }

    return route_length;
}

/*
 *  Goes through the array for the range specified and sees if the value passed
 *  is already in it
 */
inline
bool contains
(int * arr, int val, int lb, int range)
{
    for (int ii = lb; ii < lb+range; ii++)
    {
        if (arr[ii] == val)
        {
            return true;
        }
    }

    return false;
}

/*
 *  Takes global pointers to chromosomes, node coordinates, node demands, and a
 *  pointer to route_starts which is global storage holding the positions in
 *  each chromosome where the beginning of the mini subroutes start.
 *
 *  The subroutes are calculated based on the demand of the nodes - go through
 *  the chromosome until the demand for the nodes visited surpasses the
 *  capacity of the truck, then mark it as the split to the next subroute and
 *  start counting again.
 *
 *  For example, for a route 1 2 3 4 5 6 7 8 9, the subroute starts might be at
 *  position 0, 4, and 7. This route is split up into subroutes 1-2-3-4, 5-6-7,
 *  and 8-9.
 */
void findRouteStarts
(__global   const uint*        __restrict chromosomes,
 __constant const uint2* const __restrict node_coords,
 __constant const uint*  const __restrict node_demands,
 __global         int*         __restrict route_starts)
{
    uint group_id = get_group_id(0);
    uint loc_id = get_local_id(0);

    // counters
    uint ii, rr;

    // stuff currently in truck - 0 at first
    uint cur_capacity = 0;

    // stops in current route - initially 0
    uint stops_taken = 0;

    // increment pointers to point to this thread's chromosome values
    chromosomes  += LOCAL_SIZE * NUM_NODES * group_id + loc_id * NUM_NODES;
    route_starts += LOCAL_SIZE * NUM_SUBROUTES  * group_id + loc_id * NUM_SUBROUTES;

    for(ii = 0; ii < NUM_SUBROUTES; ii++)
    {
        route_starts[ii] = 0;
    }

    // start at route_starts[1]
    rr = 1;

    // for the total length of the chromosome
    for(ii = 0; ii < NUM_NODES; ii++)
    {
        // ignore depot stops - go back to depot when its full / too many things in route
        while(DEPOT_NODE == chromosomes[ii])
        {
            //ii++;
            if(++ii >= NUM_NODES) break;
        }

        cur_capacity += node_demands[chromosomes[ii]];

        // if adding the next node will go over capacity
        if(cur_capacity > CAPACITY || ++stops_taken >= MAX_PER_ROUTE)
        {
            stops_taken = 0;
            cur_capacity = node_demands[chromosomes[ii]];

            route_starts[++rr] = ii;
        }
    }

    // route_starts[0] contains number of sub routes
    // route_starts[1] always contains 1 - the start of the first route
    route_starts[0] = rr;
}

/************************/

__kernel void fitness
(__global   const uint *          __restrict chromosomes,
 __global         float *   const __restrict results,
 __constant const uint2 *   const __restrict node_coords,
 __constant const uint*     const __restrict node_demands,
 __global         int *           __restrict route_starts)
{
    const uint glob_id = get_global_id(0);
    const uint loc_id = get_local_id(0);
    const uint group_id = get_group_id(0);

    // offset to this work item
    __global const uint* const chromosome = 
        chromosomes + LOCAL_SIZE * group_id * NUM_NODES + loc_id * NUM_NODES;

    uint ii, jj;

    uint cur_capacity = 0;
    // for calculating length of route
    float total_distance = 0.0f;

    // add distance and capacity for first node
    total_distance += euclideanDistance(node_coords[DEPOT_NODE],
                                        node_coords[chromosome[0]]);
    cur_capacity += node_demands[chromosome[0]];

    // stops in current route
    uint stops_taken = 1;

    // TODO change this to use route_starts instead of doing it again
    for(ii = 0, jj = 1;
    ii < NUM_NODES - 1 && jj < NUM_NODES;
    ii++, jj++)
    {
        cur_capacity += node_demands[chromosome[jj]];

        // if adding the next node will go over capacity
        if(cur_capacity > CAPACITY || ++stops_taken >= MAX_PER_ROUTE)
        {
            // add distance to and from depot
            total_distance += euclideanDistance(node_coords[chromosome[ii]],
                                                node_coords[DEPOT_NODE]);
            total_distance += euclideanDistance(node_coords[DEPOT_NODE],
                                                node_coords[chromosome[jj]]);

            // reset
            stops_taken = 1;
            cur_capacity = node_demands[chromosome[jj]];
        }
        else
        {
            total_distance += euclideanDistance(node_coords[chromosome[ii]],
                                                node_coords[chromosome[jj]]);
        }
    }

    // add distance to last node
    total_distance += euclideanDistance(node_coords[chromosome[NUM_NODES - 1]],
                                        node_coords[DEPOT_NODE]);


    results[glob_id] = total_distance;
}

/*
*   Modified version of bitonic sort
*   source: http://www.bealto.com/gpu-sorting_parallel-merge-local.html
*/

__kernel void ParallelBitonic_NonElitist
(__global       float * __restrict route_lengths,
 __global const uint *  __restrict chromosomes,
 __global       uint *  __restrict output)
{
    int ii = get_local_id(0);
    int group_id = get_group_id(0) ;
    int loc_id = get_local_id(0) ;
    int glob_id = get_global_id(0) ;

    // need to be twice the group size because this works on twice the range
    __local sort_t aux[LOCAL_SIZE * 2];

    // uses the current thread index and length of route to sort, then whatever
    // thread ends up with it in its index in the local array copies it back out to the output
    sort_t sort_pair;
    sort_pair.route_length = route_lengths[glob_id];
    // idx is a pointer to the relevant chromosome
    sort_pair.idx = chromosomes + LOCAL_SIZE*group_id*NUM_NODES + loc_id*NUM_NODES;

    // Load block in AUX[WG]
    aux[ii] = sort_pair;
    barrier(CLK_LOCAL_MEM_FENCE); // make sure AUX is entirely up to date

    // Loop on sorted sequence length
    for (int length = 1; length < LOCAL_SIZE; length <<= 1)
    {
        bool direction = ((ii & (length << 1)) != 0); // direction of sort: 0=asc, 1=desc
        // Loop on comparison distance (between keys)
        for (int inc = length; inc > 0; inc >>= 1)
        {
            #define getKey(x) (x.route_length)
            int j = ii ^ inc; // sibling to compare
            sort_t iData = aux[ii];
            uint iKey = getKey(iData);
            sort_t jData = aux[j];
            uint jKey = getKey(jData);
            bool smaller = (jKey < iKey) || ( jKey == iKey && j < ii );
            bool swap = smaller ^ (j < ii) ^ direction;
            barrier(CLK_LOCAL_MEM_FENCE);
            aux[ii] = (swap)?jData:iData;
            barrier(CLK_LOCAL_MEM_FENCE);
            #undef getKey
        }
    }

    uint kk;
    output += LOCAL_SIZE*group_id*NUM_NODES + loc_id*NUM_NODES;

    __global const uint* input = aux[loc_id].idx;
    for(kk = 0; kk < NUM_NODES; kk++)
    {
        output[kk] = input[kk];
    }

    route_lengths[glob_id] = aux[loc_id].route_length;
}

__kernel void ParallelBitonic_Elitist
(__global       float * __restrict route_lengths,
 __global const uint *  __restrict parents,
 __global const uint *  __restrict children,
 __global       uint *  __restrict output)
{
    int ii = get_local_id(0); // index in workgroup
    int group_id = get_group_id(0) ;
    int loc_id = get_local_id(0) ;
    int loc_div;

    __local sort_t aux[LOCAL_SIZE * 4];

    // offset to this work group
    route_lengths += group_id * LOCAL_SIZE;
    output += LOCAL_SIZE * NUM_NODES * group_id + loc_id * NUM_NODES;

    // uses the current thread index and length of route to sort, then whatever
    // thread ends up with it in its index in the local array copies it back out to the output
    sort_t sort_pair;

    // if an even item in the work group
    if(loc_id > LOCAL_SIZE)
    {
        // then use this to access children
        loc_div = loc_id / 2;
        sort_pair.idx = children + LOCAL_SIZE * NUM_NODES * group_id + loc_div * NUM_NODES;
        sort_pair.route_length = route_lengths[loc_div];
    }
    else
    {
        // then use this to access parents
        loc_div = loc_id / 2;
        sort_pair.idx = parents + LOCAL_SIZE * NUM_NODES * group_id + loc_div * NUM_NODES;
        sort_pair.route_length = route_lengths[loc_div + GLOBAL_SIZE];
    }

    aux[loc_id] = sort_pair;

    barrier(CLK_LOCAL_MEM_FENCE); // make sure AUX is entirely up to date

    // Loop on sorted sequence length
    for (int length = 1; length < get_local_size(0); length <<= 1)
    {
        bool direction = ((ii & (length << 1)) != 0); // direction of sort: 0=asc, 1=desc
        // Loop on comparison distance (between keys)
        for (int inc = length; inc > 0; inc >>= 1)
        {
            #define getKey(x) (x.route_length)
            int j = ii ^ inc; // sibling to compare
            sort_t iData = aux[ii];
            uint iKey = getKey(iData);
            sort_t jData = aux[j];
            uint jKey = getKey(jData);
            bool smaller = (jKey < iKey) || ( jKey == iKey && j < ii );
            bool swap = smaller ^ (j < ii) ^ direction;
            barrier(CLK_LOCAL_MEM_FENCE);
            aux[ii] = (swap)?jData:iData;
            barrier(CLK_LOCAL_MEM_FENCE);
            #undef getKey
        }
    }

    // copy back only the first half
    if(loc_id < LOCAL_SIZE)
    {
        __global const uint* input = aux[loc_id].idx;
        for(ii = 0; ii < NUM_NODES; ii++)
        {
            output[ii] = input[ii];
        }

        route_lengths[loc_id] = aux[loc_id].route_length;
    }
}

__kernel void breed
(__global const uint*          __restrict parents,
 __global       uint*          __restrict children,
 __global       uint*          __restrict route_lengths,
 __global       int *          __restrict route_starts,
 __constant     uint2*   const __restrict node_coords,
 __constant     uint*    const __restrict node_demands,
 __global       uint2*   const __restrict state)
{
    uint glob_id = get_global_id(0);
    uint group_id = get_group_id(0);
    uint loc_id = get_local_id(0);
    uint ii, jj;

    // offset to this work group
    parents += LOCAL_SIZE * NUM_NODES * group_id;

    // randomly choose
    uint other_parent, counter = 0, tmp_rand;

    // use only if its value is set, otherwise just choose randomly
#if (defined(ARENA_SIZE) && ARENA_SIZE)
    // choose one of the top ones based on a random choice
    do
    {
        other_parent = counter;
        tmp_rand = MWC64X(&state[glob_id]) % ARENA_SIZE;
    }
    while (other_parent == loc_id
    && counter++ < ARENA_SIZE
    && tmp_rand > counter);
#else
    // randomly choose
    do
    {
        other_parent = MWC64X(&state[glob_id]) % LOCAL_SIZE;
    }
    while (other_parent == loc_id);
#endif

    // offset to this item
    __global const uint* parent_1 = parents + loc_id * NUM_NODES;

    // other parent
    __global const uint* parent_2 = parents + other_parent * NUM_NODES;

    uint child[NUM_NODES];

#if defined(BREED_O1)

    for (ii = 0; ii < NUM_NODES; ii++)
    {
        child[ii] = 0;
    }

    // choose range
    uint lb, range;
    lb = MWC64X(&state[glob_id]) % NUM_NODES;
    // XXX possibly make range bound to 50% of chromosome or something? ?
    range = MWC64X(&state[glob_id]) % (NUM_NODES - lb);

    // copy an initial random range
    for (ii = lb; ii < range+lb; ii++)
    {
        child[ii] = parent_1[ii];
    }

    // start trying to copy from parent_2[0]
    jj = 0;

    // go through parent 2 and insert in order
    for (ii = 0; ii < NUM_NODES; ii++)
    {
        // hasn't had a value written to it yet
        if (!child[ii])
        {
            // go until a value that hasn't already been copied is found
            while (contains(child, parent_2[jj], lb, range))
            {
                jj++;
            }

            // copy and increment jj
            child[ii] = parent_2[jj++];
        }
    }

#elif defined(BREED_CX)

    // at the end, will hold array of numbers form 1-num cycles
    int cycles[NUM_NODES];

    // reset
    for (ii = 0; ii < NUM_NODES; ii++)
    {
        cycles[ii] = 0;
    }

    // cycle count
    uint cc = 1;

    // index of current element to look at
    uint cur_idx;

    do
    {
        // find somewhere random to start this cycle
        do
        {
            cur_idx = MWC64X(&state[glob_id]) % NUM_NODES;
        }
        while (cycles[cur_idx]);

        // keep going until the cycle is closed
        while (!cycles[cur_idx])
        {
            // mark as being in this route
            cycles[cur_idx] = cc;

            for (ii = 0; ii < NUM_NODES; ii++)
            {
                if (parent_1[ii] == parent_2[cur_idx])
                {
                    cur_idx = ii;
                    break;
                }
            }
        }

        // if still doing crossover
        bool finished = false;

        // see if they've all been added
        for (ii = 0; ii < NUM_NODES; ii++)
        {
            // if even a single one remains, dont finish
            if (!cycles[ii])
            {
                finished = true;
                break;
            }
        }
        if (!finished)
        {
            break;
        }

    }
    // increment cycle number
    // while it hasn't been told to end
    while (cc++);

    // flip parent between cycles
    bool parent_flip = false;

    // cycle numbers
    for(ii = 1; ii < cc + 1; ii++)
    {
        // go through cycle, picking from one parent or another
        for(jj = 0; jj < NUM_NODES; jj++)
        {
            // if the current cycle contained this idx
            if(cycles[jj] == ii)
            {
                child[jj] = parent_flip ? parent_1[jj] : parent_2[jj];
            }
        }

        // choose from different parent for each cycle
        parent_flip = !parent_flip;
    }

#elif defined(BREED_PMX)

    // to see which elements in child have not yet been written
    int copied[NUM_NODES];
    // reset
    for(jj = 0; jj < NUM_NODES; jj++)
    {
        copied[jj] = 0;
    }

    // first choose a random range like in swap mutation
    uint lb, range;
    lb = MWC64X(&state[glob_id]) % NUM_NODES;
    // XXX possibly make range bound to 50% of chromosome or something? ?
    range = MWC64X(&state[glob_id]) % (NUM_NODES - lb);

    // copy initial range from parent 1 (most genetic code wilb be from it?)
    for (ii = lb; ii < range+lb; ii++)
    {
        child[ii] = parent_1[ii];
        copied[ii] = 1;
    }

    // value at this position in parent_1 (and hence the child too)
    uint p1_val;

    /*
     *  Could either do a nested loop or do one loop to find all the elemnts
     *  which haven't been copied from parent 2 - tradeoff between memory usage
     *  and computation
     */

    // for each value in the copied range in child/parent_1
    for (ii = lb; ii < range+lb; ii++)
    {
        bool found = false;

        /*
         *  Go through all elements of parent_2 in the same range that was
         *  copied and see if any of them are the same - if they are, then it
         *  doens't need to be copied and just continue
         */
        for (jj = lb; jj < range+lb; jj++)
        {
            // break immediately if found in parent_2 range
            if (parent_2[ii] == child[jj])
            {
                found = true;
                break;
            }
        }

        // if child[ii] was in the copied range in parent_2, go to next element
        // in child
        if (found)
        {
            continue;
        }

        /*
         *  p1_val is the element in parent_1 at the same index for which the
         *  non matching element in parent 2 was found
         */
        p1_val = parent_1[ii];

        // has parent_2[ii] been copied yet
        bool finished = false;

        // go until something has been copied
        do
        {
            // try to find p1_val in parent_2
            for (jj = 0; jj < NUM_NODES; jj++)
            {
                if (parent_2[jj] == p1_val)
                {
                    // if its not in the copied range
                    if (jj < lb || jj >= range+lb)
                    {
                        // copy initial unmatched parent_2 value
                        child[jj] = parent_2[ii];
                        copied[jj] = 1;
                        finished = true;
                    }
                    else
                    {
                        p1_val = parent_1[jj];
                    }

                    break;
                }
            }
        }
        while (!finished);

    }

    // copy any empty spots in child from parent_2
    for (ii = 0; ii < NUM_NODES; ii++)
    {
        if (copied[ii] == 0)
        {
            child[ii] = parent_2[ii];
        }
    }

#endif

    // increment children to point to this threads chromosome child
    children += LOCAL_SIZE * NUM_NODES * group_id + loc_id * NUM_NODES;

    // copy into children
    for(ii = 0; ii < NUM_NODES; ii++)
    {
        children[ii] = child[ii];
    }
}

__kernel void mutate
(__global       uint  *       __restrict chromosomes,
 __global       uint2 * const __restrict state)
{
    uint glob_id = get_global_id(0);
    uint group_id = get_group_id(0);
    uint loc_id = get_local_id(0);
    uint ii, jj, cc;
    uint tmp_val;
    #define SWAP(x, y) tmp_val=x; x=y; y=tmp_val;

    chromosomes += LOCAL_SIZE * group_id * NUM_NODES + loc_id * NUM_NODES;

    // copy in all threads for better mem access, even if some won't use it
    uint chromosome[NUM_NODES];
    for(ii = 0; ii < NUM_NODES; ii++)
    {
        chromosome[ii] = chromosomes[ii];
    }

    // mutate with MUT_RATE% chance
    if((MWC64X(&state[glob_id]) % 100) < MUT_RATE)
    {
#if defined(MUT_REVERSE)

        // reverse a random section of the chromosome
        uint ll, uu, range;

        ll = MWC64X(&state[glob_id]) % NUM_NODES;
        range = MWC64X(&state[glob_id]) % (NUM_NODES - ll);
        uu = ll + range;

        for(; ll < uu; ll++, uu--)
        {
            SWAP(chromosome[ll], chromosome[uu]);
        }

#elif defined(MUT_SWAP)

        uint ll1, uu1, ll2, uu2, range;

        /*
         *  number of values to swap
         *
         *  calculated like:
         *
         *      <--range-->     <--range-->
         *  [------------------------------------]
         *      ^         ^     ^         ^
         *     ll1       uu1   ll2       uu2
         *
         *  ranges will not overlap
         */

        // value to start bottom end of swap at - skewed towards bottom end
        ll1 = MWC64X(&state[glob_id]) % NUM_NODES;
        // FIXME not working - hanging on GPU
        //ll1 = MWC64X(&state[glob_id]) % (MWC64X(&state[glob_id]) % NUM_NODES);

        // range is random length smaller than that left...
        range = MWC64X(&state[glob_id]) % (NUM_NODES - ll1);
        // ...and enough to fit 2 ranges in to swap
        range /= 2;

        // uu1 calculated from range
        uu1 = ll1 + range;

        // calculate ll2 as some position long enough to fit the swap range in
        // after
        ll2 = MWC64X(&state[glob_id]) % (NUM_NODES - (uu1 + range));
        // and its after uu1
        ll2 += uu1;

        // swap range
        for(ii = 0; ii < range; ii++)
        {
            SWAP(chromosome[ll1+ii], chromosome[ll2+ii]);
        }

#else

        #error "No mutation strategy specified"

#endif

    }

    for(ii = 0; ii < NUM_NODES; ii++)
    {
        chromosomes[ii] = chromosome[ii];
    }
}

/************************/

__kernel void simpleTSP
(__global         uint*        __restrict chromosomes,
 __global         int*         __restrict route_starts,
 __constant const uint2* const __restrict node_coords,
 __constant const uint*  const __restrict node_demands,
 __global         uint2* const __restrict state)
{
    uint glob_id = get_global_id(0);
    uint loc_id = get_local_id(0);
    uint group_id = get_group_id(0);

    // counters
    uint ii, jj, kk, oo;

    findRouteStarts(chromosomes,
                    node_coords,
                    node_demands,
                    route_starts);

    // offset
    chromosomes  += LOCAL_SIZE * group_id * NUM_NODES + loc_id * NUM_NODES;
    route_starts += LOCAL_SIZE * group_id * NUM_SUBROUTES + loc_id * NUM_SUBROUTES;

    // copy chromosome into private memory
    uint chromosome[NUM_NODES];
    for(ii = 0; ii < NUM_NODES; ii++)
    {
        chromosome[ii] = chromosomes[ii];
    }

    // swap two values
    uint tmp_val;
    #define SWAP(x, y) tmp_val=x; x=y; y=tmp_val;

    // best length of route
    float best_subroute_length;
    float cur_subroute_length;

    uint num_routes = route_starts[0];
    // for each mini route
    for(ii = 1; ii < num_routes; ii++)
    {
        // beginning and end of current route, including depot stops
        uint route_begin = route_starts[ii];

        // current route to look at
        uint* const current_subroute = chromosome + route_begin;

        uint route_end;
        if (ii < num_routes - 1)
        {
            route_end = route_starts[ii + 1];
        }
        else
        {
            route_end = NUM_NODES;
        }

        // length of current route
        uint route_length = route_end - route_begin;

        // best for this route
        best_subroute_length = subrouteLength(current_subroute,
                                              route_length,
                                              node_coords);

        // for each item in the current sub route pointed to by current_subroute
        for(jj = 1; jj < route_length; jj++)
        {
            // for each pair of routes in total route
            for(kk = jj + 1; kk < route_length; kk++)
            {
                if(jj != kk
                && current_subroute[jj] != 0
                && current_subroute[kk] != 0)
                {
                    // swap and see if its good
                    SWAP(current_subroute[jj], current_subroute[kk]);
                    cur_subroute_length = subrouteLength(current_subroute,
                                                         route_length,
                                                         node_coords);

                    if(cur_subroute_length < best_subroute_length)
                    {
                        best_subroute_length = cur_subroute_length;
                    }
                    else
                    {
                        // swap back if not
                        SWAP(current_subroute[jj], current_subroute[kk]);
                    }
                }
            }
        }
    }

    // copy chromosome back
    for(ii = 0; ii < NUM_NODES; ii++)
    {
        chromosomes[ii] = chromosome[ii];
    }
}

/************************/

// TODO this can probly be improved
__kernel void foreignExchange
(__global uint* __restrict chromosomes,
 __global uint2* const __restrict state)
{
    uint loc_id = get_local_id(0);
    uint group_id = get_group_id(0);
    uint num_groups = get_num_groups(0);

    // the best item from each group gets swapped
    if(loc_id == 0 && group_id < num_groups / 2)
    {
        // best in this work group
        __global uint* local_chrom = chromosomes
            + LOCAL_SIZE * group_id * NUM_NODES;

        // best in the other work group
        __global uint* foreign_chrom = chromosomes
            + LOCAL_SIZE * (group_id + (num_groups / 2)) * NUM_NODES;

        int rand_offset = MWC64X(&state[get_global_id(0)]) % LOCAL_SIZE;

        foreign_chrom += NUM_NODES * rand_offset;

        uint tmp_val;
        #define SWAP(x, y) tmp_val=x; x=y; y=tmp_val;

        uint ii;
        for(ii = 0; ii < NUM_NODES; ii++)
        {
            SWAP(local_chrom[ii], foreign_chrom[ii]);
        }
    }
}

