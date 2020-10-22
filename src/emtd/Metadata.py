from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class Metadata(SimObject):
    type = 'Metadata'
    cxx_header = "emtd/Metadata.hh"
    filename = Param.String('', "EMTD metadata file name")  
    insfilename = Param.String ('', "EMTD taint file name")
    progname = Param.String('', "EMTD program file name") 
    libc_start = Param.Addr("PC of libc, any PCs below (<) are from the program")
    clock = Param.Int(400, "Clock period in ticks, e.g. 2.5Ghz = 400 clock period")
    enc_latency = Param.Int(20, "Encryption latency in cycles")

