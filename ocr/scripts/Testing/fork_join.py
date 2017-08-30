#!/usr/bin/env python

import OCR
OCR.parse_args(__file__)

prog = OCR.OCR_prog()

blocks     = 2#4
xes_per_blk= 4#8
db_size    = 1024
num_edts   = blocks*xes_per_blk*1#0
main_dbs   = [ OCR.DB("db_"+str(n),db_size) for n in range(0,num_edts) ]
child_edts = [ OCR.EDT("child"+str(i))      for i in range(0,xes_per_blk) ]
child_joiner = OCR.EDT("child_join", deps=child_edts)
comp_edts  = [ OCR.EDT("comp_"+str(n), deps=[main_dbs[n]], scope=[child_joiner,child_edts]) for n in range(0,num_edts) ]
joiner_edt = OCR.EDT("joiner", deps=comp_edts, finish=True)
main_edt   = OCR.EDT("main", scope=[
                    main_dbs,
                    comp_edts,
                    joiner_edt
             ])
print prog.to_str()

