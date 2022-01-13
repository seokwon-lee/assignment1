#include "common.h"
#include "sim.h"
#include "trace.h" 
#include "cache.h"  /**** NEW-LAB2*/ 
#include "memory.h" // NEW-LAB2 
#include <stdlib.h>
#include <ctype.h> /* Library for useful character operations */
/*******************************************************************/
/* Simulator frame */ 
/*******************************************************************/

bool run_a_cycle(memory_c *m ); // please modify run_a_cycle function argument  /** NEW-LAB2 */ 
void init_structures(memory_c *m); // please modify init_structures function argument  /** NEW-LAB2 */ 

/* My local functions*/
void init_regfile(void);
int total_cf_count=0;

/* uop_pool related variables */ 

uint32_t free_op_num;
uint32_t active_op_num; 
Op *op_pool; 
Op *op_pool_free_head = NULL; 

/* simulator related local functions */ 

bool icache_access(ADDRINT addr); /** please change uint32_t to ADDRINT NEW-LAB2 */  
bool dcache_access(ADDRINT addr); /** please change uint32_t to ADDRINT NEW-LAB2 */   
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

  /** NEW-LAB2 */ /* just note: passing main memory pointers is hack to mix up c++ objects and c-style code */  /* Not recommended at all */ 
  memory_c *main_memory = new memory_c();  // /** NEW-LAB2 */ 


  init_structures(main_memory);  // please modify run_a_cycle function argument  /** NEW-LAB2 */ 
  run_a_cycle(main_memory); // please modify run_a_cycle function argument  /** NEW-LAB2 */ 


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
  op_latency[OP_FCMP]  = 2; 
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
  //int count;
  int latest_inst_id;
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
void MEM_stage(memory_c *main_memory); // please modify MEM_stage function argument  /** NEW-LAB2 */ 
void WB_stage(); 
void MEM_WB_stage();

/*******************************************************************/
/*  These are the variables you'll have to write.  */ 
/*******************************************************************/

bool sim_end_condition = FALSE;     /* please complete the condition. */ 
UINT64 retired_instruction = 0;    /* number of retired instruction. (only correct instructions) */ 
UINT64 cycle_count = 0;            /* total number of cycles */ 
UINT64 data_hazard_count = 0;  
UINT64 control_hazard_count = 0; 
UINT64 icache_miss_count = 0;      /* total number of icache misses. for Lab #2 and Lab #3 */ 
UINT64 dcache_hit_count = 0;      /* total number of dcache  misses. for Lab #2 and Lab #3 */ 
UINT64 dcache_miss_count = 0;      /* total number of dcache  misses. for Lab #2 and Lab #3 */ 
UINT64 l2_cache_miss_count = 0;    /* total number of L2 cache  misses. for Lab #2 and Lab #3 */  
UINT64 dram_row_buffer_hit_count = 0; /* total number of dram row buffer hit. for Lab #2 and Lab #3 */   // NEW-LAB2
UINT64 dram_row_buffer_miss_count = 0; /* total number of dram row buffer hit. for Lab #2 and Lab #3 */   // NEW-LAB2
UINT64 store_load_forwarding_count = 0;  /* total number of store load forwarding for Lab #2 and Lab #3 */  // NEW-LAB2
list<unsigned long long int> oplist; //The broadcase op-ids
list<unsigned long long int>::iterator op_iterator; //Iterator for broadcast ops
/*******************************************************************/
/*  My Variables  */
/*******************************************************************/
int MEM_latency_countdown = 0;
int EX_latency_countdown = 0;
bool control_stall = false;
bool data_stall = false;
bool read_trace_error = false;
bool dcache_miss_and_full_mshr = false;
int mem_ops_mshr = 0; // to check whether the mshr is empty to check the simulation end condition

list<Op *> MEM_WB_latch; // MEM latch for storing OPs
list<Op *>::iterator MEM_WB_latch_iterator; //MEM latches iterator
pipeline_latch *MEM_latch;  
pipeline_latch *EX_latch;
pipeline_latch *ID_latch;
pipeline_latch *FE_latch;
UINT64 ld_st_buffer[LD_ST_BUFFER_SIZE];   /* this structure is deprecated. do not use */ 
UINT64 next_pc; 

Cache *data_cache;  // NEW-LAB2 

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
  out << "Total D-cache hit: " << dcache_hit_count << endl; 
  out << "Total D-cache miss: " << dcache_miss_count << endl; 
  out << "Total L2-cache miss: " << l2_cache_miss_count << endl; 
  out << "Total data hazard: " << data_hazard_count << endl;
  out << "Total control hazard : " << control_hazard_count << endl; 
  out << "Total DRAM ROW BUFFER Hit: " << dram_row_buffer_hit_count << endl;  // NEW-LAB2
  out << "Total DRAM ROW BUFFER Miss: "<< dram_row_buffer_miss_count << endl;  // NEW-LAB2 
  out << "Total Store-load forwarding: " << store_load_forwarding_count << endl;  // NEW-LAB2 
  out.close();
}

/*******************************************************************/
/*  Support Functions  */ 
/*******************************************************************/

bool get_op(Op *op)
{
  static UINT64 unique_count = 1; //0->1
  Trace_op trace_op; 
  bool success = FALSE; 
  // read trace 
  // fill out op info 
  // return FALSE if the end of trace 
  success = (gzread(g_stream, &trace_op, sizeof(Trace_op)) >0 );
  if (KNOB(KNOB_PRINT_INST)->getValue()) dprint_trace(&trace_op); 

  /* copy trace structure to op */ 
  if (success) { 
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
  int flag = 0;
  std::cout << "--------------------------------------------" << endl; 
  std::cout <<"cycle count : " << dec << cycle_count << " retired_instruction : " << retired_instruction << endl; 
  std::cout << (int)cycle_count << " FE: " ;
  if (FE_latch->op) {
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

  if((int)oplist.size()!=0){
	  for(op_iterator=oplist.begin(); op_iterator!=oplist.end(); ++op_iterator)
	   {
		   std::cout << " MEM_MSHR: " ;
		   cout << *op_iterator ;
		   flag=1;
	   }
	   oplist.clear();
	}
  if (MEM_latch->op_valid) {
	std::cout << " MEM_PIPE: " ;
    Op *op = MEM_latch->op; 
    cout << (int)op->inst_id ;
    flag=1;
  }

  if(flag==0){
	std::cout << " MEM: " ;
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

bool run_a_cycle(memory_c *main_memory){   // please modify run_a_cycle function argument  /** NEW-LAB2 */ 

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
    MEM_WB_stage();
    main_memory->run_a_cycle();          // *NEW-LAB2 

    WB_stage(); 
    MEM_stage(main_memory);  // please modify MEM_stage function argument  /** NEW-LAB2 */ 
    EX_stage();
    ID_stage();
    FE_stage(); 
    if (KNOB(KNOB_PRINT_PIPE_FREQ)->getValue() && !(cycle_count%KNOB(KNOB_PRINT_PIPE_FREQ)->getValue())) print_pipeline();
  }
  return TRUE; 
}


/*******************************************************************/
/* Complete the following fuctions.  */
/* You can add new data structures and also new elements to Op, Pipeline_latch data structure */ 
/*******************************************************************/

void init_structures(memory_c *main_memory) // please modify init_structures function argument  /** NEW-LAB2 */ 
{
  init_op_pool(); 
  init_op_latency();
  /* please initialize other data stucturs */ 
  /* you must complete the function */
  init_latches();
  init_regfile();
  main_memory->init_mem();
  data_cache = new Cache();
  cache_init(data_cache, KNOB(KNOB_DCACHE_SIZE)->getValue(), KNOB(KNOB_BLOCK_SIZE)->getValue(),
  KNOB(KNOB_DCACHE_WAY)->getValue() , "L1cache");
  //64B block size
}

void MEM_WB_stage(){
	for(MEM_WB_latch_iterator=MEM_WB_latch.begin(); MEM_WB_latch_iterator!=MEM_WB_latch.end(); ++MEM_WB_latch_iterator)
	{
	Op* op = *MEM_WB_latch_iterator;
	if( op->dst!= -1 ) {
    if(register_file[ op->dst ].latest_inst_id == op->inst_id){
      register_file[ op->dst ].valid = true;
      if(data_stall)
        data_stall=false;
    }
  }	 
	if(mem_ops_mshr>0)
  {
		 mem_ops_mshr--;
  }
    retired_instruction++; 
    free_op(op);
    dcache_miss_and_full_mshr = false;
	}
MEM_WB_latch.clear();
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
      if(register_file[ MEM_latch->op->dst ].latest_inst_id == MEM_latch->op->inst_id){
        register_file[ MEM_latch->op->dst ].valid = true;
        if(data_stall)
          data_stall=false;
      }
    }
	
    retired_instruction++;
    free_op(MEM_latch->op);
  }
}

/*I think the implementation of the PC resolution should be in MEM or EX, not WB!*/
void MEM_stage(memory_c *main_memory) {
  if(EX_latch->op_valid){
    if(EX_latch->op->mem_type == NOT_MEM) { // NOT a MEM instruction: just pass through
      MEM_latch->op = EX_latch->op;
      MEM_latch->op_valid = EX_latch->op_valid;  
      return;
    }
    if(EX_latch->op->opcode == OP_LD || EX_latch->op->opcode == OP_ST){
      /*Inside the simulator, the simulator calls the dcache_access function at the last cycle of dcache access latency
       to mimic the actual hardware more closely.*/
      
      /*ALWAYS CHECKS THE IF STATEMENT.*/
      if(MEM_latency_countdown > 0){ //Waiting for an access to D-cache
        MEM_latency_countdown--;
      }
      else{
        MEM_latency_countdown = KNOB(KNOB_DCACHE_LATENCY)->getValue()-1;
      }

      if( MEM_latency_countdown > 0){ //Send NOP while waiting..
        MEM_latch->op = NULL;
        MEM_latch->op_valid = false;
        return;
      }
      /*if it comes to here, the instruction have waited for dcache latency, and now access it */

      uint64_t cache_check_addr;
      if(EX_latch->op->opcode == OP_LD)
        cache_check_addr = EX_latch->op->ld_vaddr;
      else if(EX_latch->op->opcode == OP_ST)
        cache_check_addr = EX_latch->op->st_vaddr;

      l2_cache_miss_count++;
      if(dcache_miss_and_full_mshr==false && dcache_access(cache_check_addr)) //If a cache hit, the instruction will be moved to the WB stage.
      {
        MEM_latch->op = EX_latch->op;
        MEM_latch->op_valid = EX_latch->op_valid;  
        return;
      }
      else{
        /*->cache miss case: non blocking cache  
        Even if an instruction generates a cache miss,
        the pipeline continues to execute if there are ready instructions.(next incoming instructions)*/
        if(main_memory->store_load_forwarding(EX_latch->op)) //store-load forwarding. In this case, it works like cache hit.
        { 
          store_load_forwarding_count++;
          MEM_latch->op = EX_latch->op;
          MEM_latch->op_valid = EX_latch->op_valid; 
          return;
        }
        
        if( main_memory->check_piggyback(EX_latch->op) == true )//match in MSHR
        {                 //search_matching_mshr is inside of check_piggyback
          mem_ops_mshr++;  // increase the number of operations in mshr
        } 
        else{ //no match in MSHR
          if(main_memory->insert_mshr(EX_latch->op)){
            mem_ops_mshr++; // increase the number of operations in mshr
            dcache_miss_and_full_mshr = false;
            /*When there is a space in the MSHR, the processor can put a request into the MSHR. 
            The mem stage can process a new instruction from the following cycle. 
            If there is space in the MSHR, the processor creates an entry in the MSHR.*/
          }
          else{ //no space in MSHR; processor stalls
            MEM_latency_countdown = 1; 
            dcache_miss_and_full_mshr = true;
            /*stall for one cycle.
            if after one cycle, there is still no space in MSHR, keep stalling
            */
          }
        }
      }
    }
  }
  else{ //NOP passing
    MEM_latch->op = EX_latch->op;
    MEM_latch->op_valid = EX_latch->op_valid;  
  }
}

/*EX stage
======================================================
ex stall         | send NOP to next stage
------------------------------------------------------
otherwise        | latch info delivery : ID -> EX 
------------------------------------------------------
mem stall        | just hold
------------------------------------------------------
*/
void EX_stage() {
  /* the if statement order matters
   EX_latency_count comes first, which means EX will
   execute its operation regardless of mem stall
   if there are remaining EX op cycles*/
  if( EX_latency_countdown > 0)
  {
    EX_latency_countdown--;
  }
  else if(MEM_latency_countdown > 0)
    return;
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
ex stall & mem stall  | just hold
----------------------------------------------------
data hazard           | send NOP to next stage
----------------------------------------------------
control hazard        | latch info delivery : ID -> EX 
& otherwise           |
----------------------------------------------------
*/
void ID_stage() {
  if( (EX_latency_countdown > 0) || (MEM_latency_countdown > 0)){
    return;
  }

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
        register_file[ FE_latch->op->dst ].latest_inst_id = FE_latch->op->inst_id;
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
  if( (EX_latency_countdown!=0) || (data_stall) || (MEM_latency_countdown!=0)) {  //Execution stall or Data stall or mem stall
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
      if((FE_latch->op_valid==false) && (ID_latch->op_valid==false) && (EX_latch->op_valid==false) && (MEM_latch->op_valid==false) && mem_ops_mshr == 0) 
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
   register_file[ii].latest_inst_id = -1;
  }
}

bool icache_access(ADDRINT addr) {   /** please change uint32_t to ADDRINT NEW-LAB2 */ 

  /* For Lab #1, you assume that all I-cache hit */     
  bool hit = FALSE; 
  if (KNOB(KNOB_PERFECT_ICACHE)->getValue()) hit = TRUE; 
  return hit; 
}


bool dcache_access(ADDRINT addr) { /** please change uint32_t to ADDRINT NEW-LAB2 */ 
  /* For Lab #1, you assume that all D-cache hit */     
  /* For Lab #2, you need to connect cache here */   // NEW-LAB2 
  bool hit = FALSE;
  if (KNOB(KNOB_PERFECT_DCACHE)->getValue()) {hit = TRUE;} 
  else if(cache_access(data_cache, addr) != false){
    hit = TRUE;
    dcache_hit_count ++;
  }
  else{
    hit = FALSE;
    if(dcache_miss_and_full_mshr == false)
      dcache_miss_count++;
  }
  return hit;
}

// NEW-LAB2 
void dcache_insert(ADDRINT addr)  // NEW-LAB2 
{                                 // NEW-LAB2 
  /* dcache insert function */   // NEW-LAB2 
  cache_insert(data_cache, addr) ;   // NEW-LAB2 
 
}                                       // NEW-LAB2 

void broadcast_rdy_op(Op* op)             // NEW-LAB2 
{                                          // NEW-LAB2 
  /* you must complete the function */     // NEW-LAB2 
  // mem ops are done.  move the op into WB stage   // NEW-LAB2 

  /* if mem op finished DRAM access, 
  all the mem ops of the corresponding MSHR entry 
  moves to WB stage*/ 
  MEM_WB_latch.push_back(op);
  oplist.push_back(op->inst_id);
}      // NEW-LAB2 



/* utility functions that you might want to implement */     // NEW-LAB2 
int64_t get_dram_row_id(ADDRINT addr)    // NEW-LAB2 
{  // NEW-LAB2 
 // NEW-LAB2 
/* utility functions that you might want to implement */     // NEW-LAB2 
/* if you want to use it, you should find the right math! */     // NEW-LAB2 
/* pleaes carefull with that DRAM_PAGE_SIZE UNIT !!! */     // NEW-LAB2 
  // addr >> 6;   // NEW-LAB2 

  /* addr as an imput, dram row id as an output
  ->how can I get dram row id out of address? */
  //addr = addr >> 6; //which byte in cacheline -> 6bits
  int64_t row_id = addr/(KNOB(KNOB_DRAM_PAGE_SIZE)->getValue() * 1024);
  return row_id;   // NEW-LAB2 
}  // NEW-LAB2 

int get_dram_bank_id(ADDRINT addr)  // NEW-LAB2 
{  // NEW-LAB2 
 // NEW-LAB2 
/* utility functions that you might want to implement */     // NEW-LAB2 
/* if you want to use it, you should find the right math! */     // NEW-LAB2 
  
  /*addr as an input, dram bank id as an output*/
  //addr = addr >> 6;   // NEW-LAB2 
  int bank_id = (addr/(KNOB(KNOB_DRAM_PAGE_SIZE)->getValue()*1024))%(KNOB(KNOB_DRAM_BANK_NUM)->getValue());
  return bank_id;   // NEW-LAB2 
}  // NEW-LAB2 


