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

""" Script to create checkpoint after booting and exit. The cores run in
    KVM mode and exits when encounters a checkpoint.
    To properly take the checkpoint and be able to boot again and run a new
    script make sure to use the rcs/boot_checkpoint.rcS.
    The default option values should accomplish this and save the checkpoint
    in boot_checkpoints/NUM_CPUS
"""

import sys
import time

import m5
import m5.ticks
from m5.objects import *

sys.path.append('configs/common/') # For the next line...
import SimpleOpts

from system.system_ruby import MyRubySystem
from system.system_classic import MyClassicSystem

# Set the default values of the options to save time passing them through the
# command line
def setDefaults(opts):
    if not opts.script:
        opts.script = "configs/my_configs/rcs/boot_checkpoint.rcS"
    if not opts.kernel:
        opts.kernel = "system_files/binaries/vmlinux-4.8.13"
    if not opts.disk_image:
        opts.disk_image = "system_files/disks/ubuntu-16.04.5.server.img"
    if not opts.checkpoint_dir:
        opts.checkpoint_dir = "boot_checkpoints"

if __name__ == "__m5_main__":

    # If ruby is enabled then add its configuration parameters
    # Keep in mind that all options have to be added BEFORE calling parse_args
    if '--ruby' in sys.argv:
        system = MyRubySystem()
    else:
        system = MyClassicSystem()
    (opts, args) = SimpleOpts.parse_args()
    # Set the defualt values for non-defined options
    setDefaults(opts)

    # Create the system we are going to simulate with kvm
    # FIXME: Checkpointing with ruby doesn't terminate gracefully as the
    #        classical system
    if opts.ruby:
        system.createMyRubySystem(opts, no_kvm = False)
    else:
        system.createMyClassicSystem(opts, no_kvm = False)

    # Read in the script file passed in via an option.
    # This file gets read and executed by the simulated system after boot.
    # Note: The disk image needs to be configured to do this.
    system.readfile = opts.script

    # Set up the root SimObject and start the simulation
    root = Root(full_system = True, system = system,
                time_sync_enable = True, time_sync_period = '1000us')

    if system.getHostParallel():
        # Required for running kvm on multiple host cores.
        # Uses gem5's parallel event queue feature
        # Note: The simulator is quite picky about this number!
        root.sim_quantum = int(1e9) # 1 ms

    # Instantiate all of the objects we've created above
    m5.instantiate()

    globalStart = time.time()

    print("Running the simulation")
    exit_event = m5.simulate()

    # While there is still something to do in the guest keep executing.
    # This is needed since we exit for the ROI begin/end
    while exit_event.getCause() != "m5_exit instruction encountered":
        print("Exited because", exit_event.getCause())

        # If the user pressed ctrl-c on the host, then we really should exit
        if exit_event.getCause() == "user interrupt received":
            print("User interrupt. Exiting")
            break
        elif exit_event.getCause() == "checkpoint":
            checkpoint_dir = opts.checkpoint_dir + "/" + str(opts.num_cpus)
            m5.checkpoint(checkpoint_dir)

        print("Continuing")
        exit_event = m5.simulate()

    print("Normally exiting simulation: m5_exit instruction encountered")
