# Copyright (c) 2016 Georgia Institute of Technology
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Tushar Krishna

import math
import m5
from m5.objects import *
from m5.defines import buildEnv
from m5.util import addToPath, fatal

def define_options(parser):
    # By default, ruby uses the simple timing cpu
    parser.set_defaults(cpu_type="TimingSimpleCPU")

    parser.add_option("--topology", type="string", default="Crossbar",
                      help="check configs/topologies for complete set")
    parser.add_option("--mesh-rows", type="int", default=0,
                      help="the number of rows in the mesh topology")
    parser.add_option("--network", type="choice", default="simple",
                      choices=['simple', 'garnet2.0'],
                      help="'simple'|'garnet2.0'")
    parser.add_option("--network-trace-enable", action="store_true", default=False,
                       help="enable trace simulation")
    parser.add_option("--network-trace-file", type="string", default=" ",
                       help="name of trace file")
    parser.add_option("--network-trace-max-packets", type="int", default=-1,
                      help="maximum packets in trace to inject")
    parser.add_option("--router-latency", action="store", type="int",
                      default=1,
                      help="""number of pipeline stages in the garnet router.
                            Has to be >= 1.
                            Can be over-ridden on a per router basis
                            in the topology file.""")
    parser.add_option("--link-latency", action="store", type="int", default=1,
                      help="""latency of each link the simple/garnet networks.
                            Has to be >= 1.
                            Can be over-ridden on a per link basis
                            in the topology file.""")
    parser.add_option("--link-width-bits", action="store", type="int",
                      default=128,
                      help="width in bits for all links inside garnet.")
    parser.add_option("--vcs-per-vnet", action="store", type="int", default=4,
                      help="""number of virtual channels per virtual network
                            inside garnet network.""")
    parser.add_option("--routing-algorithm", action="store", type="int",
                      default=0,
                      help="""routing algorithm in network.
                            0: weight-based table
                            1: XY (for Mesh. see garnet2.0/RoutingUnit.cc)
                            2: Custom (see garnet2.0/RoutingUnit.cc""")
    parser.add_option("--network-fault-model", action="store_true",
                      default=False,
                      help="""enable network fault model:
                            see src/mem/ruby/network/fault_model/""")
    parser.add_option("--garnet-deadlock-threshold", action="store",
                      type="int", default=50000,
                      help="network-level deadlock threshold.")
    parser.add_option("--warmup-cycles", action="store",
                      type="int", default=1000,
                      help="number of cycles before marked packets get injected\
                      into the network")
    parser.add_option("--marked-flits", action="store",
                      type="int", default=100000,
                      help="number of marked flits injected into the network\
                      marked packets would be just /k where ther are k-flits\
                      per packet")
    #parser.add_option("--marked-flt-per-node", action="store",
    #                  type="int", default=1000,
    #                  help="number of marked flit to be injected per node\
    #                  total marked flit would then be marked-flit per node *\
    #                  number of nodes in the network")
    parser.add_option("--conf-file", type="string",
                  default="64_nodes-connectivity_matrix_0-links_removed_0.txt",
                  help="check configs/topologies for complete set")
    parser.add_option("--spin-file", type="string",
                    default="SR_64_nodes-connectivity_matrix_0-links_removed_0.txt",
                    help="file path containg SPIN-ring information for DRAIN")
    parser.add_option("--spin", action="store",
                      type="int", default=0,
                      help="""To enable the spin-ing of the ring specified
                      by a structure; present in GarnetNetwork class""")
    parser.add_option("--uTurn-crossbar", action="store",
                      type="int", default=1,
                      help="""if 0 then uTurns are not allowed.. if it has
                      value of 1 in the commandline then uTurns are allowed. They are
                      allowed by default; if not specified""")
    parser.add_option("--spin-freq", action="store",
                      type="int", default=0,
                      help="""Provides the number of cycles after which spin
                      is performed for example after every 100 cycles""")
    parser.add_option("--spin-mult", action="store",
                      type="int", default=0,
                      help="""How many multiple times would spin be performed
                      on its turn as determined by the `spin-freq` knob""")
    parser.add_option("--drain-all-vc", action="store",
                      type="int", default=0,
                      help="""when set to 1 all vcs across vnets will be DRAINed
                      by getting spun by the spin-ring.""")
    parser.add_option("--ni-inj", type="string", default="fcfs",
                      help="'rr'|'fcfs'")
    parser.add_option("--inj-single-vnet", action="store",
                      type="int", default=0,\
                      help="when set it will inject all packets ejected from \
                      protocol buffer into single vnet of the network")

def create_network(options, ruby):

    # Set the network classes based on the command line options
    if options.network == "garnet2.0":
        NetworkClass = GarnetNetwork
        IntLinkClass = GarnetIntLink
        ExtLinkClass = GarnetExtLink
        RouterClass = GarnetRouter
        InterfaceClass = GarnetNetworkInterface

    else:
        NetworkClass = SimpleNetwork
        IntLinkClass = SimpleIntLink
        ExtLinkClass = SimpleExtLink
        RouterClass = Switch
        InterfaceClass = None

    # Instantiate the network object
    # so that the controllers can connect to it.
    network = NetworkClass(ruby_system = ruby, topology = options.topology,
            routers = [], ext_links = [], int_links = [], netifs = [])

    return (network, IntLinkClass, ExtLinkClass, RouterClass, InterfaceClass)

def init_network(options, network, InterfaceClass):

    if options.network == "garnet2.0":
        network.num_rows = options.mesh_rows
        network.vcs_per_vnet = options.vcs_per_vnet
        network.ni_flit_size = options.link_width_bits / 8
        network.routing_algorithm = options.routing_algorithm
        network.garnet_deadlock_threshold = options.garnet_deadlock_threshold
        network.trace_enable  = options.network_trace_enable
        network.trace_file = options.network_trace_file
        network.trace_max_packets = options.network_trace_max_packets
        network.sim_type = options.sim_type
        network.warmup_cycles = options.warmup_cycles
        network.marked_flits = options.marked_flits
        network.ni_inj = options.ni_inj
        network.inj_single_vnet = options.inj_single_vnet
        network.spin_file = options.spin_file

    if options.network == "simple":
        network.setup_buffers()

    if InterfaceClass != None:
        netifs = [InterfaceClass(id=i) \
                  for (i,n) in enumerate(network.ext_links)]
        network.netifs = netifs

    if options.network_fault_model:
        assert(options.network == "garnet2.0")
        network.enable_fault_model = True
        network.fault_model = FaultModel()

    # NOTE: changes here doesn't require re-compilation unless
    # there is dependency in corresponding C/C++ code

    if options.spin == 1:
      assert(options.network == "garnet2.0")
      print "setting spin to: ", options.spin
      network.spin = options.spin

    if options.spin == 1:
      assert(options.network == "garnet2.0")
      print "setting spin-freq to: ", options.spin_freq
      network.spin_freq = options.spin_freq

    if options.spin == 1:
      assert(options.network == "garnet2.0")
      print "setting spin-mult to: ", options.spin_mult
      network.spin_mult = options.spin_mult

    # if options.spin == 1:
    assert(options.network == "garnet2.0")
    print "using spin config file: ", options.conf_file
    network.conf_file = options.conf_file
    # print "using spin-ring conf file: ", options.spin_file
    # network.spin_file = options.spin_file

    if options.spin == 1:
      assert(options.network == "garnet2.0")
      print "Drain-all-vcs: ", options.drain_all_vc
      network.drain_all_vc = options.drain_all_vc

    if options.spin == 1:
      assert(options.network == "garnet2.0")
      print "setting uTurn-crossbar: ", options.uTurn_crossbar
      network.uTurn_crossbar = options.uTurn_crossbar
