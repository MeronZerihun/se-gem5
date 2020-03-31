/*
 * Handy header storing groupings of x86 operations, matching their opcodes
 *
 * Author: Lauren Biernacki (lbiernac@umich.edu)
 * Version: March 31, 2020
 */

#ifndef _X86_OP_GROUPS
#define _X86_OP_GROUPS

#include <array>

class X86Ops
{

private:
// List of x86 instructions grouped into catagories
std::array <std::string,7> emtd_memory_load_ops {{"lb","lh","lw","ld","lbu","lhu","lwu"}};
std::array <std::string,4> emtd_memory_store_ops {{"sb","sh","sw","sd"}};
std::array <std::string,21> emtd_reg_arith_ops {{"add","sub","sll","xor","srl",
									"sra","or","and","addw","subw",
									"sllw","srlw","sraw","mul","mulh",
									"mulhsu", "mulhu", "div","divu","rem","remu"}};
std::array <std::string,13> emtd_immed_arith_ops {{"lui",
								"auipc", 
 								"addi",
								"slli", 
 								"xori", 
 								"srli", 
 								"srai", 
 								"ori", 
 								"andi", 
 								"addiw", 
 								"slliw", 
 								"srliw", 
 								"sraiw"}};

std::array <std::string,4> emtd_cmp_ops {{"slt", 
 							  "sltu", 
							  "slti", 
							  "sltiu"}};

std::array <std::string,2> emtd_jump_ops {{"jalr", "jal"}};

std::array <std::string,6> emtd_branch_ops {{"beq", "bne", "blt", "bge", "bltu", "bgeu"}};

public:

bool is_memory_load_op(std::string opc);
bool is_memory_store_op(std::string opc);
bool is_reg_arith_op(std::string opc);
bool is_immed_arith_op(std::string opc);
bool is_cmp_op(std::string opc);
bool is_jump_op(std::string opc);
bool is_branch_op(std::string opc);
};

#endif // _X86_OP_GROUPS_H
