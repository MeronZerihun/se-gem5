/*
 * macroop.cc
 *
 *  Created on: Feb 27, 2017
 *      Author: mkt
 */


#include "macroop.hh"
#include "arch/x86/generated/decoder.hh"
#include "arch/x86/isa_traits.hh"
#include <iostream>
#include "debug/csd.hh"
#include "arch/x86/regs/float.hh"
#include <string>

namespace X86ISA
{
MacroopBase::MacroopBase(const char *mnem, ExtMachInst _machInst,
		uint32_t _numMicroops, X86ISA::EmulEnv _env) :
            				X86StaticInst(mnem, _machInst, No_OpClass),
							numMicroops(_numMicroops), env(_env)
{
	assert(numMicroops);
	microops = new StaticInstPtr[numMicroops];
	flags[IsMacroop] = true;
	ctx_decoded=false;
}

MacroopBase::~MacroopBase()
{
	delete [] microops;
}



int 
MacroopBase::countLoadMicros (StaticInstPtr load_microop){

	X86ISA::InstRegIndex dest = InstRegIndex(load_microop->destRegIdx(0).index());
	X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(load_microop->srcRegIdx(1).index());
	}

	if (dest == ptr) {return 2;}

	return 1;
}

std::vector<StaticInstPtr>
MacroopBase::injectLoadMicros (StaticInstPtr load_microop){

	std::vector<StaticInstPtr> result;
	
	X86ISA::InstRegIndex dest = InstRegIndex(load_microop->destRegIdx(0).index());
	X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(load_microop->srcRegIdx(1).index());
	}
	std::string opName = load_microop->getName();

	if(dest == ptr){
	// MOV constructor originates from microregop.hh::85
		StaticInstPtr inj_mov = new X86ISAInst::Mov(
				machInst, 							//ExtMachInst _machInst
				"INJ_MOV", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				ptr, 								//InstRegIndex _src1
				ptr, 								//InstRegIndex _src2
				InstRegIndex(NUM_INTREGS+1), 		//InstRegIndex _dest
				env.dataSize, 						//uint8_t _dataSize
				0); 								//uint16_t _ext
		inj_mov->setInjected();
		inj_mov->clearLastMicroop();
		result.push_back(inj_mov);
		ptr = InstRegIndex(NUM_INTREGS+1); //New ptr is temp register
	}

	
	if(opName.compare("ld")==0){
		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_LD", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr existing_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"MOV_M_R",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_load->clearLastMicroop();
		result.push_back(existing_load);
	}
	else if(opName.compare("ldfp")==0){
		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr inj_load = new X86ISAInst::Ldfp(
				machInst, 							//ExtMachInst _machInst
				"INJ_LD", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr existing_load = new X86ISAInst::Ldfp(
				machInst, 							//ExtMachInst _machInst
				"MOV_M_R",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_load->clearLastMicroop();
		result.push_back(existing_load);
	}
	else{
		DPRINTF(csd, "WARNING:: OpName not handled by load injection in cTXAlterMicroops():: %s\n", opName);
	}

	return result;
}





int 
MacroopBase::countStoreMicros (StaticInstPtr store_microop){
	return 1;
}

std::vector<StaticInstPtr> 
MacroopBase::injectStoreMicros (StaticInstPtr store_microop){
	std::vector<StaticInstPtr> result;

       /*DPRINTF(csd, "TESTS:: %d\n", store_microop->destRegIdx(0).index());
       DPRINTF(csd, "TESTS:: %d\n", store_microop->destRegIdx(1).index());
       DPRINTF(csd, "TESTS:: %d\n", store_microop->srcRegIdx(0).index());
       DPRINTF(csd, "TESTS:: %d\n", store_microop->srcRegIdx(1).index());
       DPRINTF(csd, "TESTS:: %d\n", env.base);
       DPRINTF(csd, "TESTS:: %d\n", env.regm);
       DPRINTF(csd, "TESTS:: %d\n", env.reg);*/

    X86ISA::InstRegIndex dest = InstRegIndex(env.reg);
    X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(store_microop->srcRegIdx(1).index());
	}

	std::__cxx11::string diss = store_microop->generateDisassembly(0, NULL);
   	if(diss.find("_low") != std::string::npos){
		dest = InstRegIndex(FLOATREG_XMM_LOW(env.reg));
   	}
   	else if (diss.find("_high") != std::string::npos){
	   	dest = InstRegIndex(FLOATREG_XMM_HIGH(env.reg));
   	}

	std::string opName = store_microop->getName();
		
	//STORE constructor originates from microldstop.hh::100
	if(opName.compare("st")==0){
		// StaticInstPtr inj_store = new X86ISAInst::St(
		// 		machInst, 							//ExtMachInst _machInst
		// 		"INJ_ST",							//const char * instMnem
		// 		(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
		// 		env.scale, 							// uint8_t _scale
		// 		InstRegIndex(env.index), 			//InstRegIndex _index
		// 		ptr, 								//InstRegIndex _base
		// 		store_microop->getDisp() + 8,		// uint64_t _disp
		// 		InstRegIndex(env.seg), 				//InstRegIndex _segment
		// 		dest,								// InstRegIndex _data
		// 		env.dataSize, 						//uint8_t _dataSize
		// 		env.addressSize, 					//uint8_t _addressSize
		// 		0); 								//Request::FlagsType _memFlags
		// inj_store->clearLastMicroop();
		// result.push_back(inj_store);
		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_ST", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				InstRegIndex(NUM_INTREGS),			// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		StaticInstPtr existing_store = new X86ISAInst::St(
				machInst, 							//ExtMachInst _machInst
				"MOV_R_M",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_store->setInjected();
		existing_store->clearLastMicroop();
		result.push_back(existing_store);
	}
	else if(opName.compare("stfp")==0){
		/*StaticInstPtr inj_store = new X86ISAInst::Stfp(
				machInst, 							//ExtMachInst _machInst
				"INJ_ST",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_store->clearLastMicroop();
		result.push_back(inj_store);*/

		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_ST",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				InstRegIndex(NUM_INTREGS),		// InstRegIndex _data
				4, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
        inj_load->setInjected();
        inj_load->clearLastMicroop();
        result.push_back(inj_load);


		StaticInstPtr existing_store = new X86ISAInst::Stfp(
				machInst, 							//ExtMachInst _machInst
				"MOV_R_M",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_store->clearLastMicroop();
		result.push_back(existing_store);
	}
	else{
		DPRINTF(csd, "WARNING:: OpName not handled by store injection in cTXAlterMicroops():: %s\n", opName);
	}

	return result;
}




int
MacroopBase::cTXAlterMicroops(bool arith_tainted, bool mem_tainted, Addr pc)
{
	if(ctx_decoded==false){
		
		DPRINTF(csd, "MacroopBase::cTXAlterMicroops():: Altering microops of tainted instruction 0x%x\n", pc);

		//Caculate total number of injected microops
		int num_inj_microops = 0;
		for(int i=0;i<numMicroops;i++){
			DPRINTF(csd, "(OLD %d)-- %s\n", i, microops[i]->generateDisassembly(0, NULL));
			if(microops[i]->isLoad() && mem_tainted)
			{
				num_inj_microops += countLoadMicros(microops[i]);
			}
			else if(microops[i]->isStore() && mem_tainted){
				num_inj_microops += countStoreMicros(microops[i]);
			}
			else if(arith_tainted){
				switch(microops[i]->opClass()){
					case OpClass::IntAlu: microops[i]->setOpClass(OpClass::EncIntAlu); 
						break;
					case OpClass::IntMult: microops[i]->setOpClass(OpClass::EncIntMult); 
						break;
					case OpClass::IntDiv: microops[i]->setOpClass(OpClass::EncIntDiv); 
						break;
					case OpClass::FloatAdd: microops[i]->setOpClass(OpClass::EncFloatAdd); 
						break;
					case OpClass::FloatCmp: microops[i]->setOpClass(OpClass::EncFloatCmp); 
						break;
					case OpClass::FloatCvt: microops[i]->setOpClass(OpClass::EncFloatCvt); 
						break;
					case OpClass::FloatMult : microops[i]->setOpClass(OpClass::EncFloatMult); 
						break;
					case OpClass::FloatMultAcc : microops[i]->setOpClass(OpClass::EncFloatMultAcc); 
						break;
					case OpClass::FloatDiv : microops[i]->setOpClass(OpClass::EncFloatDiv); 
						break;
					case OpClass::FloatMisc : microops[i]->setOpClass(OpClass::EncFloatMisc); 
						break;
					case OpClass::FloatSqrt : microops[i]->setOpClass(OpClass::EncFloatSqrt); 
						break;
					case OpClass::SimdAddOp : microops[i]->setOpClass(OpClass::EncSimdAddOp); 				
						break;
					case OpClass::SimdAddAccOp : microops[i]->setOpClass(OpClass::EncSimdAddAccOp); 						
						break;
					case OpClass::SimdAluOp : microops[i]->setOpClass(OpClass::EncSimdAluOp); 					
						break;
					case OpClass::SimdCmpOp : microops[i]->setOpClass(OpClass::EncSimdCmpOp); 					
						break;
					case OpClass::SimdCvtOp : microops[i]->setOpClass(OpClass::EncSimdCvtOp); 					
						break;
					case OpClass::SimdMiscOp : microops[i]->setOpClass(OpClass::EncSimdMiscOp); 					
						break;
					case OpClass::SimdMultOp : microops[i]->setOpClass(OpClass::EncSimdMultOp); 					
						break;
					case OpClass::SimdMultAccOp : microops[i]->setOpClass(OpClass::EncSimdMultAccOp); 					
						break;
					case OpClass::SimdShiftOp : microops[i]->setOpClass(OpClass::EncSimdShiftOp); 					
						break;
					case OpClass::SimdShiftAccOp : microops[i]->setOpClass(OpClass::EncSimdShiftAccOp); 					
						break;
					case OpClass::SimdDivOp : microops[i]->setOpClass(OpClass::EncSimdDivOp); 					
						break;
					case OpClass::SimdSqrtOp : microops[i]->setOpClass(OpClass::EncSimdSqrtOp); 					
						break;
					case OpClass::SimdReduceAddOp : microops[i]->setOpClass(OpClass::EncSimdReduceAddOp); 					
						break;
					case OpClass::SimdReduceAluOp : microops[i]->setOpClass(OpClass::EncSimdReduceAluOp); 					
						break;
					case OpClass::SimdReduceCmpOp : microops[i]->setOpClass(OpClass::EncSimdReduceCmpOp); 					
						break;
					case OpClass::SimdFloatAddOp : microops[i]->setOpClass(OpClass::EncSimdFloatAddOp); 					
						break;
					case OpClass::SimdFloatAluOp : microops[i]->setOpClass(OpClass::EncSimdFloatAluOp); 					
						break;
					case OpClass::SimdFloatCmpOp : microops[i]->setOpClass(OpClass::EncSimdFloatCmpOp); 					
						break;
					case OpClass::SimdFloatCvtOp : microops[i]->setOpClass(OpClass::EncSimdFloatCvtOp); 					
						break;
					case OpClass::SimdFloatDivOp : microops[i]->setOpClass(OpClass::EncSimdFloatDivOp); 					
						break;
					case OpClass::SimdFloatMiscOp : microops[i]->setOpClass(OpClass::EncSimdFloatMiscOp); 					
						break;
					case OpClass::SimdFloatMultOp : microops[i]->setOpClass(OpClass::EncSimdFloatMultOp); 					
						break;
					case OpClass::SimdFloatMultAccOp : microops[i]->setOpClass(OpClass::EncSimdFloatMultAccOp); 			
						break;
					case OpClass::SimdFloatSqrtOp : microops[i]->setOpClass(OpClass::EncSimdFloatSqrtOp); 					
						break;
					case OpClass::SimdFloatReduceCmpOp : microops[i]->setOpClass(OpClass::EncSimdFloatReduceCmpOp); 			
						break;
					case OpClass::SimdFloatReduceAddOp : microops[i]->setOpClass(OpClass::EncSimdFloatReduceAddOp); 		
						break;											
					default: DPRINTF(csd, "WARNING:: OpClass not handled by switch in cTXAlterMicroops(): %d\n %s\n", microops[i]->opClass(), microops[i]->generateDisassembly(0, NULL));
						break;
				}
			}


		}
		
		//Perform microop injection
		if(num_inj_microops > 0){
			
			int temp_numMicroops = numMicroops + num_inj_microops;
			int temp_index = 0;
			StaticInstPtr* temp_microops = new StaticInstPtr[temp_numMicroops];

			for(int i=0;i<numMicroops;i++){
				if(microops[i]->isLoad()) {
					std::vector<StaticInstPtr> toInject = injectLoadMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						temp_microops[temp_index+j] = toInject.at(j); 
					}
					temp_index = temp_index + toInject.size(); //Skip injected microops
				}
				else if(microops[i]->isStore()){
					std::vector<StaticInstPtr> toInject = injectStoreMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						temp_microops[temp_index+j] = toInject.at(j); 
					}
					temp_index = temp_index + toInject.size(); //Skip injected microops	
				}
				else {
					temp_microops[temp_index]=microops[i];
					temp_microops[temp_index]->clearLastMicroop();
					temp_index++;
				}
			}
			delete [] microops;
			microops = temp_microops;
			numMicroops = temp_numMicroops;
			microops[(numMicroops-1)]->setLastMicroop();
		}
		ctx_decoded=true;
	
		for(int i=0; i<numMicroops; i++){
        		DPRINTF(csd, "(NEW %d)-- %s\n", i, microops[i]->generateDisassembly(0, NULL));
		}
	}
	return 0;

}




}


