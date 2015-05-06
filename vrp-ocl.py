#!/usr/bin/env python

import pyopencl as cl
import os
import numpy as np
import itertools
mf = cl.mem_flags

from src.run_params import RunInfo

import matplotlib.pyplot as plt

class OCLRun(object):

    def timeKnl(self, kernel, *args, **kwargs):
        """ launch a kernel and see how long it took if profiling is on """

        # args are set every time, but it has next to no overhead
        event = kernel(*args, **kwargs)

        if self.cl_profiling:
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

        while 1:
            self.queue.finish()

            self.timeKnl(cl.enqueue_copy,
                self.queue,
                self.u,
                self.u_buf)

            self.timeKnl(cl.enqueue_copy,
                self.queue,
                self.u_buf,
                self.u)

            if iteration > 500:
                break
                plt.imshow(self.u, cmap=plt.cm.pink)
                plt.show()
                break

    def initDevBuffers(self):
        # copy to device
        def createBuf(host_arr, name):
            setattr(self, name + "_buf", cl.Buffer(self.context,
                    mf.READ_WRITE | mf.COPY_HOST_PTR, hostbuf=host_arr))

        # create one of these buffers buffer for each sim - done in parallel
        def createOnes(name):
            tmp = np.ones(self.grid_size)
            createBuf(tmp, name)

        createOnes("Kx")
        createOnes("Ky")
        createOnes("r")
        createOnes("sd")
        
        coefs = np.ones(200)
        createBuf(coefs, "alpha")
        createBuf(coefs, "beta")

        self.u = np.ones(self.grid_shape)
        self.u[10:20, 10:20] = 50

        createBuf(self.u, "u")

    def initPoints(self):
        options = ""

        # add options for constants
        options += "-D NOTEST "
        options += "-D LOCAL_SIZE={0:d} ".format(self.run_info.args["pop_size"])
        options += "-D GLOBAL_SIZE={0:d} ".format(self.run_info.args["pop_size"]*self.run_info.args["num_populations"])
        options += "-D MUT_RATE={0:d} ".format(self.run_info.args["mutation_rate"])
        options += "-D NUM_SUBROUTES={0:d} ".format(self.run_info.args["num_trucks"])
        options += "-D MAX_CAPACITY={0:d} ".format(self.run_info.args["capacity"])
        options += "-D MIN_CAPACITY={0:d} ".format(self.run_info.args["min_capacity"])
        options += "-D NUM_NODES={0:d} ".format(self.run_info.args["dimension"])
        options += "-D DEPOT_NODE={0:d} ".format(self.run_info.args["depot"])
        options += "-D MAX_PER_ROUTE={0:d} ".format(self.run_info.args["stops_per_route"])

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
        if self.cl_profiling and len(self.kernel_times) > 0:
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

        device_idx = self.run_info.args["ocl_device"]

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

        if self.run_info.args["ocl_profiling"]:
            self.queue = cl.CommandQueue(self.context, self.device,
                    properties=cl.command_queue_properties.PROFILING_ENABLE)
        else:
            self.queue = cl.CommandQueue(self.context, self.device)

    def __init__(self):
        # for optional profiling
        self.kernel_times = {}
        self.cl_profiling = True

        # verbose output
        os.environ['PYOPENCL_COMPILER_OUTPUT'] = "1"

        self.run_info = RunInfo(description="CVRP solver")

        self.initOCL()
        self.initPoints()
        self.initDevBuffers()

if __name__ == '__main__':
    runner = OCLRun()
    runner.run()

