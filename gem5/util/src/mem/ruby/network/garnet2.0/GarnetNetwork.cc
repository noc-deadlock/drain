/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 *          Tushar Krishna
 */


#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"

#include <cassert>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <fstream>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "mem/ruby/network/garnet2.0/CreditLink.hh"
#include "mem/ruby/network/garnet2.0/GarnetLink.hh"
#include "mem/ruby/network/garnet2.0/NetworkInterface.hh"
#include "mem/ruby/network/garnet2.0/NetworkLink.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "sim/sim_exit.hh"

#include "mem/ruby/network/garnet2.0/flit.hh"
//end


using namespace std;
using m5::stl_helpers::deletePointers;

/*
 * GarnetNetwork sets up the routers and links and collects stats.
 * Default parameters (GarnetNetwork.py) can be overwritten from command line
 * (see configs/network/Network.py)
 */

GarnetNetwork::GarnetNetwork(const Params *p)
    : Network(p), Consumer(this)
{
    m_num_rows = p->num_rows;
    m_ni_flit_size = p->ni_flit_size;
    m_vcs_per_vnet = p->vcs_per_vnet;
    m_buffers_per_data_vc = p->buffers_per_data_vc;
    m_buffers_per_ctrl_vc = p->buffers_per_ctrl_vc;
    m_routing_algorithm = p->routing_algorithm;
    warmup_cycles = p->warmup_cycles;
    marked_flits = p->marked_flits;
    marked_flt_injected = 0;
    marked_flt_received = 0;
    marked_pkt_received = 0;
    marked_pkt_injected = 0;
    total_marked_flit_latency = 0;
    total_marked_flit_received = 0;
    max_flit_latency = Cycles(0);
    max_flit_network_latency = Cycles(0);
    max_flit_queueing_latency = Cycles(0);

    min_flit_latency = Cycles(999999); // 1 million
    min_flit_network_latency = Cycles(999999);
    min_flit_queueing_latency = Cycles(999999);

    marked_flit_latency = Cycles(0);
    marked_flit_network_latency = Cycles(0);
    marked_flit_queueing_latency = Cycles(0);
    sim_type = p->sim_type;
    cout << "sim-type: " << sim_type << endl;
    ni_inj = p->ni_inj;
    inj_single_vnet = p->inj_single_vnet;
    cout << "inject single vnet: " << inj_single_vnet;
    pre_drain_deadlock_cycle_idx = 0;
    post_drain_deadlock_cycle_idx = 0;

    //  SPIN-defaults from commandline:
    m_spin = p->spin;
    m_spin_thrshld = p->spin_freq;
    m_spin_mult = p->spin_mult;
    m_conf_file = p->conf_file;
    m_spin_file = p->spin_file;
    m_uTurn_crossbar = p->uTurn_crossbar;
    drain_all_vc = p->drain_all_vc;

    lock = -1;

    if (m_spin) {
        // If ''spin' is set then 'm_spin_thrshld' and 'm_spin_mult' should
        // not be equal to 0. Assert.
        assert(m_spin_thrshld != 0);
        // assert(m_spin_mult != 0); // can be 0 for random number of spins..
        #if (DEBUG_PRINT)
            cout << "***********************************" << endl;
            cout << "SPIN is enabled" << endl;
            cout << "SPIN-threshold is: " << m_spin_thrshld << endl;
            cout << "SPIN-Multiplicity is: " << m_spin_mult << endl;
            cout << "***********************************" << endl;
        #endif
        // populate spinRing:
        init_spinRing();
    }


    m_enable_fault_model = p->enable_fault_model;
    if (m_enable_fault_model)
        fault_model = p->fault_model;

    m_trace_enable = p->trace_enable;
    m_trace_filename = p->trace_file;
    m_trace_max_packets = p->trace_max_packets;

    m_vnet_type.resize(m_virtual_networks);

    for (int i = 0 ; i < m_virtual_networks ; i++) {
        if (m_vnet_type_names[i] == "response")
            m_vnet_type[i] = DATA_VNET_; // carries data (and ctrl) packets
        else
            m_vnet_type[i] = CTRL_VNET_; // carries only ctrl packets
    }

    // record the routers
    for (vector<BasicRouter*>::const_iterator i =  p->routers.begin();
         i != p->routers.end(); ++i) {
        Router* router = safe_cast<Router*>(*i);
        m_routers.push_back(router);

        // initialize the router's network pointers
        router->init_net_ptr(this);
    }

    // record the network interfaces
    for (vector<ClockedObject*>::const_iterator i = p->netifs.begin();
         i != p->netifs.end(); ++i) {
        NetworkInterface *ni = safe_cast<NetworkInterface *>(*i);
        m_nis.push_back(ni);
        ni->init_net_ptr(this);
    }
}


void
GarnetNetwork::init_spinRing()
{

    // cout << "m_spin_config: " << m_spin_config << endl;
    char buff[FILENAME_MAX];
    char* ret = getcwd(buff, FILENAME_MAX);
    assert(ret != NULL);
    std::string current_working_dir(buff);
    // cout << current_working_dir << endl;
    ifstream infile;
    string file = "spin_configs/" + std::string("SR_") + m_conf_file;
    file = m_spin_file;
    //cout << "spin ring configuration file:" << file << endl;
    string data, data1;

    infile.open(file);
    // First entry of spin-ring is always router-0.
	// what if that port is not present in the irregular topology?
	// then it would be spinStruct(0, "East") <-- implement this check.
    spinRing.push_back(spinStruct(-1, "Unknown"));
    if(infile.is_open()) {
        while(!infile.eof()) {
            infile >> data;
            // cout << "data: " << data << endl;
            infile >> data1;
            // cout << "data:" << data << "\t data1: " << data1 << endl;
			if ((data1 == "N") || (data1 == "n"))
				data1 = "North";
			else if ((data1 == "E") || (data1 == "e"))
				data1 = "East";
			else if ((data1 == "S") || (data1 == "s"))
				data1 = "South";
			else if ((data1 == "W") || (data1 == "w"))
				data1 = "West";
			else {
				fatal("Illegal direction name or format\n");
				// assert(0);
			}

			// populating the spinRing structure...
			spinRing.push_back(spinStruct(stoi(data), data1));
		}
    } else {
        fatal("Couldn't open the file: %s \n", file);
    }
    // Based on last router-id put the spinRing[0]..
    // Logic: if the router-id of last spinRing node is 1
    // then spinRing[0].inport_dir_ = "East" else if router-id
    // is == col then spinRing[0].inport_dir_ = "North" else assert(0)
    int lst_indx = spinRing.size() - 1;
    if( spinRing[lst_indx].router_id_ == 1 ) {
        spinRing[0].router_id_ = 0;
        spinRing[0].inport_dir_ = "East";
    }
    else if ( spinRing[lst_indx].router_id_ == m_num_rows ) {
        // because 'm_num_cols' is initialized later in the code
        spinRing[0].router_id_ = 0;
        spinRing[0].inport_dir_ = "North";
    }
    else {
        print_spinRing();
        assert(0);
    }

	spinRing.push_back(spinRing[0]);  // because it's a closed loop structure..
									 // and a ring of node 'N' will have 'n+1' elements
									 // last element is same as first element.
	infile.close();
    // print_spinRing();
    // assert(0);
    return;
}

void
GarnetNetwork::doSpin(int vc_) {
    // go the corresponding router and its input-port vc-0
    // if it has flit take the and put it in the next member
    // of the deque..
    // update -- stats:
    // put asserts: number of pkts present in VC-base ('vc_')
    int spun_pkt_num = 0;
    for (vector<Router*>::const_iterator itr= m_routers.begin();
         itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        for (int inport = 0; inport < router->get_num_inports(); inport++) {
            assert(inport == router->get_inputUnit_ref()[inport]->get_id());
            if (router->get_inputUnit_ref()[inport]->get_direction() != "Local") {
                if(router->get_inputUnit_ref()[inport]->vc_isEmpty(vc_)) {
                } else {
                    spun_pkt_num++;
                }
            }
        }
    }

    // cout << "Packets needed to be spun: " << spun_pkt_num << endl;

    m_total_spins++;
    int num_pkts = 0; // number of packets taken out and inserted must be same.

    // 2-stage credit management
    for (int idx = 0; idx < spinRing.size() - 1; idx++) { // stage to remove flits
        // 1. get the id of the inputUnit in that direction for
        // the given router
        Router* router = m_routers[spinRing[idx].router_id_];

        int inport = router->m_routing_unit\
                                ->m_inports_dirn2idx[spinRing[idx].inport_dir_];
        if(router->get_inputUnit_ref()[inport]->vc_isEmpty(vc_)) {
            // 'idx+1' node alredy populated with 'NULL' for member flit_
            // populate every flit...
            // this just means that flit should be nullptr

            spinRing[idx+1].flit_ = nullptr;
            m_bubble++;

        } else {
            // take this flit out... and put it in the next node
            //////////////////////////////////////////////////
            //
            flit* t_flit = router->get_inputUnit_ref()[inport]->peekTopFlit(vc_);
            std::vector<int> pref_outport = router->m_routing_unit\
                               ->lookupRoutingTable_pref_outport(t_flit->get_vnet(),
                               t_flit->get_route().net_dest);
            spinRing[idx+1].flit_ =
                    router->get_inputUnit_ref()[inport]->getTopFlit(vc_); // ptr-cpy
            num_pkts++;
            int idx_;
            for (idx_ = 0; idx_ < pref_outport.size(); idx_++) {
                // check if the outport leads to router_id
                // present at spinRing[idx+1].router_id_
                // cout << pref_outport[idx_] << " ";
                PortDirection next_hop_dir_ = \
                router->get_outputUnit_ref().at(pref_outport[idx_])->get_direction();
                int pref_router_id = get_upstreamId(next_hop_dir_, router->get_id());
                assert(pref_router_id != -1);
                if (pref_router_id == spinRing[idx+1].router_id_) {
                    // update the 'm_fwd_progress++'
                    m_fwd_progress++;
                    break;
                }
            }

            if (idx_ == pref_outport.size()) {
                // update the 'm_misroute++'
                m_misroute++;
            }


            // update the hops_needed_efore_spin, in the flit here
            assert(spinRing[idx+1].flit_->hops_needed_before_spin == -1);\

            spinRing[idx+1].flit_->hops_needed_before_spin = router\
                                             ->compute_hops_remaining(spinRing[idx+1].flit_);

            // set vc idle:
            router->get_inputUnit_ref()[inport]->set_vc_idle(vc_/*vc-id*/, curCycle());
            // credit management stage-1:
            // from whichever router's input port you are taking out flit..
            // increment the credits in the outVC state of corresponding
            // upstream router.. and update the vc_state for outvc.
            Router* upstream_router = get_upstreamrouter(spinRing[idx].inport_dir_,
                                                            router->get_id());
            assert(upstream_router != nullptr);
            // now you have got the upstream router...mark the outvc0 as IDLE and
            // increment credit.
            PortDirection outportDirn = get_upstreamOutportDirn(spinRing[idx].inport_dir_);
            assert(outportDirn != "Local");
            int outport = upstream_router->m_routing_unit\
                                                    ->m_outports_dirn2idx[outportDirn];

            upstream_router->get_outputUnit_ref()[outport]->increment_credit(vc_);
            upstream_router->get_outputUnit_ref()[outport]->set_vc_state(IDLE_, vc_, curCycle());
        }
    }
    // cout << "Packets actually spun: " << num_pkts << endl;
    assert( spun_pkt_num == num_pkts );
    // cout << "num_pkts spun: " << num_pkts << endl;
    // print_spinRing();
    // print out the spin-ring structure here...

    // Stage-2 of credit management...
    // decrement the credits in corresponding upstream router whenever
    // you insert the flit in the input port of the router, as guided
    // by deque--spinRing. update the vc state as well for both input vc
    // and outvc.
    for (int idx = 1; idx < spinRing.size(); idx++) { // stage to insert flit.

        if(spinRing[idx].flit_ != nullptr) {

            Router* router = m_routers[spinRing[idx].router_id_];

            int inport = router->m_routing_unit\
                                    ->m_inports_dirn2idx[spinRing[idx].inport_dir_];
            assert(inport < router->get_inputUnit_ref().size());
            flit *t_flit;
            t_flit = spinRing[idx].flit_;
            num_pkts--;
            int outport = router->route_compute(t_flit->get_route(),
                    inport, spinRing[idx].inport_dir_);

            t_flit->set_outport(outport);
            assert(outport < router->get_outputUnit_ref().size());

            ///////////////////////////////////////
            PortDirection outdir;
            outdir = router->getOutportDirection(outport);
            t_flit->set_outport_dir(outdir);
            // increment the number of hops here for the flit
            t_flit->increment_hops();
            router->get_inputUnit_ref()[inport]->m_vcs[vc_]->insertFlit(t_flit);

            // stats update:
            assert(t_flit->hops_needed_after_spin == -1);
            t_flit->hops_needed_after_spin = router->compute_hops_remaining(t_flit);

            if (t_flit->hops_needed_after_spin > t_flit->hops_needed_before_spin) {
                m_total_misroute += (t_flit->hops_needed_after_spin -
                                        t_flit->hops_needed_before_spin);
            }
            // reset the stats in flit object
            t_flit->hops_needed_after_spin = -1;
            t_flit->hops_needed_before_spin = -1;
            assert(router->get_inputUnit_ref()[inport]->m_vcs[vc_]->get_state() == IDLE_);
            // set-vc active
            router->get_inputUnit_ref()[inport]->set_vc_active(vc_, curCycle());

            //////////////////////////////////////////
            // decrement-credit from upstream router... and mark out-vc as active
            Router* upstream_router = get_upstreamrouter(spinRing[idx].inport_dir_,
                                                                router->get_id());
            assert(upstream_router != nullptr);
            // Mark outVC-0 of this router as ACTIVE_ and decrement credit.
            PortDirection outportDirn = get_upstreamOutportDirn(spinRing[idx].inport_dir_);
            assert(outportDirn != "Local");
            int upstream_outport = upstream_router->m_routing_unit\
                                        ->m_outports_dirn2idx[outportDirn];
            upstream_router->get_outputUnit_ref()[upstream_outport]->decrement_credit(vc_);
            upstream_router->get_outputUnit_ref()[upstream_outport]->set_vc_state(ACTIVE_,
                                                        vc_, curCycle());


        }
        else {
            // cout << "Not inserting at this iteration number.." << endl;
        }
    }

    assert(num_pkts == 0);
    // scanNetwork();
    // clean ring.
    for(auto itr : spinRing ) {
        itr.flit_ = nullptr; // ptr-cpy
    }

    return;
}


void
GarnetNetwork::print_spinRing() {
    for( int kk=0; kk < spinRing.size(); kk++) {
        cout << "------------" << endl;
        if (spinRing[kk].flit_ != NULL) {
            cout << "[spinRing-Node: "<< kk <<" ] Router-id: " << spinRing[kk].router_id_ \
                << " inport_dir_: " << spinRing[kk].inport_dir_ << " " <<*(spinRing[kk].flit_) << endl;
            cout << *(spinRing[kk].flit_->m_msg_ptr) << endl;
        }
        else {
            cout << "[spinRing-Node: "<< kk <<" ] Router-id: " << spinRing[kk].router_id_ \
                << " inport_dir_: " << spinRing[kk].inport_dir_ << endl;
        }
    }
}


// this api will set flit time for only those flits
// which are present on the network inport and outport
// of the router. Barring injection and ejection ports
// and at vc = vc_
void
GarnetNetwork::set_flit_time(int vc_)
{
    for (vector<Router*>::const_iterator itr= m_routers.begin();
        itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        // in this 'router' set time of flit accordingly for
        // "North"; "East"; "South"; "West" ports accordingly.
        for (int inport = 0; inport < router->get_num_inports(); inport++) {
            if(router->get_inputUnit_ref()[inport]->vc_isEmpty(vc_) == false) {
                PortDirection dirn_ = router->get_inputUnit_ref()[inport]->get_direction();
                if ((dirn_ == "North") || (dirn_ == "South") ||
                    (dirn_ == "East") || (dirn_ == "West")) {
                        flit* t_flit;
                        t_flit = (router->get_inputUnit_ref()[inport]->peekTopFlit(vc_));
                        assert(t_flit != nullptr);
                        // t_flit->set_time(curCycle() + Cycles(2*m_spin_mult));
                        t_flit->advance_stage(SA_, curCycle() + Cycles(2*m_spin_mult));
                }
            }

        }
    }
}

void
GarnetNetwork::wakeup_all_input_unit() {
    for (vector<Router*>::const_iterator itr= m_routers.begin();
        itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        for (int inport = 0; inport < router->get_num_inports(); inport++) {
            router->get_inputUnit_ref()[inport]->wakeup();
        }
    }
}

void
GarnetNetwork::wakeup_all_output_unit() {
    for (vector<Router*>::const_iterator itr= m_routers.begin();
        itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        for (int outport = 0; outport < router->get_num_outports(); outport++) {
            router->get_outputUnit_ref()[outport]->wakeup();
        }
    }
}

void
GarnetNetwork::wakeup() {

}

void
GarnetNetwork::schedule_wakeup(Cycles time) {
    // wake up after times cycles
    scheduleEvent(time);
}


void
GarnetNetwork::init()
{
    Network::init();

    for (int i=0; i < m_nodes; i++) {
        m_nis[i]->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
    }

    // The topology pointer should have already been initialized in the
    // parent network constructor
    assert(m_topology_ptr != NULL);
    m_topology_ptr->createLinks(this);

    // Initialize topology specific parameters
    if (getNumRows() > 0) {
        // Only for Mesh topology
        // m_num_rows and m_num_cols are only used for
        // implementing XY or custom routing in RoutingUnit.cc
        m_num_rows = getNumRows();
        m_num_cols = m_routers.size() / m_num_rows;
        assert(m_num_rows * m_num_cols == m_routers.size());
    } else {
        m_num_rows = -1;
        m_num_cols = -1;
    }

    // FaultModel: declare each router to the fault model
    if (isFaultModelEnabled()) {
        for (vector<Router*>::const_iterator i= m_routers.begin();
             i != m_routers.end(); ++i) {
            Router* router = safe_cast<Router*>(*i);
            int router_id M5_VAR_USED =
                fault_model->declare_router(router->get_num_inports(),
                                            router->get_num_outports(),
                                            router->get_vc_per_vnet(),
                                            getBuffersPerDataVC(),
                                            getBuffersPerCtrlVC());
            assert(router_id == router->get_id());
            router->printAggregateFaultProbability(cout);
            router->printFaultVector(cout);
        }
    }

    const char *trace_filename; //[100];
    trace_filename = m_trace_filename.c_str();
    //strcpy(trace_filename, m_trace_filename);
    char  command_string[512];

    sprintf(command_string,"gunzip -c %s", trace_filename);
    if (( tracefile= popen(command_string, "r")) == NULL){
        printf("Command string is %s\n", command_string);
    }

    // Read first thirteen lines of the trace
    int count = 13;
    char trstring [1000];

    while (count > 0) {
        if ( fgets (trstring , 1000, tracefile) != NULL ) {
            count--;
            //puts (trstring);
        } else {
            break;
        }
    }

/*
    char trstring [1000];
    bool done = false;

    // Read trace up to point when packets start
    while (!done) {
        if ( fgets (trstring , 1000, tracefile) != NULL ) {
            puts (trstring);

            // Step 2: Get tokens

            // array to store memory addresses of the tokens in buf
            const char* first_token;

            first_token = strtok(trstring, " "); // first token
            std::string trace_start = "info";

            if (strcmp(first_token, trace_start.c_str())) {
                done = true;
            }
        } else {
            done = true;
        }
    }
*/
    trace_num_packets_injected = 0;
    trace_num_flits_injected = 0;
    trace_num_flits_received = 0;

    trace_start_time = 0;

    // Initialize next packet
    trace_next_packet.valid = false;
    trace_next_packet.time = Cycles(0);
    trace_next_packet.src_id = -1;
    trace_next_packet.dest_id = -1;

    /*
    // Trace Read
    ifstream fin;
    fin.open("data.txt");
    if (!fin.good())
        exit(0);

    // read each line of the file
    while (!fin.eof())
    {
        // read an entire line into memory
        char buf[MAX_CHARS_PER_LINE];
        fin.getline(buf, MAX_CHARS_PER_LINE);

        // parse the line into blank-delimited tokens
        int n = 0; // loop idex

        // array to store memory addresses of the tokens in buf
        const char* token[MAX_TOKENS_PER_LINE] = {}; // initialize to 0

        // parse the line
        token[0] = strtok(buf, DELIMITER); // first token
        if (token[0]) // zero if line is blank
        {
            for (n = 1; n < MAX_TOKENS_PER_LINE; n++)
            {
                token[n] = strtok(0, DELIMITER); // subsequent tokens
                if (!token[n]) break; // no more tokens
            }
        }

        // process (print) the tokens
        for (int i = 0; i < n; i++) // n = #of tokens
            DPRINTF(NetworkTrace, "Token[%d] = %s\n", i, token[i]);
    }
    */

//    scheduleWakeupAbsolute(curCycle() + Cycles(1));
	Sequencer::gnet = this;
}

void
GarnetNetwork::set_halt(bool val) {
    for (vector<Router*>::const_iterator i= m_routers.begin();
         i != m_routers.end(); ++i) {
        Router* router = safe_cast<Router*>(*i);
        router->halt_ = val;
    }
    return;
}

void
GarnetNetwork::scanNetwork(int vnet) {
    int vc_base = vnet * m_vcs_per_vnet;
    cout << "**********************************************" << endl;
    for (vector<Router*>::const_iterator itr= m_routers.begin();
         itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        cout << "--------" << endl;
        cout << "Router_id: " << router->get_id() << " Cycle: " << curCycle() \
            << endl;
        cout << "~~~~~~~~~~~~~~~" << endl;
        for (int inport = 0; inport < router->get_num_inports(); inport++) {
            // print here the inport ID and flit in that inport...
            cout << "inport: " << inport << " direction: " << router->get_inputUnit_ref()[inport]\
                                                                ->get_direction() << endl;
            assert(inport == router->get_inputUnit_ref()[inport]->get_id());

            for(int vc_ = vc_base; vc_ < vc_base + router->get_vc_per_vnet(); vc_++) {
                cout << "VC: " << vc_ << endl;
                if(router->get_inputUnit_ref()[inport]->vc_isEmpty(vc_)) {
                    cout << "inport is empty" << endl;
                } else {
                    flit *t_flit;
                    t_flit = router->get_inputUnit_ref()[inport]->peekTopFlit(vc_);
                    cout << "flit info in this inport:" << endl;
                    cout << *(t_flit) << endl;
                }
            }

        }
    }

    return;
}



void
GarnetNetwork::scanNetwork() {
    // THIS is to avoid multiple routers printing the
    // same stats in the same cycle
//    if(curCycle() > print_cycle) {
//        print_cycle = curCycle() + Cycles(1);
		cout << "Topology info: (lock: " << lock << " )" << endl;
		for (vector<Router*>::const_iterator itr= m_routers.begin();
			itr != m_routers.end(); ++itr) {
			Router* router = safe_cast<Router*>(*itr);
			cout << " Router: " << router->get_id() << " inports: ";
			for (int inport = 0; inport < router->get_num_inports(); inport++) {
				cout << inport << "(" << router->get_inputUnit_ref()[inport]\
                ->get_direction() << ")  ";
			}
			cout << endl;
		}

        cout << "**********************************************" << endl;
        for (vector<Router*>::const_iterator itr= m_routers.begin();
             itr != m_routers.end(); ++itr) {
            Router* router = safe_cast<Router*>(*itr);
            cout << "--------" << endl;
            cout << "Router_id: " << router->get_id() << " Cycle: " << curCycle() \
                << " Halt: " << router->halt_ << endl;
            cout << "~~~~~~~~~~~~~~~" << endl;
            for (int inport = 0; inport < router->get_num_inports(); inport++) {
                // print here the inport ID and flit in that inport...
                cout << "inport: " << inport << " direction: " << router->get_inputUnit_ref()[inport]\
                                                                    ->get_direction() << endl;
                assert(inport == router->get_inputUnit_ref()[inport]->get_id());
               int limit = inj_single_vnet ? 1 : m_virtual_networks;
                for(int vnet_=0; vnet_ < limit; vnet_++) {
                    for(int vc_ = 0; vc_ < router->get_vc_per_vnet(); vc_++) {
                        cout << "VC: " << vc_ + vnet_*m_vcs_per_vnet << endl;
                        if(router->get_inputUnit_ref()[inport]->vc_isEmpty(vc_ + vnet_*m_vcs_per_vnet)) {
                            cout << "inport is empty" << endl;
                        } else {
                            flit *t_flit;
                            t_flit = router->get_inputUnit_ref()[inport]->peekTopFlit(vc_ + vnet_*m_vcs_per_vnet);
                            cout << "flit info in this inport:" << endl;
                            cout << *(t_flit) << endl;
//                            cout << "Message carried by this flit: " << endl;
//                            cout << *(t_flit->m_msg_ptr) << endl;
                        }
                    }
                }
            }
        }
        cout << "**********************************************" << endl;
        cout << "Link States:" << endl;
    for (vector<Router*>::const_iterator itr= m_routers.begin();
         itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        cout << "--------" << endl;
        cout << "Router_id: " << router->get_id() << " Cycle: " << curCycle() << endl;
        for (int outport = 0; outport < router->get_num_outports(); outport++) {
            // print here the outport ID and flit in that outport Link...
            PortDirection direction_ = router->get_outputUnit_ref()[outport]\
                                                                ->get_direction();
            // cout << "outport: " << outport << " direction: " << direction_ << endl;
            assert(outport == router->get_outputUnit_ref()[outport]->get_id());
            if(router->get_outputUnit_ref()[outport]->m_out_link->linkBuffer->isEmpty()) {
                cout << "outport: " << outport << " direction(link): " << direction_ \
                    << " is EMPTY" << endl;
            } else {
                cout << "outport: " << outport << " direction(link): " << direction_ \
                     << endl;
                flit *t_flit;
                t_flit = router->get_outputUnit_ref()[outport]->m_out_link\
                            ->linkBuffer->peekTopFlit();
                cout << *(t_flit) << endl;
//                cout << "Message carried by this flit: " << endl;
//                cout << *(t_flit->m_msg_ptr) << endl;
            }
        }
        cout << "--------" << endl;
    }

    return;
}

 void
GarnetNetwork::scheduleAll_wakeup() {
     for (vector<Router*>::const_iterator itr= m_routers.begin();
          itr != m_routers.end(); ++itr) {
         Router* router = safe_cast<Router*>(*itr);
         router->schedule_wakeup(Cycles(1));
     }
  return;
 }

 void
GarnetNetwork::scheduleAll_wakeup(uint32_t k) {
     for (vector<Router*>::const_iterator itr= m_routers.begin();
          itr != m_routers.end(); ++itr) {
         Router* router = safe_cast<Router*>(*itr);
         router->schedule_wakeup(Cycles(k));
     }
  return;
 }

 void
GarnetNetwork::scheduleAll_wakeup_next_k_cycles(uint32_t k) {
     for (vector<Router*>::const_iterator itr= m_routers.begin();
          itr != m_routers.end(); ++itr) {
         Router* router = safe_cast<Router*>(*itr);
         for (int ii=0; ii<k; ii++)
             router->schedule_wakeup(Cycles(ii));
     }
  return;
 }


bool
GarnetNetwork::check_mrkd_flt()
{
    int itr = 0;
    for(itr = 0; itr < m_routers.size(); ++itr) {
      if(m_routers.at(itr)->mrkd_flt_ > 0)
          break;
    }

    if(itr < m_routers.size()) {
      return false;
    }
    else {
        if(marked_flt_received < marked_flt_injected )
            return false;
        else
            return true;
    }
}


bool
GarnetNetwork::chck_link_state() {
    bool spin_safe_ = true;
    // cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
    for (vector<Router*>::const_iterator itr= m_routers.begin();
         itr != m_routers.end(); ++itr) {
        Router* router = safe_cast<Router*>(*itr);
        // cout << "--------" << endl;
        // cout << "~~~~~~~~~~~~~~~" << endl;
        // cout << "Router_id: " << router->get_id() << " Cycle: " << curCycle() << endl;
        for (int outport = 0; outport < router->get_num_outports(); outport++) {
            // print here the outport ID and flit in that outport Link...
            PortDirection direction_ = router->get_outputUnit_ref()[outport]\
                                                                ->get_direction();
            // cout << "outport: " << outport << " direction: " << direction_ << endl;
            assert(outport == router->get_outputUnit_ref()[outport]->get_id());
            if(router->get_outputUnit_ref()[outport]->m_out_link->linkBuffer->isEmpty()) {
                /*cout << "outport: " << outport << " direction(link): " << direction_ \
                    << " is EMPTY" << endl;*/
            } else {
                spin_safe_ = false;
                /*cout << "outport: " << outport << " direction(link): " << direction_ \
                    << " is NOT-EMPTY" << endl;
                cout << *(router->get_outputUnit_ref()[outport]->m_out_link\
                            ->linkBuffer->peekTopFlit()) << endl;*/
            }
        }
    }
    // cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
    return (spin_safe_);
}

GarnetNetwork::~GarnetNetwork()
{
    deletePointers(m_routers);
    deletePointers(m_nis);
    deletePointers(m_networklinks);
    deletePointers(m_creditlinks);
}

/*
 * This function creates a link from the Network Interface (NI)
 * into the Network.
 * It creates a Network Link from the NI to a Router and a Credit Link from
 * the Router to the NI
*/

void
GarnetNetwork::makeExtInLink(NodeID src, SwitchID dest, BasicLink* link,
                            const NetDest& routing_table_entry)
{
    assert(src < m_nodes);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);

    // GarnetExtLink is bi-directional
    NetworkLink* net_link = garnet_link->m_network_links[LinkDirection_In];
    net_link->setType(EXT_IN_);
    CreditLink* credit_link = garnet_link->m_credit_links[LinkDirection_In];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection dst_inport_dirn = "Local";
    m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    m_nis[src]->addOutPort(net_link, credit_link, dest);
}

/*
 * This function creates a link from the Network to a NI.
 * It creates a Network Link from a Router to the NI and
 * a Credit Link from NI to the Router
*/

void
GarnetNetwork::makeExtOutLink(SwitchID src, NodeID dest, BasicLink* link,
                             const NetDest& routing_table_entry)
{
    assert(dest < m_nodes);
    assert(src < m_routers.size());
    assert(m_routers[src] != NULL);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);

    // GarnetExtLink is bi-directional
    NetworkLink* net_link = garnet_link->m_network_links[LinkDirection_Out];
    net_link->setType(EXT_OUT_);
    CreditLink* credit_link = garnet_link->m_credit_links[LinkDirection_Out];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection src_outport_dirn = "Local";
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
    m_nis[dest]->addInPort(net_link, credit_link);
}

/*
 * This function creates an internal network link between two routers.
 * It adds both the network link and an opposite credit link.
*/

void
GarnetNetwork::makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                                const NetDest& routing_table_entry,
                                PortDirection src_outport_dirn,
                                PortDirection dst_inport_dirn)
{
    GarnetIntLink* garnet_link = safe_cast<GarnetIntLink*>(link);

    // GarnetIntLink is unidirectional
    NetworkLink* net_link = garnet_link->m_network_link;
    net_link->setType(INT_);
    CreditLink* credit_link = garnet_link->m_credit_link;

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
}

// Total routers in the network
int
GarnetNetwork::getNumRouters()
{
    return m_routers.size();
}

// Get ID of router connected to a NI.
int
GarnetNetwork::get_router_id(int ni)
{
    return m_nis[ni]->get_router_id();
}

int
GarnetNetwork::get_upstreamId( PortDirection inport_dir, int my_id )
{
    int num_cols = getNumCols();
    int upstream_id = -1; // router_id for downstream router
    /*outport direction fromt he flit for this router*/
    if (inport_dir == "East") {
        upstream_id = my_id + 1;
    }
    else if (inport_dir == "West") {
        upstream_id = my_id - 1;
    }
    else if (inport_dir == "North") {
        upstream_id = my_id + num_cols;
    }
    else if (inport_dir == "South") {
        upstream_id = my_id - num_cols;
    }
    else if (inport_dir == "Local") {
        upstream_id = my_id;
        #if (DEBUG_PRINT)
            cout << "inport_dir: " << inport_dir << endl;
        #endif
        // assert(0);
        // return -1;
    }
    else {
        #if (DEBUG_PRINT)
            cout << "inport_dir: " << inport_dir << endl;
        #endif
        assert(0); // for completion of if-else chain
        return -1;
    }

    return upstream_id;
}

Router*
GarnetNetwork::get_upstreamrouter(PortDirection inport_dir, int upstream_id)
{
    int router_id = -1;
    router_id = get_upstreamId( inport_dir, upstream_id);
//    cout << "downstream router-id: " << router_id << endl;
    // assert(router_id >= 0);
    if ((router_id < 0) || (router_id >= getNumRouters()))
        return NULL;
    else
        return m_routers[router_id];
}


PortDirection
GarnetNetwork::get_upstreamOutportDirn( PortDirection inport_dir )
{
    // 'inport_dirn' of the downstream router
    // NOTE: it's Mesh specific
    PortDirection outport_dirn;
    if (inport_dir == "East") {
        outport_dirn = "West";
    } else if (inport_dir == "West") {
        outport_dirn = "East";
    } else if (inport_dir == "North") {
        outport_dirn = "South";
    } else if (inport_dir == "South") {
        outport_dirn = "North";
    } else if (inport_dir == "Local") {
        assert(0); // shouldn't come here,,,
    }

    return outport_dirn;
}


void
GarnetNetwork::regStats()
{
    Network::regStats();

    m_pre_mature_exit
        .name(name() + ".pre_mature_exit");

    total_pre_drain_deadlock
        .name(name() + ".total_pre_drain_deadlock");

    total_post_drain_deadlock
        .name(name() + ".total_post_drain_deadlock");


    m_marked_flt_dist
        .init(m_routers.size())
        .name(name() + ".marked_flit_distribution")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_dist
        .init(m_routers.size())
        .name(name() + ".flit_distribution")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    m_network_latency_histogram
        .init(21)
        .name(name() + ".network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_latency_hist
        .init(100)
        .name(name() + ".flit_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_network_latency_hist
        .init(100)
        .name(name() + ".flit_network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flt_queueing_latency_hist
        .init(100)
        .name(name() + ".flit_queueing_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_marked_flt_latency_hist
        .init(100)
        .name(name() + ".marked_flit_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    m_marked_flt_network_latency_hist
        .init(100)
        .name(name() + ".marked_flit_network_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_marked_flt_queueing_latency_hist
        .init(100)
        .name(name() + ".marked_flit_queueing_latency_histogram")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    total_deadlocks_per_Vnet
        .init(m_virtual_networks)
        .name(name() + ".total_deadlocks_per_vnet")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    total_vnet_pkt_across_deadlocks
        .init(m_virtual_networks)
        .name(name() + ".total_vnet_pkt_across_deadlocks")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    // Packets
    m_packets_received
        .init(m_virtual_networks)
        .name(name() + ".packets_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packets_injected
        .init(m_virtual_networks)
        .name(name() + ".packets_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packet_network_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_network_latency")
        .flags(Stats::oneline)
        ;

    m_packet_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_queueing_latency")
        .flags(Stats::oneline)
        ;

    m_max_flit_latency
        .name(name() + ".max_flit_latency");
    m_max_flit_network_latency
        .name(name() + ".max_flit_network_latency");
    m_max_flit_queueing_latency
        .name(name() + ".max_flit_queueing_latency");
    m_max_marked_flit_latency
        .name(name() + ".max_marked_flit_latency");
    m_max_marked_flit_network_latency
        .name(name() + ".max_marked_flit_network_latency");
    m_max_marked_flit_queueing_latency
        .name(name() + ".max_marked_flit_queueing_latency");

    m_min_flit_latency
        .name(name() + ".min_flit_latency");
    m_min_flit_network_latency
        .name(name() + ".min_flit_network_latency");
    m_min_flit_queueing_latency
        .name(name() + ".min_flit_queueing_latency");


    m_fwd_progress
        .name(name() + ".total_flit_forward_progress");
    m_misroute
        .name(name() + ".total_flit_misroute");
    m_bubble
        .name(name() + ".total_bubble_movement");
    m_num_drain
        .name(name() + ".total_DRAIN_spins");

    m_marked_pkt_received
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_receivced")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_injected
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_network_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_network_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_pkt_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_pkt_queueing_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_packets_received.subname(i, csprintf("vnet-%i", i));
        m_packets_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_received.subname(i, csprintf("vnet-%i", i));
        m_packet_network_latency.subname(i, csprintf("vnet-%i", i));
        m_packet_queueing_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_network_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_pkt_queueing_latency.subname(i, csprintf("vnet-%i", i));

        total_deadlocks_per_Vnet.subname(i, csprintf("vnet-%i", i));
        total_vnet_pkt_across_deadlocks.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_packet_vnet_latency
        .name(name() + ".average_packet_vnet_latency")
        .flags(Stats::oneline);
    m_avg_packet_vnet_latency =
        m_packet_network_latency / m_packets_received;

    m_avg_packet_vqueue_latency
        .name(name() + ".average_packet_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_packet_vqueue_latency =
        m_packet_queueing_latency / m_packets_received;

    m_avg_packet_network_latency
        .name(name() + ".average_packet_network_latency");
    m_avg_packet_network_latency =
        sum(m_packet_network_latency) / sum(m_packets_received);

    m_avg_packet_queueing_latency
        .name(name() + ".average_packet_queueing_latency");
    m_avg_packet_queueing_latency
        = sum(m_packet_queueing_latency) / sum(m_packets_received);

    m_avg_packet_latency
        .name(name() + ".average_packet_latency");
    m_avg_packet_latency
        = m_avg_packet_network_latency + m_avg_packet_queueing_latency;

    m_avg_marked_pkt_network_latency
        .name(name() + ".average_marked_pkt_network_latency");
    m_avg_marked_pkt_network_latency =
        sum(m_marked_pkt_network_latency) / sum(m_marked_pkt_received);

    m_avg_marked_pkt_queueing_latency
        .name(name() + ".average_marked_pkt_queueing_latency");
    m_avg_marked_pkt_queueing_latency =
        sum(m_marked_pkt_queueing_latency) / sum(m_marked_pkt_received);

    m_avg_marked_pkt_latency
        .name(name() + ".average_marked_pkt_latency");
    m_avg_marked_pkt_latency
        = m_avg_marked_pkt_network_latency + m_avg_marked_pkt_queueing_latency;

    // Deadlock detection
    m_pre_drain_deadlock_cycles
        .init(10000)
        .name(name() + ".pre_drain_deadlock_cycles")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_post_drain_deadlock_cycles
        .init(10000)
        .name(name() + ".m_post_drain_deadlock_cycles")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    // Flits
    m_flits_received
        .init(m_virtual_networks)
        .name(name() + ".flits_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flits_injected
        .init(m_virtual_networks)
        .name(name() + ".flits_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flit_network_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_network_latency")
        .flags(Stats::oneline)
        ;

    m_flit_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_queueing_latency")
        .flags(Stats::oneline)
        ;

    m_marked_flt_injected
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_received
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_network_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_network_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
    m_marked_flt_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".marked_flt_queueing_latency")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;


    for (int i = 0; i < m_virtual_networks; i++) {
        m_flits_received.subname(i, csprintf("vnet-%i", i));
        m_flits_injected.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_received.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_injected.subname(i, csprintf("vnet-%i", i));
        m_flit_network_latency.subname(i, csprintf("vnet-%i", i));
        m_flit_queueing_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_network_latency.subname(i, csprintf("vnet-%i", i));
        m_marked_flt_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_flit_vnet_latency
        .name(name() + ".average_flit_vnet_latency")
        .flags(Stats::oneline);
    m_avg_flit_vnet_latency = m_flit_network_latency / m_flits_received;

    m_avg_flit_vqueue_latency
        .name(name() + ".average_flit_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_flit_vqueue_latency =
        m_flit_queueing_latency / m_flits_received;

    m_avg_flit_network_latency
        .name(name() + ".average_flit_network_latency");
    m_avg_flit_network_latency =
        sum(m_flit_network_latency) / sum(m_flits_received);

    m_avg_flit_queueing_latency
        .name(name() + ".average_flit_queueing_latency");
    m_avg_flit_queueing_latency =
        sum(m_flit_queueing_latency) / sum(m_flits_received);

    m_avg_flit_latency
        .name(name() + ".average_flit_latency");
    m_avg_flit_latency =
        m_avg_flit_network_latency + m_avg_flit_queueing_latency;

    m_avg_marked_flt_network_latency
        .name(name() + ".average_marked_flt_network_latency");
    m_avg_marked_flt_network_latency =
        sum(m_marked_flt_network_latency) / sum(m_marked_flt_received);

    m_avg_marked_flt_queueing_latency
        .name(name() + ".average_marked_flt_queueing_latency");
    m_avg_marked_flt_queueing_latency =
        sum(m_marked_flt_queueing_latency) / sum(m_marked_flt_received);

    m_avg_marked_flt_latency
        .name(name() + ".average_marked_flt_latency");
    m_avg_marked_flt_latency
        = m_avg_marked_flt_network_latency + m_avg_marked_flt_queueing_latency;


    // Hops
    m_avg_hops.name(name() + ".average_hops");
    m_avg_hops = m_total_hops / sum(m_flits_received);

    m_marked_avg_hops.name(name() + ".marked_average_hops");
    m_marked_avg_hops = m_marked_total_hops / sum(m_marked_flt_received);

    // Links
    m_total_ext_in_link_utilization
        .name(name() + ".ext_in_link_utilization");
    m_total_ext_out_link_utilization
        .name(name() + ".ext_out_link_utilization");
    m_total_int_link_utilization
        .name(name() + ".int_link_utilization");
    m_average_link_utilization
        .name(name() + ".avg_link_utilization");

    // Drain
    m_total_uturn_request
        .name(name() + ".total_uturn_request");

    m_success_uturn
        .name(name() + ".total_successful_uturns");

    m_total_misroute
        .name(name() + ".total_misroute");

    m_misroute_per_pkt
        .name(name() + ".misroute_per_pkt");
    m_misroute_per_pkt = m_total_misroute / sum(m_flits_received);

    m_total_spins
        .name(name() + ".total_spins");

    m_average_vc_load
        .init(m_virtual_networks * m_vcs_per_vnet)
        .name(name() + ".avg_vc_load")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_fwd_progress_per_drain
        .name(name() + ".flit_forward_progress_per_drain");
    m_fwd_progress_per_drain = m_fwd_progress / m_num_drain;

    m_misroute_per_drain
        .name(name() + ".flit_misroute_per_drain");
    m_misroute_per_drain = m_misroute / m_num_drain;

    m_bubble_per_drain
        .name(name() + ".bubble_movement_per_drain");
    m_bubble_per_drain = m_bubble / m_num_drain;

}

void
GarnetNetwork::collateStats()
{
    RubySystem *rs = params()->ruby_system;
    double time_delta = double(curCycle() - rs->getStartCycle());

    for (int i = 0; i < m_networklinks.size(); i++) {
        link_type type = m_networklinks[i]->getType();
        int activity = m_networklinks[i]->getLinkUtilization();

        if (type == EXT_IN_)
            m_total_ext_in_link_utilization += activity;
        else if (type == EXT_OUT_)
            m_total_ext_out_link_utilization += activity;
        else if (type == INT_)
            m_total_int_link_utilization += activity;

        m_average_link_utilization +=
            (double(activity) / time_delta);

        vector<unsigned int> vc_load = m_networklinks[i]->getVcLoad();
        for (int j = 0; j < vc_load.size(); j++) {
            m_average_vc_load[j] += ((double)vc_load[j] / time_delta);
        }
    }

    // Ask the routers to collate their statistics
    for (int i = 0; i < m_routers.size(); i++) {
        m_routers[i]->collateStats();
    }
}

void
GarnetNetwork::print(ostream& out) const
{
    out << "[GarnetNetwork]";
}

GarnetNetwork *
GarnetNetworkParams::create()
{
    return new GarnetNetwork(this);
}

uint32_t
GarnetNetwork::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    for (unsigned int i = 0; i < m_routers.size(); i++) {
        num_functional_writes += m_routers[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_nis.size(); ++i) {
        num_functional_writes += m_nis[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_networklinks.size(); ++i) {
        num_functional_writes += m_networklinks[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}
