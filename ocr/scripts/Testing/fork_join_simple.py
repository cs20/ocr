#!/usr/bin/env python

import OCR
OCR.parse_args(__file__)

prog = OCR.OCR_prog()

blocks     = 4
xes_per_blk= 8
db_size    = 1024*32
num_edts   = blocks*xes_per_blk*10
main_dbs   = [ OCR.DB("db_"+str(n),db_size) for n in range(0,num_edts) ]
comp_edts  = [ OCR.EDT("comp_"+str(n), deps=[main_dbs[n]]) for n in range(0,num_edts) ]
main_edt   = OCR.EDT("main", scope=[
                    main_dbs,
                    comp_edts,
                    OCR.EDT("joiner", deps=comp_edts, finish=True)
             ])
print prog.to_str()

