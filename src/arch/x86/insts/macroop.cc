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
	
	//LB:: I think this constructor comes from ldstop.isa line 226, 260
	//	 	or line 100 of microldstop.hh
	StaticInstPtr injected = new X86ISAInst::Ld(
			machInst, 						//ExtMachInst _machInst
			"INJ_MOV_R_M", 					//const char * instMnem, in later versions this is just set to "injectedBranch"
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0 |  (1ULL << StaticInst::IsDataPrefetch), //uint64_t setFlags
			env.scale, 						// uint8_t _scale
			InstRegIndex(env.index), 		//InstRegIndex _index
			InstRegIndex(env.base), 		//InstRegIndex _base
			load_microop->getDisp() + 8,		// uint64_t _disp
			InstRegIndex(env.seg), 			//InstRegIndex _segment
			InstRegIndex(load_microop->destRegIdx(0).index()), 	// InstRegIndex _data
			env.dataSize, 					//uint8_t _dataSize
			env.addressSize, 				//uint8_t _addressSize
			0); 							//Request::FlagsType _memFlags
	injected->setInjected();
	result.push_back(injected);

	DPRINTF(csd, "Load Ins:: %s\n", load_microop->generateDisassembly(0, NULL));
	DPRINTF(csd, "Inj Ins:: %s\n", injected->generateDisassembly(0, NULL));	

	return result;

}


int
MacroopBase::cTXAlterMicroops()
{
	DPRINTF(csd, "MacroopBase::cTXAlterMicroops()\n");
	if(ctx_decoded==false){
		//Caculate total number of injected microops
		int new_numMicroops = numMicroops;
		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				new_numMicroops += 1;
			}
		}

		//Now inject microops
		numMicroops = new_numMicroops;
		StaticInstPtr* tempmicroops = new StaticInstPtr[numMicroops];

		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				std::vector <StaticInstPtr> toBeInjected = injectLoadMicros(microops[i]);	
				tempmicroops[i] = toBeInjected[0];
				tempmicroops[i+i] = microops[i];
				i += 1; //Skip injected microop
			}
			else {
				tempmicroops[i]=microops[i];
				tempmicroops[i]->clearLastMicroop();
			}
		}
		delete [] microops;
		microops = tempmicroops;
		microops[(numMicroops-1)]->setLastMicroop();
		ctx_decoded=true;
	}
	return 0;

}




}


