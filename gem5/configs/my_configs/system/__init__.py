# -*- coding: utf-8 -*-
# Copyright (c) 2016 Jason Lowe-Power
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
# Authors: Jason Lowe-Power

import SimpleOpts
import MemConfig

# Define common general options
# Note that not all optionsa are used by all systems. The actually used options
# depend on the system being used.

# Simulator options
SimpleOpts.add_option("--script", default='',
                      help="Script to execute in the simulated system")
SimpleOpts.add_option("--kernel", default='', help="Linux kernel")
SimpleOpts.add_option("--disk-image", default='', help="Disk image")
SimpleOpts.add_option("--second_disk", default='',
                      help="The second disk image to mount (/dev/hdb)")
SimpleOpts.add_option("--checkpoint-dir", default='',
                      help="Checkpoint home directory")
SimpleOpts.add_option("--no_host_parallel", default=False, action="store_true",
                      help="Do NOT run gem5 on multiple host threads "\
                           "(kvm only)")
# FIXME: Understand what this does
SimpleOpts.add_option("--enable_tuntap", action='store_true', default=False,
                      help="Something related to ethernet!!!")

# System options
# Keep in mind that which options are actually used depends on the system
# being created
###############################################################3
#################################################################
SimpleOpts.add_option("--sim_type", type="int", default=1,
                  help="to run the garnet simulation in default mode\
                  or run it in warm-up -- cool-down mode.")

# spin scheme

SimpleOpts.add_option("--dd-thresh", action="store", type="int", default=300,
                  help="""deadlock detection threshold""")

SimpleOpts.add_option("--max-turn-capacity", action="store", type="int", default=100,
                  help="""max turn capacity of probe""")

SimpleOpts.add_option("--enable-sb-placement", action="store", type="int", default=0,
                  help="""enable static bubble placement on mesh""")

SimpleOpts.add_option("--enable-variable-dd", action="store",
                  type="int", default=0,
                  help="to enable routers to have different dd thresholds ")

SimpleOpts.add_option("--enable-rotating-priority", action="store",
                  type="int", default=0,
                  help="rotating router priority for spin scheme ")

# 3-D FBFLY topology options
SimpleOpts.add_option("--fbfly-rows", type="int", default=0,
                  help="the number of rows in the fbfly topology")

SimpleOpts.add_option("--fbfly-cols", type="int", default=0,
                  help="the number of rows in the fbfly topology")

SimpleOpts.add_option("--enable-fbfly-vc-partition", type="int", default=0,
                  help="enable vc partitioning in fbfly for deadlock avoidance")



# Dragon-fly topology options
SimpleOpts.add_option("--dfly-group-size", type="int", default=0,
                  help="group size in dragon-fly topology")

SimpleOpts.add_option("--enable-dfly-dlock-avoidance", type="int", default=0,
                  help="enable deadlock avoidance in dragonfly topology")

# minimal adaptive routing with 1-VC
SimpleOpts.add_option("--staleness-thresh", type="int", default=5,
                  help="staleness threshold for minimal adaptive routing")

# escape-VC
SimpleOpts.add_option("--enable-escape-vc", type="int", default=0,
                  help="enable escape vc to be used with Mesh topology")
###############################################################
##############################################################
SimpleOpts.add_option("--marked-flt-per-node", action="store",
                     type="int", default=1000,
                     help="number of marked flit to be injected per node\
                     total marked flit would then be marked-flit per node *\
                     number of nodes in the network")
###############################################################
SimpleOpts.add_option("-n", "--num_cpus", type="int", default=1,
                      help="Number of cores")
SimpleOpts.add_option("--ruby", action="store_true",
                      help="Enable Ruby memory system")
SimpleOpts.add_option("--cacheline_size", type="int", default=64,
                      help="Size of cache block in bytes")
SimpleOpts.add_option("--l1i_size", type="string", default="32kB",
                      help="Size of L1-I cache")
SimpleOpts.add_option("--l1i_assoc", type="int", default=2,
                      help="Associativity of L1-I cache")
SimpleOpts.add_option("--l1d_size", type="string", default="64kB",
                      help="Size of L1-D cache")
SimpleOpts.add_option("--l1d_assoc", type="int", default=2,
                      help="Associativity of L1-D cache")
SimpleOpts.add_option("--num_l2caches", type="int", default=1,
                      help="Number of L2 caches")
SimpleOpts.add_option("--l2_size", type="string", default="2MB",
                      help="Total size of L2 cache")
SimpleOpts.add_option("--l2_assoc", type="int", default=8,
                      help="Associativity of L1-D cache")
SimpleOpts.add_option("--num_dirs", type="int", default=1,
                      help="Number of directories")
SimpleOpts.add_option("--mem_type", type="choice", default="DDR3_1600_8x8",
                      choices=MemConfig.mem_names(),
                      help="Type of memory to use")
SimpleOpts.add_option("--l3_size", default = '4MB',
                      help="L3 cache size. Default: 4MB")
SimpleOpts.add_option("--l3_banks", default = 4, type = 'int',
                      help="L3 cache banks. Default: 4")
SimpleOpts.add_option("--no_prefetchers", default=False, action="store_true",
                       help="Enable prefectchers on the caches")

# Ruby options
# check configs/ruby/Ruby.py and related files
