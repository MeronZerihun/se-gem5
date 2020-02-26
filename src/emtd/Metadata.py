from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class Metadata(SimObject):
    type = 'Metadata'
    cxx_header = "emtd/Metadata.hh"
    filename = Param.String('', "EMTD metadata file name")   
    progname = Param.String('', "EMTD program file name") 
    libc_start = Param.Addr("PC of libc, any PCs below (<) are from the program")

