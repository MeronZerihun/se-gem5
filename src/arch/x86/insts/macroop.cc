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
	if (dest == ptr) {return 2;}

	return 1;
}

std::vector<StaticInstPtr>
MacroopBase::injectLoadMicros (StaticInstPtr load_microop){

	std::vector<StaticInstPtr> result;
	
	X86ISA::InstRegIndex dest = InstRegIndex(load_microop->destRegIdx(0).index());
	X86ISA::InstRegIndex ptr = InstRegIndex(env.base);

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

	return result;
}





int 
MacroopBase::countStoreMicros (StaticInstPtr store_microop){
	return 1;
}

std::vector<StaticInstPtr> 
MacroopBase::injectStoreMicros (StaticInstPtr store_microop){
	std::vector<StaticInstPtr> result;

    X86ISA::InstRegIndex dest = InstRegIndex(store_microop->destRegIdx(0).index());
    X86ISA::InstRegIndex ptr = InstRegIndex(env.base);

	//STORE constructor originates from microldstop.hh::100
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
	existing_store->clearLastMicroop();
	result.push_back(existing_store);

	//STORE constructor originates from microldstop.hh::100
	StaticInstPtr inj_store = new X86ISAInst::St(
			machInst, 							//ExtMachInst _machInst
			"MOV_R_M",							//const char * instMnem
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
			env.scale, 							// uint8_t _scale
			InstRegIndex(env.index), 			//InstRegIndex _index
			ptr, 								//InstRegIndex _base
			store_microop->getDisp() + 8,		// uint64_t _disp
			InstRegIndex(env.seg), 				//InstRegIndex _segment
			dest,								// InstRegIndex _data
			env.dataSize, 						//uint8_t _dataSize
			env.addressSize, 					//uint8_t _addressSize
			0); 								//Request::FlagsType _memFlags
	inj_store->setInjected();
	inj_store->clearLastMicroop();
	result.push_back(inj_store);

	return result;
}




int
MacroopBase::cTXAlterMicroops()
{
	if(ctx_decoded==false){

		//Caculate total number of injected microops
		int num_inj_microops = 0;
		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				num_inj_microops += countLoadMicros(microops[i]);
			}
			else if(microops[i]->isStore()){
				num_inj_microops += countStoreMicros(microops[i]);
			}
		}
		
		//Perform microop injection
		if(num_inj_microops > 0){
			numMicroops += num_inj_microops;
			StaticInstPtr* tempmicroops = new StaticInstPtr[numMicroops];

			for(int i=0;i<numMicroops;i++){
				if(microops[i]->isLoad()) {	
					std::vector<StaticInstPtr> toInject = injectLoadMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						tempmicroops[i+j] = toInject.at(j); 
					}
					i = i + toInject.size() - 1; //Skip injected microops
				}
				else if(microops[i]->isStore()){
					std::vector<StaticInstPtr> toInject = injectStoreMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						tempmicroops[i+j] = toInject.at(j); 
					}
					i = i + toInject.size() - 1; //Skip injected microops	
				}
				else {
					tempmicroops[i]=microops[i];
					tempmicroops[i]->clearLastMicroop();
				}
			}
			delete [] microops;
			microops = tempmicroops;
			microops[(numMicroops-1)]->setLastMicroop();
		}
		ctx_decoded=true;
	
		for(int i=0; i<numMicroops; i++){
        		DPRINTF(csd, "---- %s\n", microops[i]->generateDisassembly(0, NULL));
		}
	}
	return 0;

}




}


