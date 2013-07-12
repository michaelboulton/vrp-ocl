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

colours = ['r', 'g', 'b', 'y', 'k', 'c', 'm', 'r'] # TODO change to another colour

def length(best, nodes):
    # double check length
    ind = [int(i) for i in best.split()]
    total = 0
    for ii in xrange(len(ind)-1):
        x1 = (nodes[ind[ii]][0])
        y1 = (nodes[ind[ii]][1])

        x2 = (nodes[ind[ii+1]][0])
        y2 = (nodes[ind[ii+1]][1])
        total += math.sqrt( (y1-y2)**2 + (x1-x2)**2 )

    return total

def old_plot(best, split_best, demands, nodes, x, y):
    total = length(best, nodes)
    print "Total length", total

    fig=plt.figure()
    data = fig.add_subplot(111)
    data.scatter(x,y, marker='x', c='k')
    #print split_best
    for route in split_best:
        print route,
        split_route = route.split()

        sum_len = 0
        for i in split_route:
            #print sum_len,"+",
            #print demands[i],"=",
            sum_len += int(demands[i])
            #print sum_len
        print "- capacity", sum_len,
        if sum_len > 220:
            print "- OVER CAPACITY - INVALID ROUTE"
        else:
            print

        line_taken_x = [x[int(ii)-1] for ii in split_route]
        line_taken_y = [y[int(ii)-1] for ii in split_route]

        colour = colours[split_best.index(route) % len(colours)]
        plt.plot(line_taken_x, line_taken_y, c=colour)

    plt.show()

def new_plot(best, split_best, demands, nodes, x, y):

    whole_route = []
    for i in split_best:
        for o in i.split():
            whole_route.append(o)

    for x in whole_route:
        if x == '1':
            del whole_route[whole_route.index(x)]

    cur_cap = 0

    best_route = ""
    best_len = 1000000
    weird_best = ""

    for max_cap in xrange(max([int(i) for i in demands])+10, 220):
        for rl in xrange(3, 20):
            cur_count = 0
            new_route = "1 "
            weird_route = "1->"
            while(1):
                
                # number of stops stopped at so far
                stop_count = 0
                cur_cap = 0

                for ii in xrange(cur_count, len(whole_route)):

                    cur_cap += int(demands[whole_route[ii]])
                    stop_count += 1

                    # if max specs for subroute have been reached
                    if cur_cap > max_cap or stop_count >= rl:
                        new_route += '1 '
                        weird_route += '1\n1->'
                        break
                    else:
                        new_route += (whole_route[ii])
                        new_route += (' ')
                        weird_route += str(int(whole_route[ii]))
                        weird_route += '->'
                        cur_count += 1
                if cur_count >= len(whole_route):
                    break
            new_route += ('1')
            weird_route += ('1')
            new_len = length(new_route, nodes)
            if new_len < best_len:
                best_route = new_route
                best_len = new_len
                weird_best = weird_route
    print weird_best
    return best_route

def plot(best_in, recurse):

    x = numpy.zeros(76)
    y = numpy.zeros(76)
    icount = 0
    best = best_in.strip()

    with open('fruitybun-data.vrp') as sett_file:
        at_node=False
        at_demand=False
        for line in sett_file:
            if "NODE_" in line:
                at_node=True
            elif at_node==True:
                if "DEMAND_" in line:
                    at_node=False
                    at_demand=True
                    break
                else:
                    line = line.split()
                    x[icount] = line[1]
                    y[icount] = line[2]
                    icount+=1

    split_best = best.split(' 1 ')
    # add depot to them
    for oo in split_best:
        if not oo.startswith('1 '):
            ii = split_best.index(oo)
            oo = '1 ' + oo
            split_best[ii] = oo
    for oo in split_best:
        if not oo.endswith(' 1'):
            ii = split_best.index(oo)
            oo = oo + ' 1'
            split_best[ii] = oo

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
                demands[str(int(dem_line[0]))] = dem_line[1]

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
                nodes[(int(dem_line[0]))] = int(dem_line[1]),int(dem_line[2])

    if recurse:
        new_best = best_in.lstrip('1 ').rstrip('1').rstrip(' ').split(' 1 ')
        modified = new_plot(best, new_best, demands, nodes, x, y)
        plot(modified, False)
    else:

        found = []
        # check to find duplicates or missing ones
        for i in best.split():
            if not i=='1' and i in found:
                print i, "already in route - first at", best.split().index(i)
            found.append(i)
        for i in xrange(1,76):
            if not str(i) in  best.split():
                print i, "not found in route"

        print split_best
        old_plot(best, split_best, demands, nodes, x, y)
        exit(1)

if __name__ == '__main__':
    if len(sys.argv) == 1:
        best_in = "53 28 14 55 20 36 9 47 35 68 5 52 4 45 33 10 40 73 59 13 41 18 46 30 58 16 6 48 62 29 23 63 3 34 64 24 57 42 65 43 44 2 74 76 31 49 38 21 71 61 72 37 70 22 75 27 8 54 15 60 12 67 66 39 11 32 56 26 51 19 25 50 17 7 69 "

    else:
        best_in = ""
        for i in sys.argv[1:]:
            best_in += str(i)
            best_in += " "

    plot(best_in, True)
