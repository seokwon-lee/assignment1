#include "common.h"
#include "sim.h"
#include "trace.h" 
#include <stdlib.h>
/*******************************************************************/
/* Simulator frame */ 
/*******************************************************************/

bool run_a_cycle();
void init_structures(); 


/* My local functions*/
void init_regfile(void);
int total_cf_count=0;

/* uop_pool related variables */ 

uint32_t free_op_num;
uint32_t active_op_num; 
Op *op_pool; 
Op *op_pool_free_head = NULL; 

/* simulator related local functions */ 

bool icache_access(uint32_t addr);
bool dcache_access(uint32_t addr);
void  init_latches(void);

#include "knob.h"
#include "all_knobs.h"

// knob variables
KnobsContainer *g_knobsContainer; /* < knob container > */
all_knobs_c    *g_knobs; /* < all knob variables > */

gzFile g_stream;

void init_knobs(int argc, char** argv)
{
  // Create the knob managing class
  g_knobsContainer = new KnobsContainer();

  // Get a reference to the actual knobs for this component instance
  g_knobs = g_knobsContainer->getAllKnobs();

  // apply the supplied command line switches
  char* pInvalidArgument = NULL;
  g_knobsContainer->applyComandLineArguments(argc, argv, &pInvalidArgument);

  g_knobs->display();
}

void read_trace_file(void)
{
  g_stream = gzopen((KNOB(KNOB_TRACE_FILE)->getValue()).c_str(), "r");
}

// simulator main function is called from outside of this file 

void simulator_main(int argc, char** argv) 
{
  init_knobs(argc, argv);

  // trace driven simulation 
  read_trace_file();
  init_structures();
  run_a_cycle();

}
int op_latency[NUM_OP_TYPE]; 

void init_op_latency(void)
{
  op_latency[OP_INV]   = 1; 
  op_latency[OP_NOP]   = 1; 
  op_latency[OP_CF]    = 1; 
  op_latency[OP_CMOV]  = 1; 
  op_latency[OP_LDA]   = 1;
  op_latency[OP_LD]    = 1; 
  op_latency[OP_ST]    = 1; 
  op_latency[OP_IADD]  = 1; 
  op_latency[OP_IMUL]  = 2; 
  op_latency[OP_IDIV]  = 4; 
  op_latency[OP_ICMP]  = 2; 
  op_latency[OP_LOGIC] = 1; 
  op_latency[OP_SHIFT] = 2; 
  op_latency[OP_BYTE]  = 1; 
  op_latency[OP_MM]    = 2; 
  op_latency[OP_FMEM]  = 2; 
  op_latency[OP_FCF]   = 1; 
  op_latency[OP_FCVT]  = 4; 
  op_latency[OP_FADD]  = 2; 
  op_latency[OP_FMUL]  = 4; 
  op_latency[OP_FDIV]  = 16; 
  op_latency[OP_FCMP]  = 2; 
  op_latency[OP_FBIT]  = 2; 
  op_latency[OP_FCMO]  = 2; 
}

void init_op(Op *op)
{
  op->num_src               = 0; 
  op->src[0]                = -1; 
  op->src[1]                = -1;
  op->dst                   = -1; 
  op->opcode                = 0; 
  op->is_fp                 = false;
  op->cf_type               = NOT_CF;
  op->mem_type              = NOT_MEM;
  op->write_flag             = 0;
  op->inst_size             = 0;
  op->ld_vaddr              = 0;
  op->st_vaddr              = 0;
  op->instruction_addr      = 0;
  op->branch_target         = 0;
  op->actually_taken        = 0;
  op->mem_read_size         = 0;
  op->mem_write_size        = 0;
  op->valid                 = FALSE; 
  /* you might add more features here */ 
}


void init_op_pool(void)
{
  /* initialize op pool */ 
  op_pool = new Op [1024];
  free_op_num = 1024; 
  active_op_num = 0; 
  uint32_t op_pool_entries = 0; 
  int ii;
  for (ii = 0; ii < 1023; ii++) {

    op_pool[ii].op_pool_next = &op_pool[ii+1]; 
    op_pool[ii].op_pool_id   = op_pool_entries++; 
    init_op(&op_pool[ii]); 
  }
  op_pool[ii].op_pool_next = op_pool_free_head; 
  op_pool[ii].op_pool_id   = op_pool_entries++;
  init_op(&op_pool[ii]); 
  op_pool_free_head = &op_pool[0]; 
}


Op *get_free_op(void)
{
  /* return a free op from op pool */ 

  if (op_pool_free_head == NULL || (free_op_num == 1)) {
    std::cout <<"ERROR! OP_POOL SIZE is too small!! " << endl; 
    std::cout <<"please check free_op function " << endl; 
    assert(1); 
    exit(1);
  }

  free_op_num--;
  assert(free_op_num); 

  Op *new_op = op_pool_free_head; 
  op_pool_free_head = new_op->op_pool_next; 
  assert(!new_op->valid); 
  init_op(new_op);
  active_op_num++; 
  return new_op; 
}

void free_op(Op *op)
{
  free_op_num++;
  active_op_num--; 
  op->valid = FALSE; 
  op->op_pool_next = op_pool_free_head;
  op_pool_free_head = op; 
}



/*******************************************************************/
/*  Data structure */
/*******************************************************************/

typedef struct pipeline_latch_struct {
  Op *op; /* you must update this data structure. */
  bool op_valid; 
   /* you might add more data structures. But you should complete the above data elements */ 
}pipeline_latch; 


typedef struct Reg_element_struct{
  bool valid;
  int count;
  // data is not needed 
  /* you might add more data structures. But you should complete the above data elements */ 
}REG_element; 

REG_element register_file[NUM_REG];


/*******************************************************************/
/* These are the functions you'll have to write.  */ 
/*******************************************************************/

void FE_stage();
void ID_stage();
void EX_stage(); 
void MEM_stage();
void WB_stage(); 

/*******************************************************************/
/*  These are the variables you'll have to write.  */ 
/*******************************************************************/

bool sim_end_condition = FALSE;     /* please complete the condition. */ 
UINT64 retired_instruction = 0;    /* number of retired instruction. (only correct instructions) */ 
UINT64 cycle_count = 0;            /* total number of cycles */ 
UINT64 data_hazard_count = 0;  
UINT64 control_hazard_count = 0; 
UINT64 icache_miss_count = 0;      /* total number of icache misses. for Lab #2 and Lab #3 */ 
UINT64 dcache_miss_count = 0;      /* total number of dcache  misses. for Lab #2 and Lab #3 */ 
UINT64 l2_cache_miss_count = 0;    /* total number of L2 cache  misses. for Lab #2 and Lab #3 */  


/*******************************************************************/
/*  My Variables  */
/*******************************************************************/
int EX_latency_countdown = 0;
bool control_stall = false;
bool data_stall = false;
bool read_trace_error = false;



pipeline_latch *MEM_latch;  
pipeline_latch *EX_latch;
pipeline_latch *ID_latch;
pipeline_latch *FE_latch;
UINT64 ld_st_buffer[LD_ST_BUFFER_SIZE]; 
UINT64 next_pc; 

/*******************************************************************/
/*  Print messages  */
/*******************************************************************/

void print_stats() {
  std::ofstream out((KNOB(KNOB_OUTPUT_FILE)->getValue()).c_str());
  /* Do not modify this function. This messages will be used for grading */ 
  out << "Total instruction: " << retired_instruction << endl; 
  out << "Total cycles: " << cycle_count << endl; 
  float ipc = (cycle_count ? ((float)retired_instruction/(float)cycle_count): 0 );
  out << "IPC: " << ipc << endl; 
  out << "Total I-cache miss: " << icache_miss_count << endl; 
  out << "Total D-cache miss: " << dcache_miss_count << endl; 
  out << "Total L2-cache miss: " << l2_cache_miss_count << endl; 
  out << "Total data hazard: " << data_hazard_count << endl;
  out << "Total control hazard : " << control_hazard_count << endl; 
  out.close();
}

/*******************************************************************/
/*  Support Functions  */ 
/*******************************************************************/

bool get_op(Op *op)
{
  static UINT64 unique_count = 0; 
  Trace_op trace_op; 
  bool success = FALSE; 

  success = (gzread(g_stream, &trace_op, sizeof(Trace_op)) >0 );
  if (KNOB(KNOB_PRINT_INST)->getValue()) dprint_trace(&trace_op); 

  /* copy trace structure to op */ 
  if ( success ) { 
    copy_trace_op(&trace_op, op); 

    op->inst_id  = unique_count++;
    op->valid    = TRUE; 
  }
  return success; 
}
/* return op execution cycle latency */ 

int get_op_latency (Op *op) 
{
  assert (op->opcode < NUM_OP_TYPE); 
  return op_latency[op->opcode];
}

/* Print out all the register values */ 
void dump_reg() {
  for (int ii = 0; ii < NUM_REG; ii++) {
    std::cout << cycle_count << ":register[" << ii  << "]: V:" << register_file[ii].valid << endl; 
  }
}

void print_pipeline() {
  std::cout << "--------------------------------------------" << endl; 
  std::cout <<"cycle count : " << dec << cycle_count << " retired_instruction : " << retired_instruction << endl; 
  std::cout << (int)cycle_count << " FE: " ;
  if (FE_latch->op_valid) {
    Op *op = FE_latch->op; 
    cout << (int)op->inst_id ;
  }
  else {
    cout <<"####";
  }
  std::cout << " ID: " ;
  if (ID_latch->op_valid) {
    Op *op = ID_latch->op; 
    cout << (int)op->inst_id ;
  }
  else {
    cout <<"####";
  }
  std::cout << " EX: " ;
  if (EX_latch->op_valid) {
    Op *op = EX_latch->op; 
    cout << (int)op->inst_id ;
  }
  else {
    cout <<"####";
  }


  std::cout << " MEM: " ;
  if (MEM_latch->op_valid) {
    Op *op = MEM_latch->op; 
    cout << (int)op->inst_id ;
  }
  else {
    cout <<"####";
  }
  cout << endl; 
  //  dump_reg();   
  std::cout << "--------------------------------------------" << endl; 
}

void print_heartbeat()
{
  static uint64_t last_cycle ;
  static uint64_t last_inst_count; 
  float temp_ipc = float(retired_instruction - last_inst_count) /(float)(cycle_count-last_cycle) ;
  float ipc = float(retired_instruction) /(float)(cycle_count) ;
  /* Do not modify this function. This messages will be used for grading */ 
  cout <<"**Heartbeat** cycle_count: " << cycle_count << " inst:" << retired_instruction << " IPC: " << temp_ipc << " Overall IPC: " << ipc << endl; 
  last_cycle = cycle_count;
  last_inst_count = retired_instruction; 
}
/*******************************************************************/
/*                                                                 */
/*******************************************************************/

bool run_a_cycle(){

  for (;;) { 
    if (((KNOB(KNOB_MAX_SIM_COUNT)->getValue() && (cycle_count >= KNOB(KNOB_MAX_SIM_COUNT)->getValue())) || 
      (KNOB(KNOB_MAX_INST_COUNT)->getValue() && (retired_instruction >= KNOB(KNOB_MAX_INST_COUNT)->getValue())) ||  (sim_end_condition))) { 
        // please complete sim_end_condition 
        // finish the simulation 
        print_heartbeat(); 
        print_stats();
        return TRUE; 
    }
    cycle_count++; 
    if (!(cycle_count%5000)) {
      print_heartbeat(); 
    }
    WB_stage(); 
    MEM_stage();
    EX_stage();
    ID_stage();
    FE_stage(); 
    /*reverse order: each stage should get operations from the former stage of the "previous cycle"
    if the order is FE->ID-> ... ->WB, then each stage should get operations from the former stage of the "current cycle"*/
    if (KNOB(KNOB_PRINT_PIPE_FREQ)->getValue() && !(cycle_count%KNOB(KNOB_PRINT_PIPE_FREQ)->getValue())) print_pipeline();
  }
  return TRUE; 
}


/*******************************************************************/
/* Complete the following fuctions.  */
/* You can add new data structures and also new elements to Op, Pipeline_latch data structure */ 
/*******************************************************************/

void init_structures(void) {
  init_op_pool(); 
  init_op_latency();
  init_latches();
  init_regfile();
}

void WB_stage() {
  if( MEM_latch->op_valid == true ) {
    if((MEM_latch->op->opcode == OP_CF) && control_stall)
      control_stall = false; 

    /*implementaion of the resolution of the control hazard at mem stage*/
    /*the actual pc determination of CF inst is at the end of the MEM stage, <- not EX stage?
    and the fetching happens in IF(FE) of the following cycle. <- why not at the same cycle?
    Therefore it should be implemented at the WB stage to prevent frome the CF resolution and fetching 
    happening at the same cycle.
    (The branch target address is forwarded at MEM stage)*/
  	
    /*resolution of data hazard; If WB stage writes a value to the register at first half cycle, ID can be 
    successfully done if there is no instruction using that regsiter at EX or MEM stage.
    register_file[register id].count indicates the number of instructions in EX or MEM stage(max 2) 
    that has the register id as a dst(destination register); 
    so that count is <0 or 1 or 2 at ID>, <0 or 1 at WB>
    (if there is no dst in instruction, dst value sets to -1)*/
    if( MEM_latch->op->dst!= -1 ) {
      register_file[ MEM_latch->op->dst ].count--;

      /*if the count is 0, it means no following insts of EX or MEM are having that register id as a destination.
      After WB<<.. count--;>>, if the count becomes 0<<if(.. ==0)>>, it is safe to use the register as src(source)
      */
      if( register_file[ MEM_latch->op->dst ].count==0 ) {
        register_file[ MEM_latch->op->dst ].valid = true;
        if(data_stall)
          data_stall = false;
      } 
    }
	
    retired_instruction++;
    free_op(MEM_latch->op);
  }
}

/*I think the implementation of the PC resolution should be in MEM or EX, not WB!*/
void MEM_stage() {
  //if( EX_latch->op_valid == true ) {
  //  if((EX_latch->op->opcode == OP_CF) && control_stall)
  //     control_stall = false;
  //} 
  MEM_latch->op = EX_latch->op;
  MEM_latch->op_valid = EX_latch->op_valid;  
}

/*EX stage
======================================================
ex stall         | send NOP to next stage
------------------------------------------------------
otherwise        | latch info delivery : ID -> EX 
------------------------------------------------------
*/
void EX_stage() {
  if( EX_latency_countdown > 0)
  {
    EX_latency_countdown--;
  }
  else if(ID_latch->op_valid){
    EX_latency_countdown = get_op_latency(ID_latch->op)-1;
  }

  if( EX_latency_countdown > 0){
    EX_latch->op = NULL;
    EX_latch->op_valid = false;
  }
  else{
    EX_latch->op = ID_latch->op;
    EX_latch->op_valid = ID_latch->op_valid;
  }
}

/*ID stage
====================================================
ex stall           | just hold
----------------------------------------------------
data hazard        | send NOP to next stage
----------------------------------------------------
control hazard     | latch info delivery : ID -> EX 
& otherwise        |
----------------------------------------------------
*/
void ID_stage() {
  if( EX_latency_countdown > 0)
    return;

  /*while execution stalling, hazard count do not increase*/
  if( FE_latch->op_valid ) {
    /* Checking for any source data hazard */   
    for (int ii = 0; ii < FE_latch->op->num_src; ii++) {
      if( register_file[ FE_latch->op->src[ii] ].valid == false ) {
        data_hazard_count++;  
        data_stall = true;
        break;
      }
    }
    /* checking control hazard */
    if(FE_latch->op->opcode == OP_CF){
      control_hazard_count++;
      control_stall = true;
    }
  }
  
  if ( data_stall ){
    ID_latch->op = NULL;
    ID_latch->op_valid = false;
  }
  else{
    /* marking invalid registers : only when there is no (data or EX) stall*/ 
    if( FE_latch->op_valid && FE_latch->op->dst != -1 ) {
        register_file[ FE_latch->op->dst ].valid = false; 
        register_file[ FE_latch->op->dst ].count++;
    }
    ID_latch->op = FE_latch->op;
    ID_latch->op_valid = FE_latch->op_valid;
  }
}

/*IF(FE) stage
==================================================
control hazard  | send NOP to next stage
--------------------------------------------------
otherwise       | just hold
--------------------------------------------------
*/
void FE_stage() {
 if( (EX_latency_countdown!=0) || (data_stall) ) {  //Execution stall or Data stall
      FE_latch->op = FE_latch->op;
      FE_latch->op_valid = FE_latch->op_valid;  
  }
  else if( control_stall ) {	//Control stall
      FE_latch->op = NULL;
      FE_latch->op_valid = false;    
  }
  /*why holds op in latch at EX or Data stall and send NOP when control stall?*/
  else{ // if there is no stall, fetches an instruction
    Op *op = get_free_op();
    
    if( get_op(op) ){  
      FE_latch->op = op;
      FE_latch->op_valid = true;  
    } 
    else { /*end of simulation if all latches are empty and if it is at the of the trace file(get_op(op) == false)*/
      if((FE_latch->op_valid==false) && (ID_latch->op_valid==false) && (EX_latch->op_valid==false) && (MEM_latch->op_valid==false) ) 
    	  sim_end_condition = true;
    	FE_latch->op = NULL;
      FE_latch->op_valid = false;  
      free_op(op);    
    }
  }
}

void  init_latches()
{
  MEM_latch = new pipeline_latch();
  EX_latch = new pipeline_latch();
  ID_latch = new pipeline_latch();
  FE_latch = new pipeline_latch();

  MEM_latch->op = NULL;
  EX_latch->op = NULL;
  ID_latch->op = NULL;
  FE_latch->op = NULL;

  /* you must set valid value correctly  */ 
  MEM_latch->op_valid = false;
  EX_latch->op_valid = false;
  ID_latch->op_valid = false;
  FE_latch->op_valid = false;

}

void init_regfile() {
  for (int ii = 0; ii < NUM_REG; ii++) {
   register_file[ii].valid = true;
   register_file[ii].count = 0;
  }
}

bool icache_access(uint32_t addr) {

  /* For Lab #1, you assume that all I-cache hit */     
  bool hit = FALSE; 
  if (KNOB(KNOB_PERFECT_ICACHE)->getValue()) hit = TRUE; 
  return hit; 
}



bool dcache_access(uint32_t addr) {
  /* For Lab #1, you assume that all D-cache hit */     
  bool hit = FALSE;
  if (KNOB(KNOB_PERFECT_DCACHE)->getValue()) hit = TRUE; 
  return hit; 
}