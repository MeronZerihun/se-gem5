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



std::vector<StaticInstPtr>
MacroopBase::injectLoadMicros (StaticInstPtr load_microop){

	std::vector<StaticInstPtr> result;
	
	//LOAD constructor originates from microldstop.hh::100
	StaticInstPtr inj_load = new X86ISAInst::Ld(
			machInst, 							//ExtMachInst _machInst
			"INJ_MOV_R_M", 						//const char * instMnem
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
			env.scale, 							// uint8_t _scale
			InstRegIndex(env.index), 			//InstRegIndex _index
			InstRegIndex(env.base), 			//InstRegIndex _base
			load_microop->getDisp() + 8,		// uint64_t _disp
			InstRegIndex(env.seg), 				//InstRegIndex _segment
			//load_microop->destRegIdx(0).index()), 	// InstRegIndex _data
			InstRegIndex(NUM_INTREGS+1),		// InstRegIndex _data
			env.dataSize, 						//uint8_t _dataSize
			env.addressSize, 					//uint8_t _addressSize
			0); 								//Request::FlagsType _memFlags
	inj_load->setInjected();
	result.push_back(inj_load);


	// DEC constructor originates from microregop.hh::85
	StaticInstPtr inj_dec = new X86ISAInst::Dec(
			machInst, 							//ExtMachInst _machInst
			"INJ_DEC", 							//const char * instMnem
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
			InstRegIndex(env.index), 			//InstRegIndex _src1
			InstRegIndex(NUM_INTREGS+2), 		//InstRegIndex _src2
			InstRegIndex(env.seg), 				//InstRegIndex _dest
			env.dataSize, 						//uint8_t _dataSize
			0); 								//uint16_t _ext
	inj_dec->setInjected();
	result.push_back(inj_dec);

	return result;

}


int
MacroopBase::cTXAlterMicroops()
{
	if(ctx_decoded==false){

		//Caculate total number of injected microops
		int num_inj_microops = 0;
		for(int i=0;i<num_inj_microops;i++){
			if(microops[i]->isLoad())
			{
				num_inj_microops += 2;
			}
			else if(microops[i]->isStore()){
				DPRINTF(csd, "issa store\n");
			}
		}

		//Perform microop injection
		if(num_inj_microops > 0){
			numMicroops += num_inj_microops;
			StaticInstPtr* tempmicroops = new StaticInstPtr[numMicroops];

			for(int i=0;i<numMicroops;i++){
				if(microops[i]->isLoad())
				{	
					std::vector<StaticInstPtr> toInject = injectLoadMicros(microops[i]);
					tempmicroops[i] = toInject.at(0); 
					tempmicroops[i]->clearLastMicroop();
					tempmicroops[i+1] = microops[i];
					tempmicroops[i+1]->clearLastMicroop();
					tempmicroops[i+2] = toInject.at(1);
					tempmicroops[i+2]->clearLastMicroop();
					i += 2; //Skip injected microop
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


