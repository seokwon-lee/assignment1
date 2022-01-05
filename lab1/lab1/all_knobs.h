/**********************************************************************************************
 * File         : all_knob.h 
 * Description  : knob template 
 * This file comes from MacSim Simulator 
 * All knobs have to be added in this file  and all_knobs.h 
 *********************************************************************************************/


#ifndef __ALL_KNOBS_H_INCLUDED__
#define __ALL_KNOBS_H_INCLUDED__

//#include "global_types.h"
#include "knob.h"

#define KNOB(var) g_knobs->var

///////////////////////////////////////////////////////////////////////////////////////////////
/// \brief knob variables holder
///////////////////////////////////////////////////////////////////////////////////////////////
class all_knobs_c {
	public:
		/**
		 * Constructor
		 */
		all_knobs_c();

		/**
		 * Destructor
		 */
		~all_knobs_c();

		/**
		 * Register Knob Variables
		 */
		void registerKnobs(KnobsContainer *container);

		/**
		 * Display All the Knob Variables
		 */
		void display();

	public:

		KnobTemplate< unsigned long long >* KNOB_MAX_SIM_COUNT;
		KnobTemplate< unsigned long long >* KNOB_MAX_INST_COUNT;
		KnobTemplate< string >* KNOB_OUTPUT_FILE;
		KnobTemplate< bool >* KNOB_PRINT_INST;
		KnobTemplate< bool >* KNOB_PERFECT_ICACHE;
		KnobTemplate< bool >* KNOB_PERFECT_DCACHE;
		KnobTemplate< bool >* KNOB_PERFECT_BR;
		KnobTemplate< bool >* KNOB_PERFECT_L2;
		KnobTemplate< bool >* KNOB_READ_TRACE;
		KnobTemplate< string >* KNOB_TRACE_FILE;
		KnobTemplate< unsigned >* KNOB_ISSUE_WIDTH;
		KnobTemplate< unsigned >* KNOB_ICACHE_LATENCY;
		KnobTemplate< unsigned >* KNOB_DCACHE_LATENCY;
		KnobTemplate< unsigned >* KNOB_MEM_LATENCY;
		KnobTemplate< unsigned >* KNOB_L2CACHE_LATENCY;
		KnobTemplate< unsigned long long >* KNOB_PRINT_PIPE_FREQ;
};
#endif //__ALL_KNOBS_H_INCLUDED__
