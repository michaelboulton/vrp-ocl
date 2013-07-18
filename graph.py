#!/usr/bin/python

#  Copyright (c) 2013 Michael Boulton
#  
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.

import matplotlib.pyplot as plt
import numpy
import re
import sys
import math

# keep same as in C++
MAX_CAPACITY = 220
MIN_CAPACITY = 2
NUM_SUBROUTES = 7;
STOPS_PER_ROUTE = 14;

colours = ['r', 'g', 'b', 'y', 'k', 'c', 'm', 'r'] # TODO change to another colour

def length(p1, p2, nodes):
    return math.sqrt((nodes[p1][1]-nodes[p2][1])**2 +
                     (nodes[p1][0]-nodes[p2][0])**2 )

def graph_plot(route, nodes, demands):

    best_len = 10000.0
    route_split = []

    for MC in xrange(MAX_CAPACITY-20, MAX_CAPACITY):
        for ST in xrange(4, STOPS_PER_ROUTE):
            #  subroutes split up
            cur_route = []
            subroute = []

            capacity = MC

            # initial leaving depot
            subroute.append(1)
            stop_count = 0

            for i in route:
                capacity -= demands[i]
                stop_count += 1

                if capacity <= MIN_CAPACITY or stop_count >= ST:
                    subroute.append(1)
                    cur_route.append(subroute)

                    # reset
                    capacity = MAX_CAPACITY
                    subroute = [1]
                    stop_count = 1

                    # add node
                    subroute.append(i)
                    capacity -= demands[i]
                else:
                    subroute.append(i)

            # final return to depot
            subroute.append(1)
            cur_route.append(subroute)

            # calculate length
            total_distance = 0.0
            for rr in cur_route:
                for i in xrange(len(rr) - 1):
                    total_distance += length(rr[i], rr[i+1], nodes)

            if total_distance < best_len:
                best_len = total_distance
                route_split = cur_route

    # plot
    fig = plt.figure()
    data = fig.add_subplot(111)
    data.scatter([i[0] for i in nodes.values()],
                 [i[1] for i in nodes.values()],
                 marker='x', c='k')

    for rr in route_split:
        colour = colours[route_split.index(rr) % len(colours)]
        coords = [nodes[i] for i in rr]
        plt.plot([i[0] for i in coords],
                 [i[1] for i in coords],
                 c=colour)

    print best_len

    plt.show()

def parse(route):

    demands = {}
    # load demands
    with open('fruitybun-data.vrp') as fb:
        #line = fb.readline()
        for line in fb:
            if "DEMAND" in line:
                break
        for line in fb:
            if "DEPOT" in line:
                break
            else:
                dem_line = line.split()
                demands[int(dem_line[0])] = int(dem_line[1])

    nodes = {}
    # load nodes
    with open('fruitybun-data.vrp') as fb:
        for line in fb:
            if "NODE" in line:
                break
        for line in fb:
            if "DEMAND" in line:
                break
            else:
                dem_line = line.split()
                nodes[int(dem_line[0])] = int(dem_line[1]),int(dem_line[2])

    found = []
    # check to find duplicates or missing ones
    for i in route:
        if i in found:
            print i, "already in route - first at", route.index(i)
        found.append(i)
    for i in xrange(2, len(nodes)+1):
        if not i in route:
            print i, "not found in route"

    graph_plot(route, nodes, demands)

if __name__ == '__main__':
    if len(sys.argv) == 1:
        best_in = "53 28 14 55 20 36 9 47 35 68 5 52 4 45 33 10 40 73 59 13 41 18 46 30 58 16 6 48 62 29 23 63 3 34 64 24 57 42 65 43 44 2 74 76 31 49 38 21 71 61 72 37 70 22 75 27 8 54 15 60 12 67 66 39 11 32 56 26 51 19 25 50 17 7 69 "

    else:
        best_in = ""
        for i in sys.argv[1:]:
            best_in += str(i)
            best_in += " "

    best_in = [int(i) for i in best_in.split()]
    parse(best_in)

