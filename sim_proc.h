#ifndef SIM_PROC_H
#define SIM_PROC_H
#include <string.h>
#include <vector>
#include <iostream>
#include <math.h>

using namespace std;
extern unsigned int gclock;
typedef struct proc_params{
    unsigned long int rob_size;
    unsigned long int iq_size;
    unsigned long int width;
}proc_params;

// Put additional data structures here as per your requirement
//-------------------INSTRUCTION----------------------
typedef struct { //instruction from file
    unsigned int number;
    unsigned int operation;
    unsigned int latency;
    int source1;
    int source2;
    int dest;
    int FE;
    int RN;
    int DE;
    int RR;
    int DI;
    int IQ;
    int EX;
    int WB;
    int retire;
    int ret_end;
    int rob_tag;
    bool src1_ready;
    bool src2_ready;
} instruction;

typedef struct{ //register for RMT
    bool valid;
    unsigned int ROB_tag;
} reg;

typedef struct {
    bool valid;
    instruction inst;
} pr_element;


typedef struct {
    int dst;
    bool rdy;
    bool exc;
    bool mis;
    unsigned int pc;
    bool valid;
    unsigned int rob_tag;
    instruction inst;
} reorder_entry;

typedef struct { //the information the IQ "key" gets from the IQ
    bool valid;
    unsigned int dest_tag;
    bool rs1_ready;
    bool rs2_ready;
    unsigned int rs1_tag;
    unsigned int rs2_tag;
    instruction inst; //instruction
    bool ready;
    bool oldest_valid_flag;
} IQ_entry;

typedef struct{
    bool valid;
    instruction instr;
    unsigned int instr_cycle_count;
} fu_entry;


//-----------PIPELINE REGISTER AND OPERATIONS------------
class PIPELINE_REGISTER{
public:
    vector <pr_element> register_data;
    //functions
    PIPELINE_REGISTER();
    void pr_init(int s); //create a new pipeline register unit
    bool is_empty(); //checks if a pipeline register is full
    unsigned int num_spaces();
};

PIPELINE_REGISTER::PIPELINE_REGISTER() {

}

unsigned int PIPELINE_REGISTER::num_spaces() {
    unsigned int empty_count=0;
    for(int i=0; i<register_data.size(); i++){
        if(register_data[i].valid == 0){

        }
    }
}

void PIPELINE_REGISTER::pr_init(int s) {
    register_data.resize(s);
    for(int i=0; i<register_data.size(); i++){
        register_data[i].valid=0; //initializes empty pipeline register
    }

}

bool PIPELINE_REGISTER::is_empty() {
    bool empty_space=true;
    for(int i=0; i<register_data.size(); i++){
        if(register_data[i].valid==1){ //there is a taken space, not enough room for a full bundle
            empty_space=false;
            break;
        }
    }

    return empty_space;
}

//--------------------REORDER BUFFER CLASS AND OPERATIONS----------------
class rob_queue{
    // Initialize front and rear
public:
    int rear, front;
    // Circular Queue
    int size;
    reorder_entry *arr;

    //functions
    unsigned int enQueue(reorder_entry value);
    int deQueue();
    void displayQueue();
    void set_ready(unsigned int tag);
    void resize_queue(int s);
    unsigned int free_space();
    rob_queue();
    void clear_queue(); //initialized ROB to base state (everything 0)
};

rob_queue::rob_queue() {

}

void rob_queue::clear_queue(){
    for(int i=0; i<size; i++){
        arr[i].rdy=0;
        arr[i].valid=0;
        arr[i].pc=-1;
        arr[i].dst=0;
        arr[i].exc=0;
        arr[i].mis=0;
        arr[i].rob_tag=i+100;
    }
}

void rob_queue::resize_queue(int s) {
    front = rear = -1;
    size = s;
    arr = (reorder_entry *) malloc(s*sizeof(reorder_entry));
}

unsigned int rob_queue::enQueue(reorder_entry value){
    if (front == -1){
        front = rear = 0;
        // value.valid=1;
        //arr[rear] = value;
    }
    else if ((front == 0 && rear == size-1) ||
             (rear == (front-1)%(size-1))){
        //printf("\nROB Queue is Full");
        return 0xffffffff;
    }
    else if (rear == size-1 && front != 0){
        rear = 0;
        // value.valid=1;
        //arr[rear] = value;
    }

    else{
        rear++;
        // value.valid=1;
        //arr[rear] = value;
    }

    arr[rear]= value;
    arr[rear].rob_tag = 100 + rear;
    return arr[rear].rob_tag;
}

// Function to delete element from Circular Queue
int rob_queue::deQueue(){
    if (front == -1){
        //printf("\nQueue is Empty");
        return -1;
    }

    reorder_entry data = arr[front];
    arr[front].pc = -1;
    arr[front].valid=0;
    if (front == rear){
        front = -1;
        rear = -1;
    }
    else if (front == size-1) {
        front = 0;
    }
    else {
        front++;
    }

    return data.pc;
}

// Function displaying the elements
// of Circular Queue
void rob_queue::displayQueue(){
    if (front == -1){
        //cout<<"queue empty"<<endl;
        return;
    }
    if (rear >= front){
        for (int i = front; i <= rear; i++) {
            //printf("instruction: %d tag: %d dest: %d rdy: %d\n",arr[i].inst.number, arr[i].rob_tag, arr[i].dst, arr[i].rdy);
        }
    }
    else{
        for (int i = front; i < size; i++) {
            //printf("instruction: %d tag: %d dest: %d rdy: %d\n", arr[i].inst.number, arr[i].rob_tag, arr[i].dst, arr[i].rdy);
        }
        for (int i = 0; i <= rear; i++) {
            //printf("instruction: %d tag: %d dest: %d rdy: %d\n", arr[i].inst.number, arr[i].rob_tag, arr[i].dst, arr[i].rdy);
        }
    }
}

//count free spaces in circular queue
unsigned int rob_queue::free_space() {
    unsigned int spaces=0;
    for(int i=0; i<size; i++){
        //printf("%d ", arr[i].valid);
        if(arr[i].valid == 0){
            spaces++;
        }
    }

    return spaces;
}


void rob_queue::set_ready(unsigned int tag){
    if (front == -1){
        return;
    }
    if (rear >= front){
        for (int i = front; i <= rear; i++) {
            if(arr[i].rob_tag==tag){
                arr[i].rdy=1;
            }
            printf("%d ", arr[i].rob_tag);
        }
    }

    else{
        for (int i = front; i < size; i++) {
            if(arr[i].rob_tag==tag){
                arr[i].rdy=1;
            }
            printf("%d ", arr[i].rob_tag);
        }

        for (int i = 0; i <= rear; i++) {
            if(arr[i].rob_tag==tag){
                arr[i].rdy=1;
            }
            printf("%d ", arr[i].rob_tag);
        }
    }
}
//-----------------------------------------------------
//------------------REORDER BUFFER + FUNCTIONS---------------------
class REORDER_BUFFER{
public:
    rob_queue rob;

    //functions
    REORDER_BUFFER();
    void rob_init(int s);
    void delete_inst(reorder_entry);
    bool is_empty();
};

REORDER_BUFFER::REORDER_BUFFER() {

}

bool REORDER_BUFFER::is_empty() {
    bool taken_space=true; //empty until proven otherwise
    for(int i=0; i<rob.size; i++){
        if(rob.arr[i].valid==1){
            taken_space = false;
            break;
        }
    }
    return taken_space;
}

void REORDER_BUFFER::rob_init(int s) {
    rob.resize_queue(s);
    rob.clear_queue();
}

void REORDER_BUFFER::delete_inst(reorder_entry re) {
    vector<reorder_entry> recovery_list;
    bool found = false;

    while(rob.front != -1){ //while queue is not empty
        if((rob.arr[rob.front].inst.number== re.inst.number) && !found){
            rob.deQueue();
            found=1;
        }

        else{
            recovery_list.push_back(rob.arr[rob.front]);
            rob.deQueue();
        }
    }

    if(found == 0){
        //cout<<"this value is not in queue,, oopsie"<<endl;
    }

    else{
        for(int i=0; i<recovery_list.size(); i++){
            rob.enQueue(recovery_list[i]);
        }
    }
}




//--------------------ISSUE QUEUE CLASS AND OPERATIONS----------------
class iq_queue{
    // Initialize front and rear
public:
    int rear, front;
    // Circular Queue
    int size;
    IQ_entry *arr;

    //functions
    void enQueue(IQ_entry value);
    void deQueue(IQ_entry value);
    void displayQueue();
    //   IQ_entry find_issuable_element_based_on_age(int age);
    void resize_queue(int s);
    unsigned int free_space();
    iq_queue();
    void clear_queue();
    void set_ready(IQ_entry);
};

iq_queue::iq_queue() {

}

void iq_queue::clear_queue() {
    for(int i=0; i<size; i++){
        arr[i].valid=0;
        arr[i].rs1_ready=0;
        arr[i].rs2_ready=0;
        arr[i].oldest_valid_flag=0;
    }
}

/*

IQ_entry iq_queue::find_issuable_element_based_on_age(int age) { //walk through queue head to tail and find entry
    IQ_entry entry; //entry eligible to be issued
    entry.dest_tag=-6;
    bool issuable=false;
    if (front == -1){ //queue is empty
        entry.dest_tag=-6;
    }
    if (rear >= front){
        for (int i = front; i <= rear; i++) {
            if(arr[i].inst.number==age){
                if(arr[i].rs1_ready && arr[i].rs2_ready){
                    issuable=true;
                    arr[i].valid=0;
                    entry=arr[i];
                    break;
                }
            }
        }
    }
    else{
        for (int i = front; i < size; i++) {
            if(arr[i].inst.number==age){
                if(arr[i].rs1_ready && arr[i].rs2_ready){
                    issuable=true;
                    arr[i].valid=0;
                    entry=arr[i];
                    break;
                }
            }
        }

        for (int i = 0; i <= rear; i++) {
            if(arr[i].inst.number==age){
                if(arr[i].rs1_ready && arr[i].rs2_ready){
                    issuable=true;
                    arr[i].valid=0;
                    entry=arr[i];
                    break;
                }
            }
        }
    }

    return entry;
}

*/

//count free spaces in circular queue
unsigned int iq_queue::free_space() {
    unsigned int spaces=0;
    for(int i=0; i<size; i++){
        //printf("%d ", arr[i].valid);
        if(arr[i].valid == 0){
            spaces++;
        }
    }

    return spaces;
}

void iq_queue::resize_queue(int s) {
    front = rear = -1;
    size = s;
    arr = (IQ_entry *) malloc(s*sizeof(IQ_entry));
}



void iq_queue::enQueue(IQ_entry value){
    int i;
    for(i=0; i<size; i++){
        if(arr[i].valid == 0){
            arr[i]=value;
            arr[i].valid = 1;
            break;
        }
    }
    if (i==size){} //cout << "IQ Full!! Enqueue failed" << endl;
    /*
    if (front == -1){
        front = rear = 0;
        arr[rear] = value;
    }
    else if ((front == 0 && rear == size-1) ||
             (rear == (front-1)%(size-1))){
        printf("\nIQ Queue is Full");
        return;
    }
    else if (rear == size-1 && front != 0){
        rear = 0;
        arr[rear] = value;
    }

    else{
        rear++;
        arr[rear] = value;
    } */
}

// Function to delete element from Circular Queue
void iq_queue::deQueue(IQ_entry value){
    int i;
    for(i=0; i<size; i++){
        if(arr[i].valid && (arr[i].inst.number == value.inst.number)){
            arr[i].valid=0;
            break;
        }
    }
    if (i == size ){} //cout << "IQ : Entry pc : " << arr[i].inst.number << " not found! Dequeue failed!" << endl;
    /*
    if (front == -1){
        printf("\nQueue is Empty");
        return -1;
    }

    IQ_entry data = arr[front];
    arr[front].dest_tag = -1;
    if (front == rear){
        front = -1;
        rear = -1;
    }
    else if (front == size-1) {
        front = 0;
    }
    else {
        front++;
    }

    return data.dest_tag;*/
}

// Function displaying the elements
// of Circular Queue
void iq_queue::displayQueue(){
    int i;
    for(i=0; i<size; i++){
        if (arr[i].valid == 1) {
            //printf("Inst number: %d Valid: %d dest tag: %d rs1 ready: %d rs1 tag: %d rs2 ready: %d rs2 tag: %d\n",
                   //arr[i].inst.number, arr[i].valid,
                   //arr[i].dest_tag, arr[i].rs1_ready, arr[i].rs1_tag, arr[i].rs2_ready, arr[i].rs2_tag);
        }
    }
    /*
    if (front == -1){
        cout<<"queue empty"<<endl;
        return;
    }
    if (rear >= front){
        for (int i = front; i <= rear; i++) {
            if (arr[i].valid == 1) {
                printf("Inst number: %d Valid: %d dest tag: %d rs1 ready: %d rs1 tag: %d rs2 ready: %d rs2 tag: %d\n",
                       arr[i].inst.number, arr[i].valid,
                       arr[i].dest_tag, arr[i].rs1_ready, arr[i].rs1_tag, arr[i].rs2_ready, arr[i].rs2_tag);
            }
        }
        }
    else{
        for (int i = front; i < size; i++) {
            if (arr[i].valid == 1) {
                printf("Inst number: %d Valid: %d dest tag: %d rs1 ready: %d rs1 tag: %d rs2 ready: %d rs2 tag: %d\n",
                       arr[i].inst.number, arr[i].valid,
                       arr[i].dest_tag, arr[i].rs1_ready, arr[i].rs1_tag, arr[i].rs2_ready, arr[i].rs2_tag);
            }
        }
        for (int i = 0; i <= rear; i++) {
            if (arr[i].valid == 1) {
                printf("Inst number: %d Valid: %d dest tag: %d rs1 ready: %d rs1 tag: %d rs2 ready: %d rs2 tag: %d\n",
                       arr[i].inst.number, arr[i].valid,
                       arr[i].dest_tag, arr[i].rs1_ready, arr[i].rs1_tag, arr[i].rs2_ready, arr[i].rs2_tag);
                }
            }
        }
        */
}
//-----------------------------------------------------
class ISSUE_QUEUE{
public:
    iq_queue iq;

    //functions
    ISSUE_QUEUE();
    void iq_init(int s);
    void delete_inst(unsigned int iqx); //delete IQ entry from instruction queue
    void set_ready(IQ_entry iqe);
    bool is_empty();
};

ISSUE_QUEUE::ISSUE_QUEUE() {

}

bool ISSUE_QUEUE::is_empty() {
    bool taken_space=true; //empty until proven otherwise
    for(int i=0; i<iq.size; i++){
        if(iq.arr[i].valid==1){
            taken_space = false;
            break;
        }
    }
    return taken_space;
}

void ISSUE_QUEUE::set_ready(IQ_entry iqe) {

    for(int i=0; i<iq.size; i++){
        if(iq.arr[i].valid==1 && (iq.arr[i].inst.number == iqe.inst.number)){
            iq.arr[i].rs1_ready=1;
            iq.arr[i].rs2_ready=1;
            iq.arr[i].valid=0; //now that it's ready, set valid to 0
        }
    }
}

void ISSUE_QUEUE::delete_inst(unsigned int inx) {
    iq.arr[inx].valid = 0;
    iq.arr[inx].dest_tag=0;
    iq.arr[inx].rs1_tag=0;
    iq.arr[inx].rs2_tag=0;
}


void ISSUE_QUEUE::iq_init(int s) {
    iq.resize_queue(s);
    iq.clear_queue();
}




extern PIPELINE_REGISTER WB;
//----------FUNCTIONAL UNITS AND OPERATIONS------------
class FUNCTIONAL_UNIT{
public:
    vector<fu_entry> fu_arr;

    //functions
    void init_FU(); //sets up 5-element array
    void per_cycle_processing(); //yeets an element from the functional unit once its latency is finished
    bool space_exists();
};



void FUNCTIONAL_UNIT::init_FU() {
    fu_arr.resize(5);
}

bool FUNCTIONAL_UNIT::space_exists() {
    bool is_space=false;
    for(int i=0; i<fu_arr.size(); i++){
        if(fu_arr[i].valid==0){
            is_space=true;
        }
    }
    return is_space;
}



extern PIPELINE_REGISTER  DI;
extern PIPELINE_REGISTER  RR;

extern ISSUE_QUEUE IQ;
void FUNCTIONAL_UNIT::per_cycle_processing() {

    for (int i = 0; i < 5; i++) {
        if (fu_arr[i].valid == 1) {
            //cout<<"Instruction: "<<fu_arr[i].instr.number<<" cycle count: "<<fu_arr[i].instr_cycle_count<<endl;
            //cout<<"gclock: "<<gclock<<" valid instruction i= "<<fu_arr[i].instr.number<< " in execute. decrementing..."<<endl;
            //cout<<"Instruction: "<<fu_arr[i].instr.number<<" cycle count: "<<fu_arr[i].instr_cycle_count<<endl;
            fu_arr[i].instr.latency--;
            if (fu_arr[i].instr.latency == 0) {
                //send to writeback register if it has room- has room??
                for (int j = 0; j < WB.register_data.size(); j++) {
                    if (!WB.register_data[j].valid) {
                        WB.register_data[j].inst = fu_arr[i].instr;
                        WB.register_data[j].valid = 1;
                        break;
                    }
                }
                //3) Wakeup dependent instructions (set
                // their source operand ready flags) in
                // the IQ,
                //In the IQ

                //IQ.set_ready(IQ.iq.arr[k]);
                for (int k = 0; k < IQ.iq.size; k++) {
                    if (IQ.iq.arr[k].valid == 0) continue;
                    //get destination reg of current inst n compare to src1 or 2 of other insts in IQ
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == IQ.iq.arr[k].rs1_tag)) {
                        IQ.iq.arr[k].rs1_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src1 value "<<IQ.iq.arr[k].inst.source1<<" of instruction "<<IQ.iq.arr[k].inst.number<<" in iq"<<endl;
                    }
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == IQ.iq.arr[k].rs2_tag)) {
                        IQ.iq.arr[k].rs2_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src2 value "<<IQ.iq.arr[k].inst.source2<<" of instruction "<<IQ.iq.arr[k].inst.number<<" in iq"<<endl;
                    }

                }

                // DI (the dispatch bundle)
                for (int k = 0; k < DI.register_data.size(); k++) {
                    //get destination reg of current inst n compare to src1 or 2 of other insts in DI
                    if (DI.register_data[k].valid == 0) continue;
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == DI.register_data[k].inst.source1)) {
                        DI.register_data[k].inst.src1_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src1 value "<<DI.register_data[k].inst.source1<<" of instruction "<<DI.register_data[k].inst.number<<" in di"<<endl;
                    }
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == DI.register_data[k].inst.source2)) {
                        DI.register_data[k].inst.src2_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src2 value "<<DI.register_data[k].inst.source2<<" of instruction "<<DI.register_data[k].inst.number<<" in di"<<endl;
                    }
                    //cout<<"gclock: "<<gclock<<" wakeup in di"<<endl;
                }

                //and RR (the register-read bundle).
                for (int k = 0; k < RR.register_data.size(); k++) {
                    if (RR.register_data[k].valid == 0) continue;

                    //get destination reg of current inst n compare to src1 or 2 of other insts in RR
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == RR.register_data[k].inst.source1)) {
                        RR.register_data[k].inst.src1_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src1 value "<<RR.register_data[k].inst.source1<<" of instruction "<<RR.register_data[k].inst.number<<" in rr"<<endl;
                    }
                    if ((fu_arr[i].instr.dest != -1) && (fu_arr[i].instr.dest == RR.register_data[k].inst.source2)) {
                        RR.register_data[k].inst.src2_ready = 1;
                        //cout<<"gclock: "<<gclock<<" wakeup src2 value "<<RR.register_data[k].inst.source2<<" of instruction "<<RR.register_data[k].inst.number<<" in rr"<<endl;
                    }

                }
                fu_arr[i].valid = 0; //removes instruction from contention in execute list

            }
        }
    }
}


#endif