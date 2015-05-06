#!/usr/bin/env python

import pyopencl as cl
import pyopencl.array as cl_array
import os
import random
import numpy as np
import itertools
mf = cl.mem_flags

from src.run_params import RunInfo

import matplotlib.pyplot as plt

def gen_route_cws(node_coords):
    node_coords = np.random.permutation(node_coords)

    route = node_coords.copy()*0
    route[0] = node_coords[0, :]

    old_remaining = node_coords[1:,:]
    last_added = 0

    while len(old_remaining) > 1:
        remaining = np.vstack(filter(lambda x:x[0] not in route[:, 0], old_remaining))
        distances = np.sqrt(
            (remaining[:,1] - route[last_added,1])**2 +
            (remaining[:,2] - route[last_added,2])**2)

        argmin = np.argmin(distances)
        if random.random() < 0.95:
            route[last_added + 1] = remaining[argmin]
        else:
            route[last_added + 1] = remaining[random.random() * len(remaining)]

        last_added += 1
        old_remaining = remaining

    route[-1] = old_remaining[0]

    return route[:, 0]

class OCLRun(object):

    def timeKnl(self, kernel, *args, **kwargs):
        """ launch a kernel and see how long it took if profiling is on """

        # args are set every time, but it has next to no overhead
        event = kernel(*args, **kwargs)

        if self.run_info.ocl_profiling:
            event.wait()

            try:
                name = kernel.get_info(cl.kernel_info.FUNCTION_NAME)
            except AttributeError as e:
                name = "memcpy"

            if name not in self.kernel_times:
                self.kernel_times[name] = 0
            self.kernel_times[name] += (event.profile.end - event.profile.start)/1000000000.

            return
            print "Did {0:s} in {1:.5f} seconds".format(
                kernel.get_info(cl.kernel_info.FUNCTION_NAME), \
                (event.profile.end - event.profile.start)/1000000000.)

    def findRouteStarts(self, which_arr):
        self.timeKnl(self.program.findRouteStarts,
            self.queue, self.global_size, self.local_size,
            which_arr,
            self.node_demands_buf,
            self.route_starts_buf)

    def calcFitness(self, which_arr):
        self.timeKnl(self.program.fitness,
            self.queue, self.global_size, self.local_size,
            which_arr,
            self.results_buf,
            self.node_coords_buf,
            self.route_starts_buf)

    def neSort(self, which_array):
        self.timeKnl(self.program.ParallelBitonic_NonElitist,
            self.queue, self.global_size, self.local_size,
            self.results_buf,
            which_array,
            self.sorted_buf)

    def crossover(self):
        self.timeKnl(self.program.breed,
            self.queue, self.global_size, self.local_size,
            self.parents_buf,
            self.children_buf,
            self.rand_state_buf)

    def TSPSolve(self, which_arr):
        self.timeKnl(self.program.simpleTSP,
            self.queue, self.global_size, self.local_size,
            self.parents_buf,
            self.route_starts_buf,
            self.node_coords_buf,
            self.node_demands_buf)

    def mutate(self):
        self.timeKnl(self.program.mutate,
            self.queue, self.global_size, self.local_size,
            self.children_buf,
            self.rand_state_buf)

    def copyBack(self):
        self.timeKnl(self.program.copy_back,
            self.queue, self.global_size, self.local_size,
            self.sorted_buf,
            self.parents_buf)

    def foreignExchange(self):
        self.timeKnl(self.program.foreignExchange,
            self.queue, self.global_size, self.local_size,
            self.parents_buf,
            self.rand_state_buf)

    def run(self):
        # XXX debugging - print out the value of a device array
        def cw(name, idx=None, size=None):
            if None == size:
                tmp = np.zeros([self.levels])
            else:
                tmp = np.zeros([size])
            cl.enqueue_copy(self.queue, tmp, getattr(self, name + "_buf"))
            if None == idx:
                return tmp
            else:
                return tmp[idx]

        self.global_size = self.run_info.total_chromosomes,
        self.local_size = self.run_info.pop_size,

        self.findRouteStarts(self.parents_buf)
        self.TSPSolve(self.parents_buf)
        self.calcFitness(self.parents_buf)
        self.neSort(self.parents_buf)
        self.copyBack()

        for i in xrange(self.run_info.num_iterations):
            self.foreignExchange()
            self.crossover()
            self.mutate()

            self.findRouteStarts(self.children_buf)

            if not (i % self.run_info.tsp_solve_frequency):
                self.TSPSolve(self.children_buf)

            self.calcFitness(self.children_buf)
            self.neSort(self.children_buf)

            self.copyBack()

            if not (i % 50):
                self.getBestRoute()

        self.getBestRoute()

    def getBestRoute(self):
        self.findRouteStarts(self.parents_buf)
        self.calcFitness(self.parents_buf)
        self.neSort(self.parents_buf)
        self.calcFitness(self.parents_buf)

        results = np.zeros(self.global_size, dtype=np.float32)

        self.queue.finish()
        cl.enqueue_copy(self.queue, results, self.results_buf)

        argmin = np.argmin(results)
        smallest_route = results[argmin]

        host_chrom = np.zeros(self.run_info.dimension, dtype=np.uint32)
        cl.enqueue_copy(self.queue, host_chrom, self.parents_buf,
            device_offset=argmin*self.run_info.dimension*4)

        print smallest_route
        np.set_printoptions(linewidth=1000000)
        print host_chrom

    def genInitialRoutes(self):
        route = self.run_info.node_info[:,0]
        lots_of_routes = np.tile(route, (self.run_info.total_chromosomes, 1))

        map(np.random.shuffle, lots_of_routes)

        random_routes = lots_of_routes.astype(np.uint32)

        from multiprocessing import Pool
        mpool = Pool()
        better_routes = mpool.imap_unordered(gen_route_cws,
            itertools.repeat((self.run_info.node_info[:,:3]),
            self.run_info.total_chromosomes), chunksize=10)
        mpool.close()
        mpool.join()

        return np.hstack(better_routes)

    def initDevBuffers(self):
        # copy to device
        def createBuf(host_arr, name, flags=mf.READ_WRITE|mf.COPY_HOST_PTR):
            setattr(self, name + "_buf", cl.Buffer(self.context,
                    flags, hostbuf=host_arr))

        createBuf(self.genInitialRoutes(), "parents")

        createBuf(np.zeros(self.run_info.dimension*self.run_info.total_chromosomes,
            dtype=np.uint32), "children")

        createBuf(np.zeros(self.run_info.num_trucks*2*self.run_info.total_chromosomes,
            dtype=np.int32), "route_starts")

        #node_coords = self.run_info.node_info[:,1:3].astype(cl_array.vec.float2)
        node_coords = self.run_info.node_info[:,1:3]
        node_demands = self.run_info.node_info[:,-1]
        createBuf(node_coords.astype(np.float32), "node_coords",
            flags=mf.READ_ONLY|mf.COPY_HOST_PTR)
        createBuf(node_demands.astype(np.uint32), "node_demands",
            flags=mf.READ_ONLY|mf.COPY_HOST_PTR)

        createBuf(np.zeros(2*self.run_info.total_chromosomes,
            dtype=np.float32), "results")
        createBuf(np.zeros(2*self.run_info.dimension*self.run_info.total_chromosomes,
            dtype=np.uint32), "sorted")

        random_numbers  = np.random.rand(self.run_info.total_chromosomes*2)
        random_numbers *= np.iinfo(np.uint32).max
        random_numbers  = random_numbers.astype(dtype=np.uint32)
        createBuf(random_numbers, "rand_state")
        createBuf(random_numbers, "rand_out")

    def initPoints(self):
        options = ""

        # add options for constants
        options += "-D NOTEST "
        options += "-D NUM_NODES={0:d} ".format(self.run_info.dimension)
        options += "-D NUM_SUBROUTES={0:d} ".format(self.run_info.num_trucks)
        options += "-D MAX_PER_ROUTE={0:d} ".format(self.run_info.stops_per_route)
        options += "-D MAX_CAPACITY={0:d} ".format(self.run_info.capacity)
        options += "-D MIN_CAPACITY={0:d} ".format(self.run_info.min_capacity)
        options += "-D LOCAL_SIZE={0:d} ".format(self.run_info.pop_size)
        options += "-D GLOBAL_SIZE={0:d} ".format(self.run_info.total_chromosomes)
        options += "-D DEPOT_NODE={0:d} ".format(self.run_info.depot)
        options += "-D ARENA_SIZE={0:d} ".format(self.run_info.arena_size)
        options += "-D MUT_RATE={0:d} ".format(self.run_info.mutation_rate)

        # breed/mutation types
        options += "-D MUT_{0:s} ".format(self.run_info.mutation_strategy)
        options += "-D BREED_{0:s} ".format(self.run_info.breed_strategy)

        # opencl options
        options += "-cl-mad-enable "
        options += "-cl-fast-relaxed-math "

        print "options = {0:s}".format(options)
        print "Building..."

        with open("src/knl.cl", "r") as kernel_src:
            self.program = cl.Program(self.context, kernel_src.read())

        self.program.build(options, devices=[self.device])
        print "Done."

    def __del__(self):
        """ print out profiling info on exit """
        if self.run_info.ocl_profiling and len(self.kernel_times) > 0:
            total = sum(self.kernel_times.values())
            print "Total kernel time {0:2.2f} secs".format(total)
            for k,v in self.kernel_times.items():
                print "{0} - {1:2.2f} secs ({2:2.2f}%)".format(k, v, v/total*100)

    def initOCL(self):
        platforms = cl.get_platforms()

        print "Available platforms:"
        for i in platforms:
            print " {0:s}".format(i)

        all_devices = [i for i in itertools.chain.from_iterable(pp.get_devices() for pp in platforms)]

        print "All devices:"
        for i in all_devices:
            print " {0:s}".format(i)

        device_idx = self.run_info.ocl_device

        try:
            self.device = all_devices[device_idx]
        except IndexError as e:
            raise RuntimeError("No device at index {0} found".format(device_idx))

        print
        print "Chosen {0:s}".format(self.device)
        print

        # no way to get this from the kernel with pyopencl?
        self.max_wg_size = self.device.get_info(cl.device_info.MAX_WORK_GROUP_SIZE)

        self.context = cl.Context([self.device])
        #self.context = cl.create_some_context()

        if self.run_info.ocl_profiling:
            self.queue = cl.CommandQueue(self.context, self.device,
                    properties=cl.command_queue_properties.PROFILING_ENABLE)
        else:
            self.queue = cl.CommandQueue(self.context, self.device)

    def __init__(self):
        # for optional profiling
        self.kernel_times = {}

        # verbose output
        os.environ['PYOPENCL_COMPILER_OUTPUT'] = "1"

        self.run_info = RunInfo(description="CVRP solver")

        self.initOCL()
        self.initPoints()
        self.initDevBuffers()

if __name__ == '__main__':
    runner = OCLRun()
    runner.run()

