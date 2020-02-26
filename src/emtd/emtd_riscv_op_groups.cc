/*
 * Handy header storing groupings of RISC-V operations, matching their opcodes
 *
 * Author: Mark Gallagher (markgall@umich.edu)
 * Version: September 8, 2017
 */
#include <string>
#include "emtd/emtd_riscv_op_groups.hh"


bool RISCVOps::is_memory_load_op(std::string opc){
	for(int i=0; i<emtd_memory_load_ops.size(); i++){
		if(emtd_memory_load_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_memory_store_op(std::string opc){
	for(int i=0; i<emtd_memory_store_ops.size(); i++){
		if(emtd_memory_store_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_reg_arith_op(std::string opc){
	for(int i=0; i<emtd_reg_arith_ops.size(); i++){
		if(emtd_reg_arith_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_immed_arith_op(std::string opc){
	for(int i=0; i<emtd_immed_arith_ops.size(); i++){
		if(emtd_immed_arith_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_cmp_op(std::string opc){
	for(int i=0; i<emtd_cmp_ops.size(); i++){
		if(emtd_cmp_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_jump_op(std::string opc){
	for(int i=0; i<emtd_jump_ops.size(); i++){
		if(emtd_jump_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}

bool RISCVOps::is_branch_op(std::string opc){
	for(int i=0; i<emtd_branch_ops.size(); i++){
		if(emtd_branch_ops[i].compare(opc) == 0)
			return true;
	}
	return false;
}


