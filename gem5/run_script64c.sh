#!/bin/bash
#####	README
###############################################################################
# bench_caps=( 'UNIFORM_RANDOM' 'BIT_COMPLEMENT' 'BIT_REVERSE' 'BIT_ROTATION' 'TRANSPOSE' 'SHUFFLE' )
# bench=( 'uniform_random' 'bit_complement' 'bit_reverse' 'bit_rotation' 'transpose' 'shuffle' )

bench_caps=( 'UNIFORM_RANDOM' 'TRANSPOSE' 'SHUFFLE' )
bench=( 'uniform_random' 'transpose' 'shuffle' )

routing_algorithm=( 'DRAIN_escapeVC' )

conf=$1
rot=$2
spin_freq=$3
cycles=100000
vnet=0 #for multi-flit pkt: vnet = 2
tr=1
################# Give attention to the injection rate that you have got#############################
for b in 0 1 2
do
for vc_ in 2 4
do
for k in 0.02 0.04 0.06 0.08 0.10 0.12 0.14 0.16 0.18 0.20 0.22 0.24 0.26 0.28 0.30 0.32 0.34 0.36 0.38 0.40 0.42 0.44 0.46
do

    ./build/Garnet_standalone/gem5.opt -d 64c/${conf}/${routing_algorithm[0]}/${bench_caps[$b]}/vc-${vc_}/freq-${spin_freq}/rot-${rot}/uTurnCrossbar-1/inj-${k} configs/example/garnet_synth_traffic.py --topology=irregularMesh_XY --num-cpus=64 --num-dirs=64 --mesh-rows=8 --network=garnet2.0 --router-latency=$tr --sim-cycle=$cycles --spin=1 --conf-file=${conf} --spin-file=spin_configs/SR_${conf} --spin-freq=${spin_freq} --spin-mult=${rot} --uTurn-crossbar=1 --inj-vnet=${vnet} --vcs-per-vnet=${vc_} --injectionrate=${k} --synthetic=${bench[$b]} --routing-algorithm=0 &

done
done
done