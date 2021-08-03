# Copyright (c) 2008 Princeton University
# Copyright (c) 2009 Advanced Micro Devices, Inc.
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
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
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
# Author: Tushar Krishna
#

from m5.params import *
from m5.proxy import *
from Network import RubyNetwork
from BasicRouter import BasicRouter
from ClockedObject import ClockedObject

class GarnetNetwork(RubyNetwork):
    type = 'GarnetNetwork'
    cxx_header = "mem/ruby/network/garnet2.0/GarnetNetwork.hh"
    num_rows = Param.Int(0, "number of rows if 2D (mesh/torus/..) topology");
    ni_flit_size = Param.UInt32(16, "network interface flit size in bytes")
    vcs_per_vnet = Param.UInt32(4, "virtual channels per virtual network");
    # 'spin-ring' parameters:
    spin = Param.UInt32(0, "To enable spin technique")
    drain_all_vc = Param.UInt32(0, "when set all vcs will be drained")
    spin_freq = Param.UInt32(0, "How often are we going to spin")
    spin_mult = Param.UInt32(0,
                 "How many multiple times are we going to spin on its turn")
    conf_file = Param.String("up-down routing configuration file")
    uTurn_crossbar = Param.Int32(1,
                  "If check if uTurns are allowed (1) or not(0). uTurns are " \
                  "allowed by default")
    buffers_per_data_vc = Param.UInt32(4, "buffers per data virtual channel");
    buffers_per_ctrl_vc = Param.UInt32(1, "buffers per ctrl virtual channel");
    routing_algorithm = Param.Int(0,
        "0: Weight-based Table, 1: XY, 2: Custom");
    enable_fault_model = Param.Bool(False, "enable network fault model");
    fault_model = Param.FaultModel(NULL, "network fault model");
    trace_enable = Param.Bool(False, "enable trace simulation");
    trace_file  = Param.String(" ", "network trace input file");
    trace_max_packets = Param.Int(-1, "maximum trace packets to inject");
    garnet_deadlock_threshold = Param.UInt32(50000,
                              "network-level deadlock threshold")
    sim_type = Param.Int(Parent.sim_type, "simulation_type")
    warmup_cycles = Param.Int(Parent.warmup_cycles, "warmup_cycles")
    marked_flits = Param.Int(Parent.marked_flits, "number of marked flits")
    ni_inj = Param.String(Parent.ni_inj,
                "chice of packet injection manner from NIC to network")
    inj_single_vnet = Param.Int(Parent.marked_flits,
                    "when set it will inject all packets ejected from \
                      protocol buffer into single vnet of the network")
    spin_file = Param.String(Parent.spin_file,
					"file path containing SPIN-ring information for DRAIN")

class GarnetNetworkInterface(ClockedObject):
    type = 'GarnetNetworkInterface'
    cxx_class = 'NetworkInterface'
    cxx_header = "mem/ruby/network/garnet2.0/NetworkInterface.hh"

    id = Param.UInt32("ID in relation to other network interfaces")
    vcs_per_vnet = Param.UInt32(Parent.vcs_per_vnet,
                             "virtual channels per virtual network")
    virt_nets = Param.UInt32(Parent.number_of_virtual_networks,
                          "number of virtual networks")
    garnet_deadlock_threshold = Param.UInt32(Parent.garnet_deadlock_threshold,
                                      "network-level deadlock threshold")

class GarnetRouter(BasicRouter):
    type = 'GarnetRouter'
    cxx_class = 'Router'
    cxx_header = "mem/ruby/network/garnet2.0/Router.hh"
    vcs_per_vnet = Param.UInt32(Parent.vcs_per_vnet,
                              "virtual channels per virtual network")
    virt_nets = Param.UInt32(Parent.number_of_virtual_networks,
                          "number of virtual networks")
