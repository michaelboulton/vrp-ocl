#!/usr/bin/python

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

        line_taken_x = [x[int(ii)] for ii in split_route]
        line_taken_y = [y[int(ii)] for ii in split_route]

        colour = colours[split_best.index(route) % len(colours)]
        plt.plot(line_taken_x, line_taken_y, c=colour)

    total = length(best, nodes)

    print "Total length", total
    plt.show()

def new_plot(best, split_best, demands, nodes, x, y):

    whole_route = []
    for i in split_best:
        for o in i.split():
            whole_route.append(o)

    for x in whole_route:
        if x == '0':
            del whole_route[whole_route.index(x)]

    cur_cap = 0

    best_route = ""
    best_len = 1000000
    weird_best = ""

    for max_cap in xrange(max([int(i) for i in demands])+10, 220):
        for rl in xrange(3, 20):
            cur_count = 0
            new_route = "0 "
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
                        new_route += '0 '
                        weird_route += '1\n1->'
                        break
                    else:
                        new_route += (whole_route[ii])
                        new_route += (' ')
                        weird_route += str(int(whole_route[ii])+1)
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

    split_best = best.split(' 0 ')
    # add depot to them
    for oo in split_best:
        if not oo.startswith('0 '):
            ii = split_best.index(oo)
            oo = '0 ' + oo
            split_best[ii] = oo
    for oo in split_best:
        if not oo.endswith(' 0'):
            ii = split_best.index(oo)
            oo = oo + ' 0'
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
                demands[str(int(dem_line[0])-1)] = dem_line[1]

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
                nodes[(int(dem_line[0])-1)] = int(dem_line[1]),int(dem_line[2])

    if recurse:
        new_best = best_in.lstrip('0 ').rstrip('0').rstrip(' ').split(' 0 ')
        modified = new_plot(best, new_best, demands, nodes, x, y)
        plot(modified, False)
    else:

        found = []
        # check to find duplicates or missing ones
        for i in best.split():
            if not i=='0' and i in found:
                print i, "already in route - first at", best.split().index(i)
            found.append(i)
        for i in xrange(76):
            if not str(i) in  best.split():
                print i, "not found in route"

        old_plot(best, split_best, demands, nodes, x, y)
        exit(1)

if __name__ == '__main__':
    if len(sys.argv) == 1:
        best_in = "52 27 13 54 19 35 8 46 34 67 4 51 3 44 32 9 39 72 58 12 40 17 45 29 57 15 5 47 61 28 22 62 2 33 63 23 56 41 64 42 43 1 73 75 30 48 37 20 70 60 71 36 69 21 74 26 7 53 14 59 11 66 65 38 10 31 55 25 50 18 24 49 16 6 68 "

    else:
        best_in = ""
        for i in sys.argv[1:]:
            best_in += str(i)
            best_in += " "

    plot(best_in, True)
