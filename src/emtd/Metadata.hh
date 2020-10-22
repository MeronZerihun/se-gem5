/*
 *	EMTD Project
 *	Metadata Class defines methods to initialize metadata from file and maintain it
 *
 *	Version:	Aug 25, 2017
 *	Author:		Mark Gallagher (markgall@umich.edu)
 *
 *  Modified for gem5 by Zelalem Aweke (zaweke@umich.edu)
 */

#ifndef _EMTD_METADATA_H
#define _EMTD_METADATA_H

#include <stdint.h>
#include <string>
#include <map>
#include "cpu/reg_class.hh" //RegID
#include "cpu/minor/dyn_inst.hh"
#include "cpu/simple/base.hh"
#include "sim/insttracer.hh"
#include "sim/sim_object.hh"
#include "mem/mem_object.hh"
#include "emtd/emtd_x86_op_groups.hh"
#include "params/Metadata.hh"
#include <array>
#include <string>
#include <vector>
#include <unordered_set>


//Define how many bytes get a single tag
//It is assumed that the metadata generator gives us metadata properly aligned
#define EMTD_CODE_TAG_GRANULARITY 4
#define EMTD_DATA_TAG_GRANULARITY 8

// Define how many bits a tag should be (found in the Python parser config files)
#define EMTD_TAG_BIT_WIDTH 1
#define EMTD_TAG_TYPE_BIT_WIDTH 1

// Defines for atomic cpu
#define RS1 inst->srcRegIdx(0)
#define RS2 inst->srcRegIdx(1)
#define RD inst->destRegIdx(0)

#define EMTD_INVALIDOP (-1)
#define EMTD_NUMBER_OF_TAGS (Emtd_tag::Count - 1)

// WE ASSUME A RV64 SYSTEM ONLY
typedef uint64_t memaddr_t;

// Define a struct that describes how metadata entries are stored in the binary file
typedef struct
{
	uint8_t startaddr[8]; // Start addr stored in first 8-bytes (using a uint8 to remove struct padding)
	uint8_t tagbyte;	  // Tag stored in lower 2 bits of last byte
} Emtd_MetadataEntry;


// Define a struct that describes how instuction taint entries are stored in the binary file
typedef struct Emtd_InsnTaintEntry
{
	memaddr_t insn_addr;  
	bool arith_tainted;	  
	bool mem_tainted; 
	Emtd_InsnTaintEntry(){
                insn_addr = 0; arith_tainted = false; mem_tainted = false;
        }
	Emtd_InsnTaintEntry(memaddr_t insn, bool arith, bool mem){
		insn_addr = insn; arith_tainted = arith; mem_tainted = mem;
	}
} Emtd_InsnTaintEntry;

// Define enum for tag types
enum Emtd_tag : uint8_t
{
	DATA = 0,
	CIPHERTEXT = 1,
	UNTAGGED = 2,
	CODE_PTR = 3, //Unused
	Count
};

// Define enum for tag types
enum Emtd_status_tag : uint8_t
{
	CLEAN = 0,
	STALE = 1,
	AHHH = 2
};

enum Emtd_tag_type : uint8_t
{
	DATA_SEG = 0,
	INSN_CONST = 1
};

class Metadata : public SimObject
{
public:
	//Metadata();
	Metadata(MetadataParams *params);
	//~Metadata();

	/** Stats registering */
	int warning_count = 0;

	// Get a tag, given an addr. If tag not yet declared, give UNTAGGED
	Emtd_tag get_mem_tag(memaddr_t memaddr);
	Emtd_tag get_insn_const_tag(memaddr_t memaddr);
	Emtd_tag get_reg_tag(RegId regIdx);				  // Get register tag
	Emtd_status_tag get_reg_tag_status(RegId regIdx); // Get register tag status

	// Set a tag in the map (must overwrite existing entry at that memory location)
	void set_mem_tag(memaddr_t memaddr, Emtd_tag newtag, Addr pc);
	void set_reg_tag(RegId regIdx, Emtd_tag newtag, Addr pc);

	void set_reg_tag_status(RegId regIdx, Emtd_status_tag tag);

	// Return an iterator to the memory tag map
	const std::map<memaddr_t, Emtd_tag> *get_mem_map_ptr() { return (const std::map<memaddr_t, Emtd_tag> *)&memory_tags; }

	void commit_insn(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData);
	void propagate_result_tag_o3(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData);

	void write_violation_stats();
	void record_violation(Addr pc, std::string msg, std::string pc_msg);

	bool isTainted(Addr pc);
	Emtd_InsnTaintEntry getInsnTaintEntry(Addr pc);

	// Encryption Helper Functions
	uint64_t 	get_enc_latency();
	void 		record_reg_update(RegId regIdx, bool is_fp_op, bool is_tainted, bool is_load);
	void  		void_reg_update(RegId regIdx, bool is_fp_op);
	uint64_t 	get_reg_update_time_cycles(RegId regIdx, bool is_fp_op);

private:
	std::string filename;
	std::string insfilename;
	std::string progname;
	uint64_t	clock_period;
	uint64_t 	enc_latency;
	// Addr libc_start;

private:
	// Some nice string representations of tags
	std::array<std::string, 5> EMTD_TAG_NAMES{{"DATA(0)", "CIPHERTEXT(1)", "UNTAGGED(2)"}};
	std::map<memaddr_t, Emtd_tag> memory_tags;		 // Current state of memory tags
													 // NOTE: Register's hold their own tags (in regfile)

	// Representation of insn taints / "enc_ins"
	std::map<memaddr_t, Emtd_InsnTaintEntry> insn_tags;  // Unordered_set chosen for efficiency 

	// // TODO:: Remove insns_consts_tags
	// std::map<memaddr_t, Emtd_tag> insns_consts_tags; // Used to find tag of destination register in insns

	std::map<uint16_t, uint64_t> int_reg_updates_ticks;	
	std::map<uint16_t, uint64_t> fp_reg_updates_ticks;	


	std::map<RegId, Emtd_tag> reg_tags;				  	// Register file tag
	std::map<RegId, Emtd_status_tag> reg_tags_status; 	// Register file tag

	std::map<Addr, int> pc_violation_counts;	   // Violation count
	std::map<Addr, std::string> pc_violation_type; // Violation descriptions

	void load_metadata_binary(const char *filename);				   // Populate initial mem tags and insns_const_tags
	void load_ins_taints(const char *filename);


	memaddr_t convert_byte_array_to_addr(uint8_t *byte_array);
	Emtd_tag convert_tagbyte_to_tag(uint8_t tagbyte);			// Helper function to mask bits in tagbyte
	Emtd_tag_type convert_tagbyte_to_tag_type(uint8_t tagbyte); // Helper function to get tag's type
	void initialize_reg_tags();									// Initialize tags for registers


	// Variables used to keep track of
	// stack frame size
	std::vector<uint64_t> base_sp, max_sp;

	// Clear a range of tags in the memory map (useful to clear the stack of tags...)
	void clear_mem_range_tags(memaddr_t bound1, memaddr_t bound2);

	// Stack handling
	void check_stack_pointer(ThreadContext *tc);
	void save_sp(ThreadContext *tc);

	// FUNCTIONS FOR ATOMIC CPU
	void deallocate_stack_tags();
	memaddr_t get_mem_addr(StaticInstPtr inst, Trace::InstRecord *traceData);

	// Helper variables used for metadata binary loading
	uint8_t tag_bits_mask;
	uint8_t tag_type_bits_mask;
};

#endif // _EMTD_METADATA_H
