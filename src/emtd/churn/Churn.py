from m5.params import *
from m5.proxy import *
from MemObject import MemObject

class Churn(MemObject):
    type = 'Churn'
    cxx_header = "emtd/churn/churn.hh"
    mem_side = MasterPort("Memory side port, sends requests")
    cpu = Param.DerivO3CPU("DerivO3CPU")
    cache = Param.Cache("Cache")
    churnSleepCycles =  Param.Int(10000000 , "Number of CPU cycles to sleep before starting next churn cycle")
    enable_inst_reenc = Param.Bool(True, "Enables Instruction Re-Encryption")
    enable_ff = Param.Bool(False, "Enables fast-foward")
    
