# -*- coding: utf-8 -*-
# Copyright (c) 2015 Jason Power
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
# Authors: Jason Power

""" This file creates a single CPU and a two-level cache system.
This script takes a single parameter which specifies a binary to execute.
If none is provided it executes 'hello' by default (mostly used for testing)

See Part 1, Chapter 3: Adding cache to the configuration script in the
learning_gem5 book for more information about this script.
This file exports options for the L1 I/D and L2 cache sizes.

IMPORTANT: If you modify this file, it's likely that the Learning gem5 book
           also needs to be updated. For now, email Jason <power.jg@gmail.com>

"""

# import the m5 (gem5) library created when gem5 is built
import m5
# import all of the SimObjects
from m5.objects import *

# Add the common scripts to our path
m5.util.addToPath('../../../')
m5.util.addToPath('../')

# import the caches which we made
from caches import *

# import the SimpleOpts module
from common import SimpleOpts

enc_latency = 0 

ENC_LATENCY = KCIPHER128_LATENCY

# Set the usage message to display
SimpleOpts.set_usage("usage: %prog [options] <binary to execute>")

# Finalize the arguments and grab the opts so we can pass it on to our objects
(opts, args) = SimpleOpts.parse_args()

# get ISA for the default binary to run. This is mostly for simple testing
isa = str(m5.defines.buildEnv['TARGET_ISA']).lower()

# Default to running 'hello', use the compiled ISA to find the binary
#binary = 'tests/test-progs/hello/bin/' + isa + '/linux/hello'
binary = ['fft-int.enc']
input_file = [''] #['-I./lib', 'checkspam.pl', '2500', '5', '25', '11', '150', '1', '1', '1', '1']
output_file = 'fft-int.enc.out'

# Check if there was a binary passed in via the command line and error if
# there are too many arguments
if len(args) == 1:
    enc_latency = args[0]
    print("Encrypted latency: " + enc_latency)

elif len(args) > 1:
    SimpleOpts.print_help()
    m5.fatal("Expected a binary to execute as positional argument")

# create the system we are going to simulate
system = System()

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = clock_rate
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('4GB')] # Create an address range

# Create a simple CPU
#system.cpu = TimingSimpleCPU()
system.cpu = DerivO3CPU(fetchWidth = systemWidth, decodeWidth = systemWidth, renameWidth = systemWidth, dispatchWidth = systemWidth, issueWidth = systemWidth, commitWidth = systemWidth, squashWidth = systemWidth)

#system.cpu.progname = binary[0]
#system.cpu.metadataFileName = binary[0] + '.metadata.bin'
system.metadata = Metadata()
system.metadata.filename = binary[0] + '.metadata.bin'
system.metadata.insfilename = binary[0] + '.taints'
system.metadata.progname = 'fft-int.enc'
system.metadata.libc_start = 0x0016704c
system.metadata.enc_latency = enc_latency = ENC_LATENCY
system.cpu.metadata = system.metadata

# Create an L1 instruction and data cache
system.cpu.icache = L1ICache(opts)
system.cpu.dcache = L1DCache(opts)

# Connect the instruction and data caches to the CPU
system.cpu.icache.connectCPU(system.cpu)
system.cpu.dcache.connectCPU(system.cpu)

# Create a memory bus, a coherent crossbar, in this case
system.l2bus = L2XBar()

# Hook the CPU ports up to the l2bus
system.cpu.icache.connectBus(system.l2bus)
system.cpu.dcache.connectBus(system.l2bus)

# Create an L2 cache and connect it to the l2bus
system.l2cache = L2Cache(opts)
system.l2cache.connectCPUSideBus(system.l2bus)

# Create a memory bus
system.membus = SystemXBar()

# Connect the L2 cache to the membus
system.l2cache.connectMemSideBus(system.membus)

# create the interrupt controller for the CPU
system.cpu.createInterruptController()

# For x86 only, make sure the interrupts are connected to the memory
# Note: these are directly connected to the memory bus and are not cached
if m5.defines.buildEnv['TARGET_ISA'] == "x86":
    system.cpu.interrupts[0].pio = system.membus.master
    system.cpu.interrupts[0].int_master = system.membus.slave
    system.cpu.interrupts[0].int_slave = system.membus.master

# Connect the system up to the membus
system.system_port = system.membus.slave

# Create a DDR3 memory controller
system.mem_ctrl = DRAMSim2()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master



#system.churn = Churn()
#system.churn.cache  = ChurnCache(opts)
#system.churn.mem_side = system.churn.cache.cpu_side
#system.churn.cache.mem_side = system.l2bus.slave
#system.churn.churnSleepCycles = churn_sleep_cycles
#system.churn.cpu = system.cpu

# Create a process for a simple "Hello World" application
process = Process()
system.cpu.process = process
system.cpu.max_insts_any_thread = maxinst

# Set the command
# cmd is a list which begins with the executable (like argv)
process.cmd = binary + input_file
process.output = output_file
# Set the cpu to use the process as its workload and create thread contexts
system.cpu.workload = process
system.cpu.createThreads()

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick %i because %s', m5.curTick(), exit_event.getCause())

