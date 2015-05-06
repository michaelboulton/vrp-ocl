import numpy as np
import os
import re
import json
import argparse
import itertools
import functools

import pyopencl as cl

# run specific stuff
class RunInfo(object):
    def parseProblem(self):
        with open(self.input_file, "r") as input_file:
            all_lines = input_file.readlines()

        for line in all_lines:
            match = re.search("CAPACITY\s+:\s+([0-9]+)", line)
            if match:
                setattr(self, "capacity", int(match.groups()[0]))
            match = re.search("DIMENSION\s+:\s+([0-9]+)", line)
            if match:
                setattr(self, "dimension", int(match.groups()[0]))

        def read_block(start_label, end_label):
            block_txt = [i for i in
                itertools.dropwhile(lambda x:start_label not in x,
                itertools.takewhile(lambda x:end_label   not in x, all_lines))][1:]
            block = np.loadtxt(block_txt, dtype=np.int32)

            return block

        depot = read_block("DEPOT_SECTION", "EOF")[:-1]
        if len(depot) > 1:
            raise RuntimeError("Cannot handle more than one depot")
        setattr(self, "depot", int(depot[0]))

        print json.dumps(self.args, indent=1)

        node_coords = read_block("NODE_COORD_SECTION", "DEMAND_SECTION")
        demand = read_block("DEMAND_SECTION", "DEPOT_SECTION")
        setattr(self, "node_info", np.vstack((node_coords.T, demand[:,1])).T)

    def __init__(self, description, no_country=False):
        parser = InputParser(description)
        self._parsed = parser.parse_args()
        # already parsed, but always in a list, so just remove it here so its easier to read
        self.args = {}
        for i in self._parsed.__dict__:
            try:
                if len(self._parsed.__dict__[i]) > 1:
                    setattr(self, i, self._parsed.__dict__[i])
                else:
                    if self._parsed.__dict__[i]:
                        setattr(self, i, self._parsed.__dict__[i][0])
            except TypeError as e:
                setattr(self, i, self._parsed.__dict__[i])

        self.total_chromosomes = self.pop_size*self.num_populations

        if self.verbose:
            print self.args

        self.parseProblem()

class InputParser(argparse.ArgumentParser):
    """ adds the country/aep threshold command line arguments, which is used by all the scripts """

    def __init__(self, description):
        super(InputParser, self).__init__(description=description,
            fromfile_prefix_chars='@',
            #formatter_class=argparse.RawTextHelpFormatter, # makes it insanely ugly
            formatter_class=argparse.ArgumentDefaultsHelpFormatter,
            )

        # location of input files for _volcanoes.txt etc
        self.add_argument("--input_file",
                          nargs=1,
                          default=["problems/fruitybun-data.vrp"],
                          help="""Input file in VRP format""",
                          action='store')

        # choose month
        self.add_argument("--arena_size",
                          nargs=1,
                          default=[4],
                          metavar="N",
                          type=int,
                          help="""Size of arena for arena selection. 0 means parents are
                              chosen at random""",
                          action='store')

        self.add_argument("--breed_strategy",
                          nargs=1,
                          default=["CX"],
                          choices=["CX", "PMX", "O1"],
                          help="""Breeding strategy""",
                          action='store')

        self.add_argument("--min_capacity",
                          nargs=1,
                          default=[0],
                          metavar="N",
                          type=int,
                          help="""Capacity in each truck which will cause it to go
                            back to the depot""",
                          action='store')

        self.add_argument("--select_strategy",
                          nargs=1,
                          default=["ELITIST"],
                          choices=["ELITIST", "NON_ELITIST"],
                          help="""Selection strategy""",
                          action='store')

        self.add_argument("--pop_size",
                          nargs=1,
                          default=[128],
                          metavar="N",
                          type=int,
                          help="""Size of each population""",
                          action='store')

        self.add_argument("--num_populations",
                          nargs=1,
                          default=[4],
                          metavar="N",
                          type=int,
                          help="""Number of populations""",
                          action='store')

        self.add_argument("--num_iterations",
                          nargs=1,
                          default=[100],
                          metavar="N",
                          type=int,
                          help="""Number of generations of genetic algorithm""",
                          action='store')

        self.add_argument("--tsp_solve_rate",
                          nargs=1,
                          default=[10],
                          type=int,
                          help="""How often (steps) to do quick heuristic for TSP.
                            0 disables TSP solving for entire run.""",
                          action='store')

        self.add_argument("--mutation_strategy",
                          nargs=1,
                          default=["SWAP"],
                          choices=["SWAP", "SLIDE", "REVERSE"],
                          help="""What kind of mutation strategy to use""",
                          action='store')

        self.add_argument("--num_trucks",
                          nargs=1,
                          default=[13],
                          type=int,
                          help="""Total number of trucks (or, return to depot)
                            possible in one route""",
                          action='store')

        self.add_argument("--mutation_rate",
                          nargs=1,
                          default=[1],
                          type=int,
                          metavar="N",
                          help="""Rate at which to mutate chromosomes""",
                          action='store')

        self.add_argument("--stops_per_route",
                          nargs=1,
                          default=[16],
                          type=int,
                          metavar="N",
                          help="""Stops per reout before going back to depot""",
                          action='store')

        self.add_argument("--ocl_device",
                          nargs=1,
                          default=[0],
                          type=int,
                          metavar="N",
                          help="""Which OpenCL device to use""",
                          action='store')

        self.set_defaults(ocl_profiling=False)
        self.set_defaults(verbose=False)

        self.add_argument("--ocl_profiling",
                          dest='verbose',
                          help="""Enable OpenCL profiling info""",
                          action='store_true')

        self.add_argument("--verbose",
                          dest='verbose',
                          help="""Print out verbose info""",
                          action='store_true')

