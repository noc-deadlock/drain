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

"""This class represents a mult-core system with Ruby memory system
"""

import m5
from m5.objects import *
from m5.util import convert, addToPath
from fs_tools import *
import SimpleOpts
#from caches import *
#from caches_ruby import MSIRubyCacheSystem

addToPath('../')
from ruby import Ruby

class MyRubySystem(LinuxX86System):

    def __init__(self):
        super(MyRubySystem, self).__init__()
        Ruby.define_options(SimpleOpts.get_parser())

    def createMyRubySystem(self, opts, no_kvm=False):
        #super(MyRubySystem, self).__init__()
        self._opts = opts
        self._no_kvm = no_kvm
        self._tuntap = opts.enable_tuntap

        #self._host_parallel = not self._opts.no_host_parallel
        self._host_parallel = not no_kvm

        self.kernel = self._opts.kernel

        # Physical Memory
        # On the PC platform the range 0xC0000000 to 0xFFFFFFFF is reserved
        # for various devices.
        # Thereofore, if the physical memory size exceeds 3GB (0xC0000000), it
        # it has to be split across two ranges (0 to 3GB and 4GB to total size)
        mem_size = '3GB'
        excess_mem_size = convert.toMemorySize(mem_size) - \
                          convert.toMemorySize('3GB')
        if excess_mem_size <= 0:
            self.mem_ranges = [AddrRange(mem_size)]
        else:
            #warn("Physical memory greater than 3GB. Twice the memory " \
            #     "controllers will be created")
            self.mem_ranges = [AddrRange('3GB'),\
                               AddrRange(Addr('4GB'), size = excess_mem_size)]

        self.initFS(self._opts.num_cpus)

        # Options specified on the kernel command line
        boot_options = ['earlyprintk=ttyS0', 'console=ttyS0', 'lpj=7999923',
                        'root=/dev/hda1']
        self.boot_osflags = ' '.join(boot_options)

        # Set up the clock domain and the voltage domain
        self.clk_domain = SrcClockDomain()
        self.clk_domain.clock = '3GHz'
        self.clk_domain.voltage_domain = VoltageDomain()

        # Create the CPUs for our system.
        self.createCPU()

        # Set up the interrupts controllers for the system (x86 specific)
        #self.setupInterrupts()

        # Create Ruby System
        self.createRubySystem()

        # Set up the interrupts controllers for the system (x86 specific)
        self.setupInterrupts()

        # For KVM
        if self._host_parallel:
            # To get the KVM CPUs to run on different host CPUs
            # Specify a different event queue for each CPU
            for i,cpu in enumerate(self.cpu):
                for obj in cpu.descendants():
                    obj.eventq_index = 0
                cpu.eventq_index = i + 1

    def getHostParallel(self):
        return self._host_parallel

    def totalInsts(self):
        return sum([cpu.totalInsts() for cpu in self.cpu])

    def createCPU(self):
        if self._no_kvm:
            # For main run we start aith atomic to ROI and then switch
            self.cpu = [AtomicSimpleCPU(cpu_id = i, switched_out = False)
                        for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.cpu)
            self.mem_mode = "atomic_noncaching"

            self.timingCpu = [DerivO3CPU(cpu_id = i, switched_out = True)
                              for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.timingCpu)

            # Couldn't get Ruby to switch to SimpleAtomic after DerivO3
            self.cooldownCpu = [TimingSimpleCPU(cpu_id = i,switched_out = True)
                                for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.cooldownCpu)

        else:
            # Note KVM needs a VM and atomic_noncaching
            self.cpu = [X86KvmCPU(cpu_id = i, hostFreq = "3.6GHz")
                        for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.cpu)
            self.kvm_vm = KvmVM()
            self.mem_mode = 'atomic_noncaching'

            self.atomicCpu = [AtomicSimpleCPU(cpu_id = i, switched_out = True)
                        for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.atomicCpu)

            self.timingCpu = [TimingSimpleCPU(cpu_id = i, switched_out = True)
                              for i in range(self._opts.num_cpus)]
            map(lambda c: c.createThreads(), self.timingCpu)
            #self.mem_mode = "timing"

    def switchCpus(self, old, new):
        assert(new[0].switchedOut())
        m5.switchCpus(self, zip(old, new))

    def switchCpusSetMem(self, old, new, mem_mode):
        assert(new[0].switchedOut())
        m5.switchCpusSetMem(self, zip(old, new), mem_mode)

    def setDiskImages(self, img_path_1, img_path_2):
        disk0 = CowDisk(img_path_1)
        disk2 = CowDisk(img_path_2)
        self.pc.south_bridge.ide.disks = [disk0, disk2]

    def createRubySystem(self):
        bootmem = getattr(self, 'bootmem', None)
        Ruby.create_system(self._opts, True, self, self.iobus, self._dma_ports,
                           bootmem)
        self.iobus.master = self.ruby._io_port.slave

        for (i, cpu) in enumerate(self.cpu):
            # Tie the cpu ports to the correct ruby system ports
            cpu.icache_port = self.ruby._cpu_ports[i].slave
            cpu.dcache_port = self.ruby._cpu_ports[i].slave

            # For x86/arm only
            cpu.itb.walker.port = self.ruby._cpu_ports[i].slave
            cpu.dtb.walker.port = self.ruby._cpu_ports[i].slave

    def setupInterrupts(self):
        for (i, cpu) in enumerate(self.cpu):
            # create the interrupt controller CPU and connect to the membus
            cpu.createInterruptController()

            # For x86 only, connect interrupts to the memory
            # Note: these are directly connected to the memory bus and
            #       not cached
            cpu.interrupts[0].pio = self.ruby._cpu_ports[i].master
            cpu.interrupts[0].int_master = self.ruby._cpu_ports[i].slave
            cpu.interrupts[0].int_slave = self.ruby._cpu_ports[i].master

    def createEthernet(self):
        self.pc.ethernet = IGbE_e1000(pci_bus=0, pci_dev=0, pci_func=0,
                                      InterruptLine=1, InterruptPin=1)
        self.pc.ethernet.pio = self.iobus.master
        self.pc.ethernet.dma = self.iobus.slave
        self.tap = EtherTap()
        selfself.tap.tap = self.pc.ethernet.interface

    def initFS(self, cpus):
        # Platform
        self.pc = Pc()

        # Connect Ruby system
        # North Bridge
        self.iobus = IOXBar()
        # add the ide to the list of dma devices that later need to attach to
        # dma controllers
        self._dma_ports = [self.pc.south_bridge.ide.dma]
        self.pc.attachIO(self.iobus, self._dma_ports)

        if self._tuntap:
            self.createEthernet()

        self.intrctrl = IntrControl()

        # Disks
        # Replace these paths with the path to your disk images.
        # The first disk is the root disk. The second could be used for swap
        # or anything else.
        #imagepath = '../gem5_system_files/ubuntu.img'
        imagepath = self._opts.disk_image
        if self._opts.second_disk:
            self.setDiskImages(imagepath, self._opts.second_disk)
        else:
            self.setDiskImages(imagepath, imagepath)

        # Add in a Bios information structure.
        self.smbios_table.structures = [X86SMBiosBiosInformation()]

        # Set up the Intel MP table
        base_entries = []
        ext_entries = []
        for i in range(cpus):
            bp = X86IntelMPProcessor(
                    local_apic_id = i,
                    local_apic_version = 0x14,
                    enable = True,
                    bootstrap = (i ==0))
            base_entries.append(bp)
        io_apic = X86IntelMPIOAPIC(
                id = cpus,
                version = 0x11,
                enable = True,
                address = 0xfec00000)
        self.pc.south_bridge.io_apic.apic_id = io_apic.id
        base_entries.append(io_apic)
        pci_bus = X86IntelMPBus(bus_id = 0, bus_type='PCI   ')
        base_entries.append(pci_bus)
        isa_bus = X86IntelMPBus(bus_id = 1, bus_type='ISA   ')
        base_entries.append(isa_bus)
        connect_busses = X86IntelMPBusHierarchy(bus_id=1,
                subtractive_decode=True, parent_bus=0)
        ext_entries.append(connect_busses)
        pci_dev4_inta = X86IntelMPIOIntAssignment(
                interrupt_type = 'INT',
                polarity = 'ConformPolarity',
                trigger = 'ConformTrigger',
                source_bus_id = 0,
                source_bus_irq = 0 + (4 << 2),
                dest_io_apic_id = io_apic.id,
                dest_io_apic_intin = 16)
        base_entries.append(pci_dev4_inta)

        if self._tuntap:
            # Interrupt assignment for IGbE_e1000 (bus=0,dev=2,fun=0
            pci_dev2_inta = X86IntelMPIOIntAssignment(
                    interrupt_type = 'INT',
                    polarity = 'ConformPolarity',
                    trigger = 'ConformTrigger',
                    source_bus_id = 0,
                    source_bus_irq = 0 + (2 << 2),
                    dest_io_apic_id = io_apic.id,
                    dest_io_apic_intin = 10)
            base_entries.append(pci_dev2_inta)

        def assignISAInt(irq, apicPin):
            assign_8259_to_apic = X86IntelMPIOIntAssignment(
                    interrupt_type = 'ExtInt',
                    polarity = 'ConformPolarity',
                    trigger = 'ConformTrigger',
                    source_bus_id = 1,
                    source_bus_irq = irq,
                    dest_io_apic_id = io_apic.id,
                    dest_io_apic_intin = 0)
            base_entries.append(assign_8259_to_apic)
            assign_to_apic = X86IntelMPIOIntAssignment(
                    interrupt_type = 'INT',
                    polarity = 'ConformPolarity',
                    trigger = 'ConformTrigger',
                    source_bus_id = 1,
                    source_bus_irq = irq,
                    dest_io_apic_id = io_apic.id,
                    dest_io_apic_intin = apicPin)
            base_entries.append(assign_to_apic)
        assignISAInt(0, 2)
        assignISAInt(1, 1)
        for i in range(3, 15):
            assignISAInt(i, i)
        self.intel_mp_table.base_entries = base_entries
        self.intel_mp_table.ext_entries = ext_entries

        entries = \
           [
            # Mark the first megabyte of memory as reserved
            X86E820Entry(addr = 0, size = '639kB', range_type = 1),
            X86E820Entry(addr = 0x9fc00, size = '385kB', range_type = 2),
            # Mark the rest of physical memory as available
            X86E820Entry(addr = 0x100000,
                    size = '%dB' % (self.mem_ranges[0].size() - 0x100000),
                    range_type = 1),
            ]
        # Mark [mem_size, 3GB) as reserved if memory less than 3GB, which
        # force IO devices to be mapped to [0xC0000000, 0xFFFF0000). Requests
        # to this specific range can pass though bridge to iobus.
        if self.mem_ranges[0].size() < 0xC0000000:
            entries.append(X86E820Entry(
                addr = self.mem_ranges[0].size(),
                size='%dB' % (0xC0000000 - self.mem_ranges[0].size()),
                range_type=2))

        # Reserve the last 16kB of the 32-bit address space for m5ops
        entries.append(X86E820Entry(addr = 0xFFFF0000, size = '64kB',
                                    range_type=2))

        # Add the rest of memory.
        entries.append(X86E820Entry(addr = self.mem_ranges[-1].start,
            size='%dB' % (self.mem_ranges[-1].size()),
            range_type=1))

        self.e820_table.entries = entries
