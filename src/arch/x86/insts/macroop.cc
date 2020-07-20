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
MacroopBase::cTXAlterMicroops()
{
	DPRINTF(csd, "MacroopBase::cTXAlterMicroops()\n");
	/*if(ctx_decoded==false){
		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				//relocate
				numMicroops+=1;
				StaticInstPtr*tempmicroops = new StaticInstPtr[numMicroops];
				for(int j=0;j<i+1;j++){
					tempmicroops[j]=microops[j];
					tempmicroops[j]->clearLastMicroop();
				}
				StaticInstPtr injected = new X86ISAInst::Ld(machInst,
						macrocodeBlock, (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0 |  (1ULL << StaticInst::IsDataPrefetch), env.scale, InstRegIndex(env.index),
						InstRegIndex(env.base), 0x4a3000, InstRegIndex(env.seg), InstRegIndex(NUM_INTREGS+0),
						4, 8, 0 | Request::PREFETCH );

				injected->setInjected();
				//injected->printFlags(std::cout," ");
				tempmicroops[i+1]=injected;
				for(int j=i+2;j<numMicroops;j++){
					tempmicroops[j]=microops[j-1];
				}
				i++;
				delete [] microops;
				microops = tempmicroops;

			}
		}
		microops[(numMicroops-1)]->setLastMicroop();
		ctx_decoded=true;
	}*/
	return 0;

}




}


