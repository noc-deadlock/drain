import os
import subprocess
# import pdb; pdb.set_trace()
# first compile then run
binary = 'build/Garnet_standalone/gem5.opt'
os.system("scons -j15 {}".format(binary))


bench_caps=[ "BIT_ROTATION", "SHUFFLE", "TRANSPOSE" ]
bench=[ "bit_rotation", "shuffle", "transpose" ]
file= [ '16_nodes-connectivity_matrix_0-links_removed_0.txt', '64_nodes-connectivity_matrix_0-links_removed_0.txt', '256_nodes-connectivity_matrix_0-links_removed_0.txt' ]
# file= [ '256_nodes-connectivity_matrix_0-links_removed_0.txt' ]
# bench_caps=[ "BIT_ROTATION" ]
# bench=[ "bit_rotation" ]

routing_algorithm=["ADAPT_RAND_", "UP_DN_", "Escape_VC_UP_DN_"]

num_cores = [16, 64, 256]
num_rows = [4, 8, 16]

# num_cores = [256]
# num_rows = [16]

# os.system('rm -rf ./results')
# os.system('mkdir results')

out_dir = './results_sat_thrpt'
cycles = 10000
vnet = 0
tr = 1
vc_ = [1, 2, 4]  # make this a list
sat_thrpt = []
rout_ = 0
spin_freq = 1024

for c in range(len(num_cores)):
	for b in range(len(bench)):
		for v in range(len(vc_)):
			print ("cores: {2:d} b: {0:s} vc-{1:d}".format(bench_caps[b], vc_[v], num_cores[c]))
			pkt_lat = 0
			injection_rate = 0.02
			low_load_latency = 0.0
			while(pkt_lat < 200.00 ):
				############ gem5 command-line ###########
				os.system("{0:s} -d {1:s}/{2:d}/{4:s}/{3:s}/freq-{7:d}/vc-{5:d}/inj-{6:1.2f} configs/example/garnet_synth_traffic.py --topology=irregularMesh_XY --num-cpus={2:d} --num-dirs={2:d} --mesh-rows={8:d} --network=garnet2.0 --router-latency=1 --sim-cycles={9:d} --spin=1 --conf-file={10:s} --spin-file=spin_configs/SR_{10:s} --spin-freq={7:d} --spin-mult=1 --uTurn-crossbar=1 --inj-vnet=0 --vcs-per-vnet={5:d} --injectionrate={6:1.2f} --synthetic={11:s} --routing-algorithm={12:d} ".format(binary, out_dir, num_cores[c],  bench_caps[b], routing_algorithm[rout_], vc_[v], injection_rate, spin_freq, num_rows[c], cycles, file[c], bench[b], rout_ ))


				# convert flot to string with required precision
				inj_rate="{:1.2f}".format(injection_rate)

				############ gem5 output-directory ##############
				output_dir= ("{0:s}/{1:d}/{3:s}/{2:s}/freq-{6:d}/vc-{4:d}/inj-{5:1.2f}".format(out_dir, num_cores[c],  bench_caps[b], routing_algorithm[rout_], vc_[v], injection_rate, spin_freq))
				print ("output_dir: %s" %(output_dir))

				packet_latency = subprocess.check_output("grep -nri average_flit_latency  {0:s}  | sed 's/.*system.ruby.network.average_flit_latency\s*//'".format(output_dir), shell=True)
				# print packet_latency
				pkt_lat = float(packet_latency)

				print ("injection_rate={1:1.2f} \t Packet Latency: {0:f} ".format(pkt_lat, injection_rate))
				# Code to capture saturation throughput
				if injection_rate == 0.02:
					low_load_latency = float(pkt_lat)
				elif (float(pkt_lat) > 6.0 * float(low_load_latency)):
					sat_thrpt.append(float(injection_rate))
					break

				if float(low_load_latency) > 70.00:
					sat_thrpt.append(float(injection_rate))
					break

				injection_rate+=0.02


############### Extract results here ###############

# Print the list here
for c in range(len(num_cores)):
	for b in range(len(bench)):
		for v in range(len(vc_)):
			print ("cores: {2:d} b: {0:s} vc-{1:d}".format(bench_caps[b], vc_[v], num_cores[c]))
			print sat_thrpt[c*(len(bench)*len(vc_)) + b*(len(vc_)) + v]
