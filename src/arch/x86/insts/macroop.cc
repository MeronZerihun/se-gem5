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
			"INJ_MOV_R_M", 		//const char * instMnem, in later versions this is just set to "injectedBranch"
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
			env.scale, 						// uint8_t _scale
			InstRegIndex(env.index), 		//InstRegIndex _index
			InstRegIndex(env.base), 		//InstRegIndex _base
			load_microop->getDisp() + 8,		// uint64_t _disp
			InstRegIndex(env.seg), 			//InstRegIndex _segment
			InstRegIndex(NUM_INTREGS+1),//load_microop->destRegIdx(0).index()), 	// InstRegIndex _data
			env.dataSize, 					//uint8_t _dataSize
			env.addressSize, 				//uint8_t _addressSize
			0); 							//Request::FlagsType _memFlags
	injected->setInjected();
	//result[0] = injected;

	result.push_back(injected);
	//DPRINTF(csd, "Load Ins:: %s\n", load_microop->generateDisassembly(0, NULL));
	//DPRINTF(csd, "Inj Ins:: %s\n", injected->generateDisassembly(0, NULL));	

	// Constructor from line 85 of microregop.hh
	StaticInstPtr injected2 = new X86ISAInst::Dec(
			machInst, 						//ExtMachInst _machInst
			"INJ_DEC", 		//const char * instMnem, in later versions this is just set to "injectedBranch"
			(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
			InstRegIndex(env.index), 		//InstRegIndex _src1
			InstRegIndex(NUM_INTREGS+2), 		//InstRegIndex _src2
			InstRegIndex(env.seg), 			//InstRegIndex _dest
			env.dataSize, 					//uint8_t _dataSize
			env.addressSize); 				//uint16_t _ext
	injected2->setInjected();
	result.push_back(injected2);

	return result;

}


int
MacroopBase::cTXAlterMicroops()
{
	DPRINTF(csd, "MacroopBase::cTXAlterMicroops(): %d\n", ctx_decoded);
	if(ctx_decoded==false){
		//Caculate total number of injected microops
		int new_numMicroops = 0;

		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				new_numMicroops += 2;
			}
			else if(microops[i]->isStore()){
				DPRINTF(csd, "issa store\n");
			}
		}

		//Now inject microops
		if(new_numMicroops > 0){
		numMicroops += new_numMicroops;
		StaticInstPtr* tempmicroops = new StaticInstPtr[numMicroops];

		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{	
				DPRINTF(csd, "copying over load\n");
				std::vector<StaticInstPtr> toInject = injectLoadMicros(microops[i]);
				tempmicroops[i] = toInject.at(0); 
				tempmicroops[i]->clearLastMicroop();
				tempmicroops[i+1] = microops[i];
				tempmicroops[i+1]->clearLastMicroop();
				tempmicroops[i+2] = toInject.at(1);
				tempmicroops[i+2]->clearLastMicroop();
				i += 2; //Skip injected microop
				DPRINTF(csd, "end copy\n");
			}
			else {
				DPRINTF(csd, "copying over other\n");
				tempmicroops[i]=microops[i];
				tempmicroops[i]->clearLastMicroop();
				DPRINTF(csd, "end copy\n");
			}
		}
		DPRINTF(csd, "swapping mem\n");
		delete [] microops;
		microops = tempmicroops;
		DPRINTF(csd, "end swap\n");
		DPRINTF(csd, "%d\n", numMicroops);
		microops[(numMicroops-1)]->setLastMicroop();
		}
		DPRINTF(csd, "done\n");
		ctx_decoded=true;
	
	
		for(int i=0; i<numMicroops; i++){
        		DPRINTF(csd, "---- %s\n", microops[i]->generateDisassembly(0, NULL));
		}
	}
	return 0;

}




}


