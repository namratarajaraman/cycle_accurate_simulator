#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_proc.h"
#include<vector>
using namespace std;

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim 256 32 4 gcc_trace.txt
    argc = 5
    argv[0] = "sim"
    argv[1] = "256"
    argv[2] = "32"
    ... and so on
*/

//--GLOBALS--
proc_params params;
vector <instruction> inst_list; //instruction list
vector <instruction> fetched_inst_list;
vector<FUNCTIONAL_UNIT> execute_list;
vector <reg> RMT (67);
vector <reg> ARF (67);

unsigned int gclock=0; //global clock variable to advance simulation
//pipeline registers
PIPELINE_REGISTER DE;
PIPELINE_REGISTER RN;
PIPELINE_REGISTER RR;
PIPELINE_REGISTER DI;
PIPELINE_REGISTER WB;

//reorder buffer and issue queue
REORDER_BUFFER ROB;
ISSUE_QUEUE IQ;

//retired instructions list
unsigned int retired_instructions=0;

int num_instructions=0;
bool unusual_bundle=true;

//---------------FETCH----------------
void fetch(){
    bool dont_fetch=false;
    unsigned int temp_list=0;
    if(inst_list.empty() || !DE.is_empty()){
        dont_fetch=true;
    }

    if(!dont_fetch){
        if(inst_list.size()>=params.width) {
            //there are enough instructions left for a WIDTH-size bundle
            for (int i = 0; i < params.width; i++) {
                if (fetched_inst_list[inst_list[i].number].FE == -1) {
                    fetched_inst_list[inst_list[i].number].FE = gclock;
                }
                //fetch up to WIDTH instructions from the trace file into DE.
                DE.register_data[i].inst = inst_list[i];
                DE.register_data[i].valid=1;
            }
            inst_list.erase(inst_list.begin(), inst_list.begin()+params.width);
        }

        else{ //fewer than WIDTH instructions left
            temp_list=inst_list.size();
            for(int i=0; i<temp_list; i++){
                //cout<<"Inst list number: "<<i<<endl;
                if (fetched_inst_list[inst_list[0].number].FE == -1) {
                    fetched_inst_list[inst_list[0].number].FE = gclock;
                }
                DE.register_data[i].inst = inst_list[0];
                //cout<<"Current DE.register_data index is: "<<i<<" with value "<<inst_list[0].number<<endl;
                DE.register_data[i].valid=1;
                inst_list.erase(inst_list.begin());
            }
        }
    }
}

//---------------DECODE----------------
void decode(){
    // If DE contains a decode bundle:
    if(!DE.is_empty()){
        // If RN is empty (can accept a new rename
// bundle), then advance the decode bundle
// from DE to RN.
        for(int k=0; k<DE.register_data.size(); k++){
            if (fetched_inst_list[DE.register_data[k].inst.number].DE == -1) {
                fetched_inst_list[DE.register_data[k].inst.number].DE = gclock;
            }
        }
        if(RN.is_empty()) {
            for(int i=0; i<DE.register_data.size(); i++){
                //************CHECK IF VALUE IS ALREADY IN NEXT PIPELINE*************
                for(int j=0; j<RN.register_data.size(); j++){
                    if(DE.register_data[j].valid == 1) {
                        RN.register_data[i] = DE.register_data[i];
                        RN.register_data[i].valid = 1;
                        DE.register_data[i].valid = 0;
                    }
                }

            }
        }
    }
}

//---------------RENAME----------------
void rename() {
    instruction inst;
    reorder_entry new_entry;
    bool dont_rename = false;
    unsigned int rob_tag = 0;
    unsigned int numofinsts=0;
    //cout << "Enter rename stage : " << endl;
    for(int i=0; i<RN.register_data.size(); i++){ //find number of instructions in DE bundle
        if(RN.register_data[i].valid == 1){
            if (fetched_inst_list[RN.register_data[i].inst.number].RN == -1) {
                fetched_inst_list[RN.register_data[i].inst.number].RN = gclock;
            }
            numofinsts++;
        }
    }

    if (numofinsts > 0) { // If RN contains a rename bundle:
        if ((!RR.is_empty()) || (ROB.rob.free_space() < numofinsts)) {
            //If either RR is not empty (cannot accept
            // a new register-read bundle) or the ROB
            // does not have enough free entries to
            // accept the entire rename bundle, then do
            // nothing.
            dont_rename = true;
        }

        if (!dont_rename) { //If RR is empty (can accept a new
            // register-read bundle) and the ROB has enough free entries to accept the entire rename bundle
            //cout<<"gclock: "<<gclock<<" ROB space available, continue"<<endl;
            for (int i = 0; i < RN.register_data.size(); i++) {
                // (1) allocate an entry in the ROB for the instruction
                if (RN.register_data[i].valid == 1) {
                    //if (fetched_inst_list[RN.register_data[i].inst.number].RN == -1) {
                        //fetched_inst_list[RN.register_data[i].inst.number].RN = gclock;
                    //}
                    new_entry.pc = RN.register_data[i].inst.number;
                    new_entry.valid = 1;
                    new_entry.dst = RN.register_data[i].inst.dest;
                    new_entry.mis = 0;
                    new_entry.exc = 0;
                    new_entry.rdy = 0;
                    new_entry.inst=RN.register_data[i].inst;

                    rob_tag = ROB.rob.enQueue(new_entry); //allocates new entry in ROB
                    if (rob_tag == 0xffffffff) {
                        //printf("ROB ENQ ERROR!! Exiting program\n");
                        exit(0);
                    }

                    //(2) rename its source registers
                    //rename source 1
                    if (RN.register_data[i].inst.source1 != -1) {
                        // if (RMT[RN.register_data[i].inst.source1].ROB_tag >= 100) {
                        RN.register_data[i].inst.source1 = RMT[RN.register_data[i].inst.source1].ROB_tag; //renames to ROB tag
                        // }
                    }


                    //rename source 2
                    if (RN.register_data[i].inst.source2 != -1) {
                        //if (RMT[RN.register_data[i].inst.source2].ROB_tag >= 100) {
                        RN.register_data[i].inst.source2 = RMT[RN.register_data[i].inst.source2].ROB_tag; //renames to ROB tag
                        // }
                    }

                    //put corresponding ROB tag into RMT and validate
                    if (new_entry.dst != -1) {
                        RMT[new_entry.dst].ROB_tag = rob_tag;
                        RMT[new_entry.dst].valid = true;
                    }

                    //and (3) rename its destination register (if it has one).
                    if (RN.register_data[i].inst.dest != -1) {
                        //  if (RMT[RN.register_data[i].inst.dest].ROB_tag >= 100) {
                        RN.register_data[i].inst.dest = RMT[RN.register_data[i].inst.dest].ROB_tag; //renames to ROB tag
                        //  }
                    }
                }
            }

            //advance bundle to RR
            if (RR.is_empty()) {
                for (int i = 0; i < RR.register_data.size(); i++) {
                    if (RN.register_data[i].valid == 1) {
                        RR.register_data[i] = RN.register_data[i];
                        RR.register_data[i].valid = 1;
                        RN.register_data[i].valid = 0;
                    }
                }
            }
        }
    }

    /*cout<<"gclock: "<<gclock<<" ROB display queue : "<<endl;
    ROB.rob.displayQueue();
    cout<<"RMT display registers : "<<endl;

    for(int i=0; i<RMT.size(); i++){
        if(RMT[i].ROB_tag >=100){
            cout<<i<<" "<<RMT[i].ROB_tag<<" "<<RMT[i].valid<<endl;}
    }
    cout<<"gclock: "<<gclock<<" Rename stage : RR contents : "<<endl;
    for(int i=0; i<RR.register_data.size(); i++)
        if (RR.register_data[i].valid)
        cout<<"Number: "<<RR.register_data[i].inst.number<<" DEST: "<<RR.register_data[i].inst.dest<<" SRC 1 AND 2: "<<RR.register_data[i].inst.source1<<" "<<RR.register_data[i].inst.source2<<" SRC 1 AND 2 readiness: "<<RR.register_data[i].inst.src1_ready<<" "<<RR.register_data[i].inst.src1_ready<<" Latency: "<<RR.register_data[i].inst.latency<<endl;

    cout<<"gclock: "<<gclock<<" exited rename stage "<<endl; */
}

void regRead(){
    //cout << "Enter regread stage : " << endl;
    if(!RR.is_empty()) { // If RR contains a register-read bundle
        for(int k=0; k<RR.register_data.size(); k++) {
            if (RR.register_data[k].valid && fetched_inst_list[RR.register_data[k].inst.number].RR == -1) {
                fetched_inst_list[RR.register_data[k].inst.number].RR = gclock;
            }
        }
        if (DI.is_empty()) { //if DI can accept a new bundle
            //ascertain the readiness of the renamed source operands.
            //if the source operand is >=100, it's a rob tag and not a ready value.
            for(int i=0; i<RR.register_data.size(); i++){
                //if(fetched_inst_list[RR.register_data[i].inst.number].RR == -1) {
                    //fetched_inst_list[RR.register_data[i].inst.number].RR = gclock;
                //}
                if((RR.register_data[i].inst.source1 >= 100) && (RR.register_data[i].inst.src1_ready == 0)){ //is in the ROB and needs to be rdy checked
                    if(ROB.rob.arr[(RR.register_data[i].inst.source1)-100].rdy == 1){
                        RR.register_data[i].inst.src1_ready=1;
                    }
                    else{
                        RR.register_data[i].inst.src1_ready=0;
                    }
                }
                else{RR.register_data[i].inst.src1_ready=1;} //else it is in the arf and therefore ready

                if((RR.register_data[i].inst.source2 >= 100) && (RR.register_data[i].inst.src2_ready == 0)){
                    if(ROB.rob.arr[(RR.register_data[i].inst.source2)-100].rdy == 1){
                        RR.register_data[i].inst.src2_ready=1;
                    }
                    else{RR.register_data[i].inst.src2_ready=0;}
                }
                else{RR.register_data[i].inst.src2_ready=1;} //else it is in the arf and therefore ready
            }
        }

        //advance bundle to DI
        if(DI.is_empty()) {
            for (int i = 0; i < RR.register_data.size(); i++) {
                if(RR.register_data[i].valid == 1) {
                    DI.register_data[i] = RR.register_data[i];
                    DI.register_data[i].valid = 1;
                    RR.register_data[i].valid = 0;
                }
            }
        }
    }

    /*cout<<"gclock: "<<gclock<<" Register read stage : DI contents : "<<endl;
    for(int i=0; i<DI.register_data.size(); i++){
        if (DI.register_data[i].valid)
        cout<<"Number: "<<DI.register_data[i].inst.number<<" DEST: "<<DI.register_data[i].inst.dest<<" SRC 1 AND 2: " <<DI.register_data[i].inst.source1<<" "<<DI.register_data[i].inst.source2<<" SRC 1 AND 2 readiness: "<<DI.register_data[i].inst.src1_ready<<" "<<DI.register_data[i].inst.src2_ready<<" Latency: "<<DI.register_data[i].inst.latency<<endl;
    }
    cout<<"gclock: "<<gclock<<" exited regread"<<endl;*/
}

void dispatch(){
    bool dont_dispatch = false;
    unsigned int rob_tag = 0;
    unsigned int numofinsts=0;
    IQ_entry new_entry;

    //cout << " Entered dispatch stage : " << endl;
    for(int i=0; i<DI.register_data.size(); i++){ //find number of instructions in DI bundle
        if(DI.register_data[i].valid == 1){
            if(fetched_inst_list[DI.register_data[i].inst.number].DI == -1) {
                fetched_inst_list[DI.register_data[i].inst.number].DI = gclock;
            }
            numofinsts++;
        }
    }

    if (numofinsts > 0) { // If DI contains a rename bundle:
        if (IQ.iq.free_space() < numofinsts) {
            //cout<<"gclock: "<<gclock<<" IQ has "<<IQ.iq.free_space()<<" spaces for only "<<numofinsts<<" instructions. nope"<<endl;
            dont_dispatch = true;
        }
        //If the number of
        // free IQ entries is greater than or equal
        // to the size of the dispatch bundle in DI,
        if(!dont_dispatch){
            // then dispatch all instructions from DI to the IQ.
            for(int i=0; i<DI.register_data.size(); i++){
                if(DI.register_data[i].valid==1) {
                    //if(fetched_inst_list[DI.register_data[i].inst.number].DI == -1) {
                        //fetched_inst_list[DI.register_data[i].inst.number].DI = gclock;
                    //}
                    new_entry.valid = 1;
                    new_entry.inst=DI.register_data[i].inst;
                    //destination tag
                    if(DI.register_data[i].inst.dest == -1){
                        new_entry.dest_tag = DI.register_data[i].inst.rob_tag;
                    }
                    else {
                        new_entry.dest_tag = DI.register_data[i].inst.dest;
                    }

                    //ready and tag
                    //tag
                    new_entry.rs1_tag = DI.register_data[i].inst.source1;
                    new_entry.rs2_tag = DI.register_data[i].inst.source2;

                    //ready
                    //source 1
                    new_entry.rs1_ready = DI.register_data[i].inst.src1_ready;
                    //source 2
                    new_entry.rs2_ready = DI.register_data[i].inst.src2_ready;

                    IQ.iq.enQueue(new_entry); //add to queue
                    DI.register_data[i].valid=0; //scrub the valid
                }
            }
        }
    }
    /*cout<<"gclock: "<<gclock<<" printing IQ"<<endl;
    IQ.iq.displayQueue();
    cout<<"gclock: "<<gclock<<" exited dispatch"<<endl;*/
}

void issue(){
    // Issue up to WIDTH oldest instructions
    // from the IQ. (One approach to implement
    // oldest-first issuing, is to make multiple
    // passes through the IQ, each time finding
    // the next oldest ready instruction and
    // then issuing it.
    vector<IQ_entry> issuable_instructions;  //list of issuable instructions
    IQ_entry entry;
    unsigned int issuable_inst_count=0;
    int oldest_ready_entry_index=-1;
    bool found_fu_entry = false;

    //cout << "Entered issue stage : " << endl;
    //find all issuable instructions

    for (int i = 0; i < IQ.iq.size; i++) { //number of issuable instructions
        if((IQ.iq.arr[i].valid==1) && (fetched_inst_list[IQ.iq.arr[i].inst.number].IQ == -1)) {
            fetched_inst_list[IQ.iq.arr[i].inst.number].IQ = gclock;
        } //if they just landed in issue queue
        if (IQ.iq.arr[i].rs1_ready && IQ.iq.arr[i].rs2_ready && IQ.iq.arr[i].valid) {
            //cout<<"instruction can be issued"<<endl;
            issuable_inst_count++;
            IQ.iq.arr[i].ready = 1;
        } else {
            IQ.iq.arr[i].ready = 0;
        }
    }

    if (issuable_inst_count > params.width) {issuable_inst_count = params.width;}

    for (int j = 0; j < issuable_inst_count; j++) {
        //cout<<"gclock: "<<gclock<<"finding oldest instructin:"<<endl;
        oldest_ready_entry_index = -1;
        //find oldest instructions
        for (int i = 0; i < IQ.iq.size; i++) {
            //cout<<"Instruction: "<<IQ.iq.arr[i].inst.number<<" issue readiness: "<<IQ.iq.arr[i].rs1_ready<<" "<<IQ.iq.arr[i].rs2_ready<<" Valid: "<<IQ.iq.arr[i].valid<<"Ready: "<<IQ.iq.arr[i].ready<<endl;
            if (IQ.iq.arr[i].valid == 1) {
                //cout<<"Valid instruction: "<<IQ.iq.arr[i].inst.number<<" issue readiness: "<<IQ.iq.arr[i].rs1_ready<<" "<<IQ.iq.arr[i].rs2_ready<<" Valid: "<<IQ.iq.arr[i].valid<<"Ready: "<<IQ.iq.arr[i].ready<<endl;
                //if (oldest_valid_entry_index == -1) oldest_valid_entry_index = i;
                if (IQ.iq.arr[i].ready == 1) {
                    if (oldest_ready_entry_index == -1) oldest_ready_entry_index = i;
                    else if (IQ.iq.arr[i].inst.number < IQ.iq.arr[oldest_ready_entry_index].inst.number) { //
                        oldest_ready_entry_index = i;
                    }
                }
            }
        }
        if (oldest_ready_entry_index != -1) {
            //cout << "Oldest issuable instruction: " << IQ.iq.arr[oldest_ready_entry_index].inst.number << endl;
            issuable_instructions.push_back(IQ.iq.arr[oldest_ready_entry_index]);
            IQ.iq.arr[oldest_ready_entry_index].valid = 0;
            IQ.delete_inst(oldest_ready_entry_index);
        }
    }


    // To issue an instruction:
    /*
    cout<<"gclock: "<<gclock<<" ISSUABLE INSTRUCTION LIST: "<<endl;
    for(int i=0; i<issuable_instructions.size(); i++){
        cout<<"Valid: "<<issuable_instructions[i].valid<<" Instruction number: "<<issuable_instructions[i].inst.number<<" ready bits: "<<issuable_instructions[i].rs1_ready<< " "<<issuable_instructions[i].rs2_ready<<endl;
    }*/

    for (int i = 0; i < issuable_instructions.size(); i++) { //********ISSUE ONLY WIDTH INSTRUCTIONS TO THE EXECUTE LIST!!!!!!!!!!!
        // 2) Add the instruction to the
        // execute_list.
        found_fu_entry = false;

        for (int j = 0; j < execute_list.size(); j++) {
            for (int k = 0; k < execute_list[j].fu_arr.size(); k++) {
                if (execute_list[j].fu_arr[k].valid == 0) {
                    execute_list[j].fu_arr[k].instr = issuable_instructions[i].inst;
                    execute_list[j].fu_arr[k].valid = true;
                    found_fu_entry = true;
                }
                if (found_fu_entry) break;
            }
            if (found_fu_entry) break;
        }
    }

    issuable_instructions.clear(); //clear for new instructions
    //cout<<"gclock: "<<gclock<<" Execute list: "<<endl;
    /*
    for(int i=0; i<execute_list.size(); i++){

        for(int j=0; j<execute_list[i].fu_arr.size(); i++){
            if (execute_list[i].fu_arr[j].valid)
                cout<<"valid:"<<execute_list[i].fu_arr[j].valid<<" inst number: "<<execute_list[i].fu_arr[j].instr.number<<endl;
        }
    }
    cout<<"gclock: "<<gclock<<" ISSUE QUEUE: "<<endl;
    IQ.iq.displayQueue();
    cout<<"gclock: "<<gclock<<" issue done"<<endl;

cout << "Exited issue stage :" << endl; */
}

void execute(){
    // From the execute_list, check for
    // instructions that are finishing
    // execution this cycle, and:
    // 1) Remove the instruction from
    // the execute_list.
    // 2) Add the instruction to WB.
    //cout << "Entered execute stage :" << endl;
    for(int i=0; i<execute_list.size(); i++) {
        for(int j=0; j<execute_list[i].fu_arr.size(); j++){
            if(execute_list[i].fu_arr[j].valid == 1 && fetched_inst_list[execute_list[i].fu_arr[j].instr.number].EX == -1) {
                fetched_inst_list[execute_list[i].fu_arr[j].instr.number].EX = gclock;
            }
        }
        execute_list[i].per_cycle_processing();

        // 3) Wakeup dependent instructions (set
        // their source operand ready flags) in
        // the IQ, DI (the dispatch bundle), and
        // RR (the register-read bundle).
    }

    /*cout<<"gclock: "<<gclock<<" Execute stage :  writeback reg:"<<endl;
    for(int i=0; i<WB.register_data.size(); i++){
        if (WB.register_data[i].valid)
        cout<<"Number: "<<WB.register_data[i].inst.number<<" DEST: "<<WB.register_data[i].inst.dest<<" SRC 1 AND 2 readiness: "<<WB.register_data[i].inst.src1_ready<<" "<<WB.register_data[i].inst.src2_ready<<" Latency: "<<WB.register_data[i].inst.latency<<endl;
    }
    cout << "Exited execute stage" << endl;*/
}

void writeback(){
    // For each instruction in WB, mark the
    // instruction as “ready” in its entry in
    // the ROB.
    //cout<<"Enter writeback : "<<endl;
    if(ROB.rob.front != -1) {
        for (int i = 0; i < WB.register_data.size(); i++) {
            if(WB.register_data[i].valid && fetched_inst_list[WB.register_data[i].inst.number].WB == -1) {
                fetched_inst_list[WB.register_data[i].inst.number].WB = gclock;
            }
            if (WB.register_data[i].valid) {
                for (int j = 0; j < ROB.rob.size; j++) {
                    if ((ROB.rob.arr[j].valid) && (ROB.rob.arr[j].pc == WB.register_data[i].inst.number)) {
                        ROB.rob.arr[j].rdy = 1;
                        break;
                    }
                }
                WB.register_data[i].valid = 0;
            }
        }
        /*
        cout<<"gclock: "<<gclock<<" Writeback : ROB QUEUE: "<<endl;
        ROB.rob.displayQueue();*/
    }

    else{/*cout<<"no rob entries, in writeback, moving on..."<<endl;*/}
    //cout << "gclock: "<<gclock<<" finished writeback :" << endl;
}

void retire(){
    // Retire up to WIDTH consecutive
    // “ready” instructions from the head of
    // the ROB.
    //cout<<"gclock: "<<gclock<<" Enter retire stage : "<<endl;
    //cout<<params.width<<endl;

    for(int i=0; i<ROB.rob.size; i++) {
        if (ROB.rob.arr[i].valid == 1 && fetched_inst_list[ROB.rob.arr[i].inst.number].retire == -1 && ROB.rob.arr[i].rdy == 1) {
            //cout<<"yuh"<<endl;
            fetched_inst_list[ROB.rob.arr[i].inst.number].retire = gclock;
        }
    }


    if(ROB.rob.front != -1) {
        for (int i = 0; i < params.width; i++) {
            //cout << ROB.rob.arr[ROB.rob.front].rdy << endl;
            if(ROB.rob.front != -1) {
                if (ROB.rob.arr[ROB.rob.front].rdy == 1) { //is head ready for retirement?
                    for (int j = 0; j < RMT.size(); j++) {
                        if (RMT[j].ROB_tag == ROB.rob.arr[ROB.rob.front].rob_tag) {
                            //cout<<j<<" rob tag of "<<RMT[j].ROB_tag<<" being replaced"<<endl;
                            RMT[j].ROB_tag = j;
                        }
                    }
                    fetched_inst_list[ROB.rob.arr[ROB.rob.front].inst.number].ret_end = gclock;
                    ROB.rob.deQueue();
                } else {
                    break;
                }
            }
        }

        //cout<<"gclock: "<<gclock<<" Retire : ROB:"<<endl;
        ROB.rob.displayQueue();
    }

    else{
        //cout<<"queue empty. moving on..."<<endl;
    }
    //cout << "Exiting retire stage : " << endl;
}

//--ADVANCE CYCLE FUNCTION--
bool advance_cycle(){
    gclock++;
    //go through all stages and check for valid bits :/
    bool keep_going=true; //assume pipeline is not empty until proven otherwise
    if(RN.is_empty() && RR.is_empty() && DI.is_empty() && WB.is_empty() && IQ.is_empty() && ROB.is_empty() &&(inst_list.size()==0)){
        keep_going = false;
    }
    return keep_going;
}

void print_timing_info(){
    for(int i=0; i< fetched_inst_list.size(); i++){
        cout<<i<<" fu{"<<fetched_inst_list[i].operation<<"} src{"<<fetched_inst_list[i].source1<<","<<fetched_inst_list[i].source2<<"} "<<"dst{"<<fetched_inst_list[i].dest<<"} FE{"<<fetched_inst_list[i].FE<<","<<fetched_inst_list[i].DE-fetched_inst_list[i].FE<<"} DE{"<<fetched_inst_list[i].DE<<","<<fetched_inst_list[i].RN-fetched_inst_list[i].DE<<"} RN{"<<fetched_inst_list[i].RN<<","<<fetched_inst_list[i].RR-fetched_inst_list[i].RN<<"} RR{"<<fetched_inst_list[i].RR<<","<<fetched_inst_list[i].DI-fetched_inst_list[i].RR<<"} DI{"<<fetched_inst_list[i].DI<<","<<fetched_inst_list[i].IQ-fetched_inst_list[i].DI<<"} IS{"<<fetched_inst_list[i].IQ<<","<<fetched_inst_list[i].EX-fetched_inst_list[i].IQ<<"} EX{"<<fetched_inst_list[i].EX<<","<<fetched_inst_list[i].WB-fetched_inst_list[i].EX<<"} WB{"<<fetched_inst_list[i].WB<<","<<fetched_inst_list[i].retire-fetched_inst_list[i].WB<<"} RT{"<<fetched_inst_list[i].retire<<","<<(fetched_inst_list[i].ret_end-fetched_inst_list[i].retire)+1<<"}\n";
    }
}

int main (int argc, char* argv[])
{
    //reads tracefile contents into a vector
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    // look at sim_bp.h header file for the the definition of struct proc_params
    int op_type, dest, src1, src2;  // Variables are read from trace file
    unsigned long int pc; // Variable holds the pc read from input file
    instruction inst;
    unsigned int num_instructions=0;
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }

    params.rob_size     = strtoul(argv[1], NULL, 10);
    params.iq_size      = strtoul(argv[2], NULL, 10);
    params.width        = strtoul(argv[3], NULL, 10);
    trace_file          = argv[4];
    //printf("rob_size:%lu "
    //"iq_size:%lu "
    //"width:%lu "
    //"tracefile:%s\n", params.rob_size, params.iq_size, params.width, trace_file);
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // The following loop just tests reading the trace and echoing it back to the screen.
    //
    // Replace this loop with the "do { } while (Advance_Cycle());" loop indicated in the Project 3 spec.
    // Note: fscanf() calls -- to obtain a fetch bundle worth of instructions from the trace -- should be
    // inside the Fetch() function.
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    while(fscanf(FP, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2) != EOF) {
        //printf("%lx %d %d %d %d\n", pc, op_type, dest, src1, src2);
        inst.source1=src1;
        inst.dest=dest;
        inst.source2=src2;
        inst.operation=op_type;
        inst.number=num_instructions;

        if(op_type == 0){
            inst.latency=1;
        }
        if(op_type == 1){
            inst.latency=2;
        }
        if(op_type == 2){
            inst.latency=5;
        }
        inst.IQ=-1;
        inst.DI=-1;
        inst.DE=-1;
        inst.FE=-1;
        inst.RN=-1;
        inst.RR=-1;
        inst.retire=-1;
        inst.WB=-1;
        inst.EX=-1;
        inst.ret_end=-1;

        inst_list.push_back(inst);
        fetched_inst_list.push_back(inst);
        num_instructions++;
    }

    //initialize RMT
    for(int i=0; i<RMT.size(); i++){
        RMT[i].valid=0;
        RMT[i].ROB_tag=i;// if >=100 this is an actual rob tag , else it is an ARF index
    }
    //initialize data structures- rob, iq, pipeline registers
    //initialize size of pipeline registers
    DI.pr_init(params.width);
    DE.pr_init(params.width);
    RN.pr_init(params.width);
    RR.pr_init(params.width);
    WB.pr_init(params.width*5);
    ROB.rob_init(params.rob_size);
    IQ.iq_init(params.iq_size);
    execute_list.resize(params.width);
    for(int i=0; i<execute_list.size(); i++){
        execute_list[i].init_FU();
    }

    do{
        retire();
        writeback();
        execute();
        issue();
        dispatch();
        regRead();
        rename();
        decode();
        fetch();
    } while(advance_cycle());

    print_timing_info();
    cout<<"# === Simulator Command ========="<<endl;
    printf("# ./sim %lu %lu %lu %s\n", params.rob_size, params.iq_size, params.width, trace_file);
    cout<<"# === Processor Configuration ==="<<endl;
    cout<<"# ROB_SIZE = "<<params.rob_size<<endl;
    cout<<"# IQ_SIZE  = "<<params.iq_size<<endl;
    cout<<"# WIDTH    = "<<params.width<<endl;
    cout<<"# === Simulation Results ========"<<endl;
    cout<<"# Dynamic Instruction Count    = "<<fetched_inst_list.size()<<endl;
    cout<<"# Cycles                       = "<<gclock<<endl;
    printf("# Instructions Per Cycle (IPC) = %.2f\n", double(fetched_inst_list.size())/double(gclock));
    return 0;
}