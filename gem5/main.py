# Hello World program in Python
import os
import sys
import subprocess

# print "Hello World!\n"
# bench_caps=[ "UNIFORM_RANDOM", "BIT_COMPLEMENT", "BIT_REVERSE", "BIT_ROTATION", "TRANSPOSE", "SHUFFLE" ]
bench_caps=[ "UNIFORM_RANDOM", "SHUFFLE", "TRANSPOSE" ]
# bench=[ "uniform_random", "bit_complement", "bit_reverse", "bit_rotation", "transpose", "shuffle" ]
bench=[ "uniform_random", "shuffle", "transpose" ]
routing_algorithm=["ADAPT_RAND_", "UP_DN_", "Escape_VC_UP_DN_"]
# thresh=[ 128, 1024, 4096 ]
# thresh=[ 256 ]
num_cores=64
file=sys.argv[1]
d="03-10-2019"
# out_dir="/usr/scratch2/mayank/drain_micro2019_rslt/SPIN/simType-2/"+d
out_dir="/usr/scratch/mayank/drain_micro2019_rslt/SPIN/simType-1/"+d
cycles=100000
vnet=0
tr=1
# up_dn_=int(sys.argv[2])
# esc_vc_=int(sys.argv[3])
# VC=[ 1, 4 ]
vc_=int(sys.argv[2])
thresh_=int(sys.argv[3])

for b in range(len(bench)):
	print ("b: {0:s} vc-{1:d}".format(bench_caps[b], vc_))
	pkt_lat = 0
	injection_rate = 0.02
	while(pkt_lat < 200.00 and injection_rate < 0.42 ):
		############ gem5 command-line ###########
		os.system("./build/Garnet_standalone/gem5.opt -d {0:s}/{2:s}/{3:s}/TABLE/thresh-{4:d}/vc-{5:d}/inj-{6:1.2f} configs/example/garnet_synth_traffic.py --topology=irregularMesh_XY --num-cpus=64 --num-dirs=64 --mesh-rows=8 --network=garnet2.0  --sim-cycles={7:d} --conf-file={3:s} --vcs-per-vnet={5:d} --inj-vnet=0 --injectionrate={6:1.2f} --synthetic={8:s} --sim-type=1 --enable-spin-scheme=1 --dd-thresh={4:d} --routing-algorithm=table --max-turn-capacity=40 --enable-variable-dd=0 --enable-rotating-priority=1".format(out_dir, num_cores, bench_caps[b], file, thresh_, vc_, injection_rate, cycles, bench[b]))

		# convert flot to string with required precision
		inj_rate="{:1.2f}".format(injection_rate)

		############ gem5 output-directory ##############	
		output_dir=out_dir+"/"+bench_caps[b]+"/"+file+"/TABLE/thresh-"+str(thresh_)+"/vc-"+str(vc_)+"/inj-"+inj_rate

		print ("output_dir: %s" %(output_dir))
		# print("grep -nri average_flit_latency {0:s} | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir))
		# packet_latency = subprocess.check_output("grep -nri average_marked_flt_latency  {0:s}  | sed 's/.*system.ruby.network.average_marked_flt_latency\s*//'".format(output_dir), shell=True)
		packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)
		print packet_latency
		pkt_lat = float(packet_latency)
		# print ("Packet Latency: %f"%((float)packet_latency))
		print ("Packet Latency: %f" %(pkt_lat))
		injection_rate+=0.02
		print ("injection_rate: %1.2f" %(injection_rate))
