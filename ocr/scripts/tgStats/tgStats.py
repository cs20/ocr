#!/usr/bin/env python

from pprint import pprint

import sys
import os
import time
import numpy as np
import itertools
import subprocess

import Tkinter as tk

all_traffic = []

TOTAL_EP   = 0
STATIC_EP  = 1
DYNAMIC_EP = 2
NETWORK_EP = 3
MEMORY_EP  = 4

DRAM_ACCESS_COST = 6.0 #pJ
IPM_ACCESS_COST  = 6.0 #pJ
#TODO Add these when we get SL1 and SL2 mem costs
SL2_ACCESS_COST  = 1.0 #pJ
SL1_ACCESS_COST  = 1.0 #pJ


XES_PER_BLOCK = 8

TK_WIN_WIDTH  = 1200
TK_WIN_HEIGHT = 900


AGENTS = { 'CE':0, 'IPM':1, 'DRAM':2, 'L1SP':3, 'sL2':4, 'sL3':5, 'NVM':6, 'NLNI':7 }

ALL_TAGS = { 0 : "SL2", 1 : "CE", 2 : "XE0", 3 : "XE1", 4 : "XE2", 5 : "XE3", 6 : "XE4", 7 : "XE5",
            8 : "XE6", 9 : "XE7", 10 : "NLNI", 11 : "DRAM", 12: "IPM", 13 : "NVM"}

INV_ALL_TAGS = { val: key for key, val in ALL_TAGS.iteritems() }

DEST_TAGS = {0 : "SL2", 1 : "CE", 2 : "LOCAL SL1", 3 : "REMOTE SL1", 4 : "NLNI", 5 : "DRAM", 6 : "IPM", 7 : "NVM"}
INV_DEST_TAGS = { val: key for key, val in DEST_TAGS.iteritems() }

SRC_TAGS = { 0 : "CE", 1 : "XE0", 2 : "XE1", 3 : "XE2", 4 : "XE3", 5 : "XE4", 6 : "XE5",  7 : "XE6", 8 : "XE7", 9 : "NLNI"}
INV_SRC_TAGS = { val: key for key, val in SRC_TAGS.iteritems() }


BLOCK_TAGS = { 0 : "SL2", 1 : "CE", 2 : "XE0", 3 : "XE1", 4 : "XE2", 5 : "XE3", 6 : "XE4", 7 : "XE5", 8 : "XE6", 9 : "XE7", 10 : "NLNI"}
DRAM_TAGS = {0: "DRAM", 1: "CE", 2: "NLNI"}
IPM_TAGS = {0: "IPM", 1: "CE", 2: "NLNI"}
NVM_TAGS = {0: "NVM", 1: "CE", 2: "NLNI"}


ATOMICS = ["Unit XCHG", "Unit XINC", "Unit XADD", "Unit CMPXCHG"]
LOADS   = ["Unit Load", "Region Load"]
STORES  = ["Unit Store", "Region Store"]

MEM_STRUCTURES = ['NVM', 'IPM', 'DRAM', 'SL2', 'SL3', 'sL2', 'sL3']


class_memory = ['load64rr', 'load32rr', 'load16rr', 'load8rr', 'load64ri', 'load32ri', 'load16ri', 'load8ri', 'load64i', 'load32i',
                'load16i', 'load8i', 'load_pcrel64', 'load_pcrel32', 'load_pcrel16', 'load_pcrel8', 'load_idx64r', 'load_idx32r',
                'load_idx16r', 'load_idx8r', 'load_idx64i', 'load_idx32i', 'load_idx16i', 'load_idx8i', 'store64rr', 'store32rr',
                'store16rr', 'store8rr', 'store64ri', 'store32ri', 'store16ri', 'store8ri', 'store64i', 'store32i', 'store16i',
                'store8i', 'store_pcrel64', 'store_pcrel32', 'store_pcrel16', 'store_pcrel8', 'store_idx64r', 'store_idx32r',
                'store_idx16r', 'store_idx8r', 'store_idx64i', 'store_idx32i', 'store_idx16i', 'store_idx8i', 'storeack64rr',
                'storeack32rr', 'storeack16rr', 'storeack8rr', 'storeack64ri', 'storeack32ri', 'storeack16ri', 'storeack8ri',
                'storeack64i', 'storeack32i', 'storeack16i', 'storeack8i', 'storeack_pcrel64', 'storeack_pcrel32', 'storeack_pcrel16',
                'storeack_pcrel8', 'storeack_idx64r', 'storeack_idx32r', 'storeack_idx16r', 'storeack_idx8r', 'storeack_idx64i',
                'storeack_idx32i', 'storeack_idx16i', 'storeack_idx8i', 'loadmsr64r', 'loadmsr32r', 'loadmsr16r', 'loadmsr8r',
                'loadmsr64i', 'loadmsr32i', 'loadmsr16i', 'loadmsr8i', 'storemsr64r', 'storemsr32r', 'storemsr16r', 'storemsr8r',
                'storemsr64i', 'storemsr32i', 'storemsr16i', 'storemsr8i', 'dma_copy64', 'dma_copy32', 'dma_copy16', 'dma_copy8',
                'dma_copystride64', 'dma_copystride32', 'dma_copystride16', 'dma_copystride8', 'dma_copystride_2', 'dma_gsu64',
                'dma_gsu32', 'dma_gsu16', 'dma_gsu8', 'dma_gsu_2']

class_atomic = ['xaddI64rr', 'xaddI32rr', 'xaddI16rr', 'xaddI8rr', 'xaddI64r', 'xaddI32r', 'xaddI16r', 'xaddI8r', 'xaddI64i',
                'xaddI32i', 'xaddI16i', 'xaddI8i', 'xaddF64rr', 'xaddF32rr', 'xaddF16rr', 'xaddF8rr', 'xaddF64r', 'xaddF32r',
                'xaddF16r', 'xaddF8r', 'xaddF64i', 'xaddF32i', 'xaddF16i', 'xaddF8i', 'xincdecI64rr', 'xincdecI32rr', 'xincdecI16rr',
                'xincdecI8rr', 'xincdecI64r', 'xincdecI32r', 'xincdecI16r', 'xincdecI8r', 'xincdecI64i', 'xincdecI32i', 'xincdecI16i',
                'xincdecI8i', 'xincdecF64rr', 'xincdecF32rr', 'xincdecF16rr', 'xincdecF8rr', 'xincdecF64r', 'xincdecF32r', 'xincdecF16r',
                'xincdecF8r', 'xincdecF64i', 'xincdecF32i', 'xincdecF16i', 'xincdecF8i', 'xbit64op1rr', 'xbit32op1rr', 'xbit16op1rr',
                'xbit8op1rr', 'xbit64op1r', 'xbit32op1r', 'xbit16op1r', 'xbit8op1r', 'xbit64op1i', 'xbit32op1i', 'xbit16op1i',
                'xbit8op1i', 'xmaxF64rr', 'xmaxF32rr', 'xmaxF16rr', 'xmaxF8rr', 'xmaxF64r', 'xmaxF32r', 'xmaxF16r', 'xmaxF8r',
                'xmaxF64i', 'xmaxF32i', 'xmaxF16i', 'xmaxF8i', 'xmaxI64rr', 'xmaxI32rr', 'xmaxI16rr', 'xmaxI8rr', 'xmaxI64r', 'xmaxI32r',
                'xmaxI16r', 'xmaxI8r', 'xmaxI64i', 'xmaxI32i', 'xmaxI16i', 'xmaxI8i', 'xminF64rr', 'xminF32rr', 'xminF16rr', 'xminF8rr',
                'xminF64r', 'xminF32r', 'xminF16r', 'xminF8r', 'xminF64i', 'xminF32i', 'xminF16i', 'xminF8i', 'xminI64rr', 'xminI32rr',
                'xminI16rr', 'xminI8rr', 'xminI64r', 'xminI32r', 'xminI16r', 'xminI8r', 'xminI64i', 'xminI32i', 'xminI16i', 'xminI8i',
                'xchg64rr', 'xchg32rr', 'xchg16rr', 'xchg8rr', 'xchg64r', 'xchg32r', 'xchg16r', 'xchg8r', 'xchg64ri', 'xchg32ri',
                'xchg16ri', 'xchg8ri', 'cmpxchg64rr', 'cmpxchg32rr', 'cmpxchg16rr', 'cmpxchg8rr', 'cmpxchg64r', 'cmpxchg32r',
                'cmpxchg16r', 'cmpxchg8r', 'cmpxchg64i', 'cmpxchg32i', 'cmpxchg16i', 'cmpxchg8i', 'icmpxchg64rr', 'icmpxchg32rr',
                'icmpxchg16rr', 'icmpxchg8rr', 'icmpxchg64r', 'icmpxchg32r', 'icmpxchg16r', 'icmpxchg8r', 'icmpxchg64i', 'icmpxchg32i',
                'icmpxchg16i', 'icmpxchg8i']


class_float = [ 'fmaF64', 'fmaF32', 'fmaF16', 'fmaF8', 'fmsF64', 'fmsF32', 'fmsF16', 'fmsF8', 'mulF64r', 'mulF32r', 'mulF16r', 'mulF8r',
                'mulF64i', 'mulF32i', 'mulF16i', 'mulF8i', 'divF64r', 'divF32r', 'divF16r', 'divF8r', 'divF64i', 'divF32i', 'divF16i',
                'divF8i', 'addF64r', 'addF32r', 'addF16r', 'addF8r', 'addF64i', 'addF32i', 'addF16i', 'addF8i', 'subF64r', 'subF32r',
                'subF16r', 'subF8r', 'subF64i', 'subF32i', 'subF16i', 'subF8i', 'negF64', 'negF32', 'negF16', 'negF8', 'rcpF64', 'rcpF32',
                'rcpF16', 'rcpF8', 'log2F64', 'log2F32', 'log2F16', 'log2F8', 'log10F64', 'log10F32', 'log10F16', 'log10F8', 'sqrtF64',
                'sqrtF32', 'sqrtF16', 'sqrtF8', 'rcpsqrtF64', 'rcpsqrtF32', 'rcpsqrtF16', 'rcpsqrtF8', 'sincosF64', 'sincosF32', 'sincosF16',
                'sincosF8', 'expF64', 'expF32', 'expF16', 'expF8']

class_control = ['jctir', 'jctii', 'jcfir', 'jcfii', 'jctrr', 'jctri', 'jcfrr', 'jcfri', 'jrelr', 'jreli', 'jabsr', 'jabsi', 'jlabsr', 'jlabsi', 'jlrelr', 'jlreli']

class_integer = ['fmaI64', 'fmaI32', 'fmaI16', 'fmaI8', 'fmsI64', 'fmsI32', 'fmsI16', 'fmsI8', 'mulI64r', 'mulI32r', 'mulI16r', 'mulI8r',
                 'mulI64i', 'mulI32i', 'mulI16i', 'mulI8i', 'divU64r', 'divU32r', 'divU16r', 'divU8r', 'divU64i', 'divU32i', 'divU16i',
                 'divU8i', 'addI64r', 'addI32r', 'addI16r', 'addI8r', 'addI64i', 'addI32i', 'addI16i', 'addI8i', 'subI64r', 'subI32r',
                 'subI16r', 'subI8r', 'subI64i', 'subI32i', 'subI16i', 'subI8i', 'negI64', 'negI32', 'negI16', 'negI8', 'divS64r',
                 'divS32r', 'divS16r', 'divS8r', 'divS64i', 'divS32i', 'divS16i', 'divS8i', 'mulhU64r', 'mulhU32r', 'mulhU16r', 'mulhU8r',
                 'mulhU64i', 'mulhU32i', 'mulhU16i', 'mulhU8i', 'mulhS64r', 'mulhS32r', 'mulhS16r', 'mulhS8r', 'mulhS64i', 'mulhS32i',
                 'mulhS16i', 'mulhS8i', 'remU64r', 'remU32r', 'remU16r', 'remU8r', 'remU64i', 'remU32i', 'remU16i', 'remU8i', 'remS64r',
                 'remS32r', 'remS16r', 'remS8r', 'remS64i', 'remS32i', 'remS16i', 'remS8i']


class_logical = ['bit64op1r', 'bit32op1r', 'bit16op1r', 'bit8op1r', 'bit64op1i', 'bit32op1i', 'bit16op1i', 'bit8op1i',
                 'bit64op2r', 'bit32op2r', 'bit16op2r', 'bit8op2r', 'bit64op2i', 'bit32op2i', 'bit16op2i', 'bit8op2i']

class_bitwise = ['bselect64', 'bselect32', 'bselect16', 'bselect8', 'select64', 'select32', 'select16', 'select8', 'bset64r', 'bset32r',
                 'bset16r', 'bset8r', 'bset64i', 'bset32i', 'bset16i', 'bset8i', 'bclr64r', 'bclr32r', 'bclr16r', 'bclr8r', 'bclr64i',
                 'bclr32i', 'bclr16i', 'bclr8i', 'bcnt64', 'bcnt32', 'bcnt16', 'bcnt8', 'clz64', 'clz32', 'clz16', 'clz8', 'ctz64',
                 'ctz32', 'ctz16', 'ctz8', 'brev64', 'brev32', 'brev16', 'brev8', 'sll64r', 'sll32r', 'sll16r', 'sll8r', 'sll64i', 'sll32i',
                 'sll16i', 'sll8i', 'slr64r', 'slr32r', 'slr16r', 'slr8r', 'slr64i', 'slr32i', 'slr16i', 'slr8i', 'sar64r', 'sar32r',
                 'sar16r', 'sar8r', 'sar64i', 'sar32i', 'sar16i', 'sar8i', 'rol64r', 'rol32r', 'rol16r', 'rol8r', 'rol64i', 'rol32i',
                 'rol16i', 'rol8i', 'ror64r', 'ror32r', 'ror16r', 'ror8r', 'ror64i', 'ror32i', 'ror16i', 'ror8i', 'btrans64',
                 'btrans32', 'btrans16', 'btrans8', 'bmatmul64', 'bmatmul32', 'bmatmul16', 'bmatmul8']

class_misc = ['nop', 'cvtXtoY', 'movimm64', 'movimm32', 'movimm16', 'movimm8', 'movimmshf32', 'barrier_init', 'barrier_wait',
              'barrier_poll', 'reduce_wait', 'reduce_poll', 'reduce_minU64', 'reduce_minU32', 'reduce_minU16', 'reduce_minU8',
              'reduce_minS64', 'reduce_minS32', 'reduce_minS16', 'reduce_minS8', 'mcast_r8', 'mcast_r16', 'mcast_r32', 'mcast_r64',
              'mcast_m8', 'mcast_m16', 'mcast_m32', 'mcast_m64', 'alarm1', 'delay', 'cache_wball', 'cache_wbrange', 'cache_wbl',
              'cache_invall', 'cache_invrange', 'cache_invl', 'cache_wbinvall', 'cache_wbinvrange', 'cache_wbinvl', 'cache_pf',
              'cache_upd64', 'cache_upd32', 'cache_upd16', 'cache_upd8', 'cmp64i', 'cmp32i', 'cmp16i', 'cmp8i', 'cmpimm64', 'cmpimm32',
              'cmpimm16', 'cmpimm8', 'fence', 'fencepc', 'fencepc_rel', 'flush', 'swint', 'swret', 'cli', 'sti', 'lea', 'chain_init64',
              'chain_init32', 'chain_init16', 'chain_init8', 'chain_end', 'chain_poll64', 'chain_poll32', 'chain_poll16', 'chain_poll8',
              'chain_wait64', 'chain_wait32', 'chain_wait16', 'chain_wait8', 'chain_kill64', 'chain_kill32', 'chain_kill16', 'chain_kill8',
              'fake4_64', 'fake4_32', 'fake4_16', 'fake4_8', 'fake3_64', 'fake3_32', 'fake3_16', 'fake3_8', 'fake2_64', 'fake2_32',
              'fake2_16', 'fake2_8', 'fake1_64', 'fake1_32', 'fake1_16', 'fake1_8', 'fake0_64', 'fake0_32', 'fake0_16', 'fake0_8',
              'reduce_maxU64', 'reduce_maxU32', 'reduce_maxU16', 'reduce_maxU8', 'reduce_maxS64', 'reduce_maxS32', 'reduce_maxS16',
              'reduce_maxS8', 'movimmF64', 'reduce_minF64', 'reduce_minF32', 'reduce_maxF64', 'reduce_maxF32', 'reduce_mulF64',
              'reduce_mulF32', 'reduce_mulI64', 'reduce_mulI32', 'reduce_mulI16', 'reduce_mulI8', 'reduce_addI64', 'reduce_addI32',
              'reduce_addI16', 'reduce_addI8', 'qma_add1_w64', 'qma_add1_w32', 'qma_add1_w16', 'qma_add1_w8', 'qma_add1_n64',
              'qma_add1_n32', 'qma_add1_n16', 'qma_add1_n8', 'qma_rem1_w64', 'qma_rem1_w32', 'qma_rem1_w16', 'qma_rem1_w8',
              'qma_rem1_n64', 'qma_rem1_n32', 'qma_rem1_n16', 'qma_rem1_n8', 'qma_rem2_w64', 'qma_rem2_w32', 'qma_rem2_w16', 'qma_rem2_w8',
              'qma_rem2_n64', 'qma_rem2_n32', 'qma_rem2_n16', 'qma_rem2_n8', 'qma_add2_w64', 'qma_add2_w32', 'qma_add2_w16', 'qma_add2_w8',
              'qma_add2_n64', 'qma_add2_n32', 'qma_add2_n16', 'qma_add2_n8', 'qma_addX_w64r', 'qma_addX_w32r', 'qma_addX_w16r',
              'qma_addX_w8r', 'qma_addX_w64i', 'qma_addX_w32i', 'qma_addX_w16i', 'qma_addX_w8i', 'qma_addX_n64r', 'qma_addX_n32r',
              'qma_addX_n16r', 'qma_addX_n8r', 'qma_addX_n64i', 'qma_addX_n32i', 'qma_addX_n16i', 'qma_addX_n8i', 'qma_remX_w64r',
              'qma_remX_w32r', 'qma_remX_w16r', 'qma_remX_w8r', 'qma_remX_w64i', 'qma_remX_w32i', 'qma_remX_w16i', 'qma_remX_w8i',
              'qma_remX_n64r', 'qma_remX_n32r', 'qma_remX_n16r', 'qma_remX_n8r', 'qma_remX_n64i', 'qma_remX_n32i', 'qma_remX_n16i',
              'qma_remX_n8i', 'qma_sizeX64', 'qma_sizeX32', 'qma_sizeX16', 'qma_sizeX8', 'reduce_addF64', 'reduce_addF32',
              'reduce_bitop1_8', 'reduce_bitop1_16', 'reduce_bitop1_32', 'reduce_bitop1_64']



class NETWORK:

    def __init__(self, ID, blockId, level, grid):
        self.ID = ID
        self.blockId = blockId
        self.level = level
        self.grid = grid

class XE:

    def __init__(self, ID, block, localReads, remoteReads, localWrites, remoteWrites, incNetWrites, incNetReads, localAtomics, remoteAtomics, instructions):
        self.ID = ID
        self.block = block
        self.localReads = localReads
        self.remoteReads = remoteReads
        self.localWrites = localWrites
        self.remoteWrites = remoteWrites
        self.incNetWrites = incNetWrites
        self.incNetReads = incNetReads
        self.localAtomics = localAtomics
        self.remoteAtomics = remoteAtomics
        self.instructions = instructions

class POWER_STAT:

    def __init__(self, agent, blockId, ID, time, energy, power, speed, instructions, wall):
        self.agent = agent
        self.blockId = blockId
        self.ID = ID
        self.time = time
        self.energy = energy
        self.power = power
        self.speed = speed
        self.instructions = instructions
        self.time = time
        self.wall = wall


def calculateHeatColor(minV, maxV, val):
    ratio = 2*(val-(float(minV))) / (float(maxV) - float(minV))
    r = int(max(0, 255*(1-ratio)))
    b = int(max(0, 255*(ratio-1)))
    g = 255-b-r
    return r, g, b


class trafficGrid(object):

    def __init__(self, master, **kwargs):

        global allTraffic
        frame = tk.Frame(master)

        #Initialize Canvas
        self.canvas = tk.Canvas(master, width=TK_WIN_WIDTH, height=TK_WIN_HEIGHT, borderwidth=0, highlightthickness=0)
        self.canvas.pack(side="top", fill="both", expand="true")
        self.rows = len(SRC_TAGS)
        self.columns = len(DEST_TAGS)
        self.cellwidth = (TK_WIN_WIDTH/float(len(DEST_TAGS)+1))
        self.cellheight = (TK_WIN_HEIGHT/float(len(SRC_TAGS)+1))
        self.checked = []
        self.rect = {}
        self.text = {}

        #Display heatmap
        self.drawHeatmap(allTraffic)

    def drawHeatmap(self, allTraffic):

        maxCount = 0
        minCount = sys.maxint
        #Find MIN and MAX for heat color reference
        for i in xrange(len(allTraffic)):
            for j in xrange(len(allTraffic[0])):
                if(allTraffic[i][j] > maxCount):
                    maxCount = allTraffic[i][j]
                if(allTraffic[i][j] < minCount):
                    minCount = allTraffic[i][j]

        #Draw each grid square, apply number value, and calculate heat color
        for column in range(len(allTraffic[0])+1):
            for row in range(len(allTraffic)+1):
                x1 = column*self.cellwidth
                y1 = row * self.cellheight
                x2 = x1 + self.cellwidth
                y2 = y1 + self.cellheight
                if(row == 0 or column == 0):
                    self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill="gray", tags="rect")
                    if(row == 0 and column == 0):
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text="", tags = "text")
                    elif(row == 0 and column != 0):
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=str(DEST_TAGS[column-1]), tags = "text")
                    else:
                        self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=SRC_TAGS[row-1], tags = "text")
                else:
                    #self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill="grey", tags="rectMax")
                    #self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=str(allTraffic[column-1][row-1], tags = "text")
                    if(allTraffic[row-1][column-1] == 0):
                        heatColor = 'grey'
                    else:
                        heatColor = '#%02x%02x%02x' % (calculateHeatColor(maxCount, minCount, allTraffic[row-1][column-1]))
                    if(allTraffic[row-1][column-1] == maxCount):
                        self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rectMax")
                    elif(allTraffic[row-1][column-1] == minCount):
                        self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rectMin")
                    else:
                        self.rect[row,column] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, tags="rect2")
                    self.text[row,column] = self.canvas.create_text((x2+x1)/2, (y2+y1)/2, text=allTraffic[row-1][column-1], tags = "text")




#Finds the largest int in a string (for network diagram)
def maxInt(rec):
    front = -1
    allInts = []

    string = rec.replace("[", '')
    string = string.replace("]", '')

    for i in range(len(string)):
        if front == -1:
            if string[i].isdigit():
                front = i
            else:
                continue
        else:
            if string[i].isdigit():
                continue
            else:
                allInts.append(int(string[front:i+1]))
                front = -1

    return max(allInts)


#Initialize 2D list at specified dimension (for network object construction)
def init2dGrid(dimension):
    return [[0 for i in range(dimension+1)] for j in range(dimension+1)]

def initTrafficGrid(xDim, yDim):
    return [[0 for i in range(yDim+1)] for j in range(xDim+1)]


#Populate the network objects grid (simple 2D list)
def populateNetworkGrid(obj, logRecs, lower, upper):

    labelsStripped = []

    for i in range(lower, upper):
        if i == lower:
            continue
        else:
            labelsStripped.append(logRecs[i].split()[4:])

    for i in range(len(labelsStripped)):
        for j in range(len(labelsStripped[i])):
            obj.grid[i][j] = labelsStripped[i][j]


#Get file name stripped of path
def getCurFileName(fname):
    fStr = ""
    rev = fname[::-1]

    for i in rev:
        if i != '/':
            fStr += i
        else:
            break

    return fStr[::-1]


#Get block number of associated machine component
def getComponentBlockNumber(compType, fname):
    f = getCurFileName(fname)
    if compType == "XE" or compType == "NETWORK":
        f = f.replace(".", " ")
        blkStr = f.split()
        for sub in blkStr:
            if "blk" in sub:
                return sub[3:]


#Get XE number
def getComponentId(compType, fname):
    if compType == "XE":
        idStr = ""
        rev = fname[::-1]

        for i in rev:
            if i != '.':
                idStr += i
            else:
                break

        return idStr[::-1]

    else:
        return 0


#Get machine component
def getNetworkMachineLevel(fname):
    f = getCurFileName(fname)
    f = f.replace(".", " ")
    if "drm" in f:
        return "DRAM"
    elif "blk" in f:
        return "BLOCK"
    elif "nvm" in f:
        return "NVM"
    elif "ipm" in f:
        return "IPM"
    elif "sl3" in f:
        return "SL3"
    else:
        return None

def getNetworkAgents(netObj, srcTag, dstTag):

    if srcTag not in INV_SRC_TAGS or (srcTag == "NLNI" and dstTag not in INV_DEST_TAGS):
        return -1, -1
    if "XE" in srcTag or "CE" in srcTag:
        srcIdx = INV_SRC_TAGS[srcTag]
        if srcTag == dstTag:
            dstIdx = INV_DEST_TAGS["LOCAL SL1"]
        elif "XE" in dstTag:
            dstIdx = INV_DEST_TAGS["REMOTE SL1"]
        else:
            dstIdx = INV_DEST_TAGS[dstTag]

        return srcIdx, dstIdx

    else:
        return INV_SRC_TAGS[srcTag], INV_DEST_TAGS[dstTag]

#Get string identifying the instructions type associated with network grid
def extractNetGridID(gridString):
    #Sanity check - Make sure we are parsing the right log record
    assert("Message Grid Counts" in gridString)
    stripped = gridString.split()[:-3]
    return ' '.join(stripped)


#Create objects for each record observed in the energy output files
def processEnergyStats(cmdPath, energy_files):

    #Totals done, Calculate power per agent.
    powerObjects = []
    global allTraffic

    for f in energy_files:
        if f[-3] == "swp":
            continue
        fp = open(f, 'r')
        lines = fp.readlines()
        fp.close()

        blockNum = -1

        for i in range(0, len(lines)):
            rec = lines[i].split()
            #String and character match to assure we are taking the correct Agent record
            if len(rec)>1 and rec[0] == 'outfile':
                blockNum = getComponentBlockNumber('XE', rec[2])
            if len(rec) > 1 and rec[0] == "Agent" and rec[1][0] == '#':
                tagStr = rec[2]
                tagStr = tagStr.replace('(', ' ')
                tagStr = tagStr.replace(':', ' ')
                tagStr = tagStr.replace(')', ' ')
                #curTag = int(tagStr.split()[0])

                curTag = tagStr.split()[1]
                curID = tagStr.split()[2]
                obj = POWER_STAT(curTag, blockNum, curID, 0, [], [], 0, 0, 0)

                #Power records found, iterate until the next record; Storing values for current agent
                for j in range(i+1, len(lines)):
                    subRec = lines[j].split()
                    if len(subRec) > 1 and rec[0] == "Agent" and subRec[1][0] == '#':
                        break
                    elif 'simulated time'   in lines[j]:
                        obj.time = float(subRec[3])
                    elif 'energy usage'     in lines[j]:
                        if curTag == "DRAM":
                            dram = 0
                            for i in range(len(allTraffic)):
                                for j in range(len(allTraffic[i])):
                                    if DEST_TAGS[j] == "DRAM":
                                        dram += allTraffic[i][j]
                            memory = float(dram)*DRAM_ACCESS_COST
                            static  = float(subRec[6])
                            dynamic = float(subRec[9])
                            network = float(subRec[12])
                            total = memory+static+dynamic+network

                        elif curTag == "IPM":
                            ipm = 0
                            for i in range(len(allTraffic)):
                                for j in range(len(allTraffic[i])):
                                    if DEST_TAGS[j] == "IPM":
                                        ipm += allTraffic[i][j]
                            memory = float(ipm)*IPM_ACCESS_COST
                            static  = float(subRec[6])
                            dynamic = float(subRec[9])
                            network = float(subRec[12])
                            total = memory+static+dynamic+network
#                        TODO Add this when we get SL1 and SL2 mem costs
#                        elif curTag == "XE":
#                        elif curTag == "SL2":
                        else:
                            total   = float(subRec[3])
                            static  = float(subRec[6])
                            dynamic = float(subRec[9])
                            network = float(subRec[12])
                            memory  = float(subRec[15])
                        obj.energy = [total, static, dynamic, network, memory]
                    elif 'power usage'      in lines[j]:
                        total   = float(subRec[4])
                        static  = float(subRec[7])
                        dynamic = float(subRec[10])
                        network = float(subRec[13])
                        memory = float(subRec[16])
                        obj.power = [total, static, dynamic, network, memory]
                    elif 'wall-clock'       in lines[j]:
                        obj.instructions = int(subRec[1].replace(',',''))
                        obj.wall = int(subRec[4].replace(',',''))
                    elif 'simulation speed' in lines[j]:
                        if 'AVERAGE' in lines[j]:
                            continue
                        else:
                            obj.speed = float(subRec[3].replace('\'',''))

                powerObjects.append(obj)

    return powerObjects


#Calculate sum total of each power category
def calcPowerTotals(power_objects):
    timing = [0 for i in range(3)]
    energy = [0 for i in range(5)]
    power  = [0 for i in range(5)]

    maxTime = 0
    maxWall = 0
    for i in power_objects:
        if i.time > maxTime:
            maxTime = i.time
        if i.wall > maxWall:
            maxWall = i.wall

        timing[0] += i.instructions

        energy[TOTAL_EP] += i.energy[TOTAL_EP]
        energy[STATIC_EP] += i.energy[STATIC_EP]
        energy[DYNAMIC_EP] += i.energy[DYNAMIC_EP]
        energy[NETWORK_EP] += i.energy[NETWORK_EP]
        energy[MEMORY_EP] += i.energy[MEMORY_EP]

        power[TOTAL_EP] += i.power[TOTAL_EP]
        power[STATIC_EP] += i.power[STATIC_EP]
        power[DYNAMIC_EP] += i.power[DYNAMIC_EP]
        power[NETWORK_EP] += i.power[NETWORK_EP]
        power[MEMORY_EP] += i.power[MEMORY_EP]

    timing[2] = maxTime
    timing[1] = maxWall

    return [timing, energy, power]


#Write spreadsheet breakdown of power/agent
def writeEnergyCsv(power_objects, blocks):
    agents = {}
    for i in power_objects:
        if i.agent not in agents:
            agents[i.agent] = [[i.energy, i.blockId, i.ID]]
        else:
            agents[i.agent].append([i.energy, i.blockId, i.ID])

    fp = open(os.getcwd()+'/results/energy_breakdown.csv',  'w')
    fp.write(',Total,Static,Dynamic,Network,Memory\n')


    xes = ['' for i in range(8*len(set(blocks)))]
    non_xes = ['' for i in range(8)]
    L1SP = 0
    NLNI_dyn = 0
    #Need to compute XEs first, since there's no ordering guarantee with dictionaries.
    for i in agents:
        name = i
        total   = 0
        static  = 0
        dynamic = 0
        network = 0
        memory =  0
        ID = 0
        for j in agents[i]:
            total   += j[0][TOTAL_EP]
            static  += j[0][STATIC_EP]
            dynamic += j[0][DYNAMIC_EP]
            network += j[0][NETWORK_EP]
            memory  += j[0][MEMORY_EP]
            blk_num=j[1]
            ID = j[2]
            if i == 'XE':
                assert(j[1] != None)
                xes[(8*int(blk_num))+int(ID)] = ("BLK"+str(blk_num)+'-'+str(i)+str(ID)+','+str(total)+','+str(static)+','+str(dynamic)+',0.0,0.0\n')
                L1SP += memory
                NLNI_dyn += network
                total   = 0
                static  = 0
                dynamic = 0
                network = 0
                memory  = 0
            else:
                continue

    for i in agents:

        if i == 'XE':
            continue

        name = i
        total   = 0
        static  = 0
        dynamic = 0
        network = 0
        memory =  0
        ID = 0
        for j in agents[i]:
            total   += j[0][TOTAL_EP]
            static  += j[0][STATIC_EP]
            dynamic += j[0][DYNAMIC_EP]
            network += j[0][NETWORK_EP]
            memory  += j[0][MEMORY_EP]
            blk_num=j[1]
            ID = j[2]

        if i == 'CE':
            non_xes[AGENTS[i]] = (str(i)+','+str(total)+','+str(static)+','+str(dynamic)+',0.0,0.0\n')
            L1SP += memory
            NLNI_dyn += network

        elif i != 'XE':
            if i in MEM_STRUCTURES:
                non_xes[AGENTS[i]] = (str(i)+','+str(total)+','+str(static)+','+str(memory)+','+str(network)+','+'0.0\n')
            elif i == 'NLNI':
                non_xes[AGENTS[i]] = (str(i)+','+str(total)+','+str(static)+','+str(NLNI_dyn)+','+str(network)+','+str(memory)+'\n')
            else:
                non_xes[AGENTS[i]] = (str(i)+','+str(total)+','+str(static)+','+str(dynamic)+','+str(network)+','+str(memory)+'\n')

    non_xes[AGENTS['L1SP']] = 'L1SP,0,0,'+str(L1SP)+',0,0\n'

    for i in xes:
        fp.write(i)
    for i in non_xes:
        fp.write(i)

    fp.close()

    xe_stat = xe_dyna = ce_stat = ce_dyna = 0
    for i in power_objects:
        if i.agent == 'XE':
            xe_stat += i.energy[STATIC_EP]
            xe_dyna += i.energy[DYNAMIC_EP]
        elif i.agent == 'CE':
            ce_stat += i.energy[STATIC_EP]
            ce_dyna += i.energy[DYNAMIC_EP]
    total_comp = xe_stat + ce_stat + xe_dyna + ce_dyna


    xe_mem = ce_mem = sl2_mem = sl3_mem = ipm_mem = nvm_mem = dram_mem = 0
    xe_net = ce_net = 0
    sl2_stat = sl3_stat = ipm_stat = nvm_stat = dram_stat = 0

    for i in power_objects:
        if i.agent == "XE":
            xe_mem += i.energy[MEMORY_EP]
            xe_net += i.energy[NETWORK_EP]
        elif i.agent == "CE":
            ce_mem += i.energy[MEMORY_EP]
            ce_net += i.energy[NETWORK_EP]
        elif i.agent == "sL2":
            sl2_mem += i.energy[MEMORY_EP]
            sl2_stat += i.energy[STATIC_EP]
        elif i.agent == "sL3":
            sl3_mem += i.energy[MEMORY_EP]
            sl3_stat += i.energy[STATIC_EP]
        elif i.agent == "DRAM":
            dram_mem += i.energy[MEMORY_EP]
            dram_stat += i.energy[STATIC_EP]
        elif i.agent == "IPM":
            ipm_mem += i.energy[MEMORY_EP]
            ipm_stat += i.energy[STATIC_EP]
        elif i.agent == "NVM":
            nvm_mem += i.energy[MEMORY_EP]
            nvm_stat += i.energy[STATIC_EP]

    total_dyn_mem = xe_mem + ce_mem + sl2_mem + sl3_mem + ipm_mem + nvm_mem + dram_mem
    total_net = xe_net + ce_net
    total_stat_mem = sl2_stat + sl3_stat + ipm_stat + nvm_stat + dram_stat

    total_non_comp = total_dyn_mem + total_net + total_stat_mem

    fp = open('results/energy_per_agent.txt', 'w')


    fp.write('\n======== Computational Energy ========\n\n')

    #total
    fp.write('\tTotal: '+str(total_comp)+' pJ\n\n')
    fp.write('\tXE Dynamic: '+str(xe_dyna)+' pJ  [ '+ '%.3f' % ((xe_dyna/float(total_comp))*100)+'% ]\n')
    fp.write('\tCE Dynamic: '+str(ce_dyna)+' pJ  [ '+ '%.3f' % ((ce_dyna/float(total_comp))*100)+'% ]\n')
    fp.write('\tXE Static: '+str(xe_stat)+' pJ  [ '+ '%.3f' % ((xe_stat/float(total_comp))*100)+'% ]\n')
    fp.write('\tCE Static: '+str(ce_stat)+' pJ  [ '+ '%.3f' % ((ce_stat/float(total_comp))*100)+'% ]\n')

    fp.write('\n======== Non-computational Energy ========\n\n')

    fp.write('\tTotal: '+str(total_non_comp)+' pJ\n\n')

    fp.write('\n\t======== Network Energy ========\n\n')

    fp.write('\tXE: '+str(xe_net)+' pJ  [ '+ '%.3f' % ((xe_net/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tCE: '+str(ce_net)+' pJ  [ '+ '%.3f' % ((ce_net/float(total_non_comp))*100)+'% ]\n')

    fp.write('\n\t======== Dynamic Memory Energy ========\n\n')

    fp.write('\tXE (L1SP): '+str(xe_mem)+' pJ  [ '+ '%.3f' % ((xe_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tCE (L1SP): '+str(ce_mem)+' pJ  [ '+ '%.3f' % ((ce_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tSL2: '+str(sl2_mem)+' pJ  [ '+ '%.3f' % ((sl2_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tSL3: '+str(sl3_mem)+' pJ  [ '+ '%.3f' % ((sl3_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tIPM: '+str(ipm_mem)+' pJ  [ '+ '%.3f' % ((ipm_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tNVM: '+str(nvm_mem)+' pJ  [ '+ '%.3f' % ((nvm_mem/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tDRAM: '+str(dram_mem)+' pJ  [ '+ '%.3f' % ((dram_mem/float(total_non_comp))*100)+'% ]\n')

    fp.write('\n\t======== Static Memory Energy ========\n\n')

    fp.write('\tSL2: '+str(sl2_stat)+' pJ  [ '+ '%.3f' % ((sl2_stat/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tSL3: '+str(sl3_stat)+' pJ  [ '+ '%.3f' % ((sl3_stat/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tIPM: '+str(ipm_stat)+' pJ  [ '+ '%.3f' % ((ipm_stat/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tNVM: '+str(nvm_stat)+' pJ  [ '+ '%.3f' % ((nvm_stat/float(total_non_comp))*100)+'% ]\n')
    fp.write('\tDRAM: '+str(dram_stat)+' pJ  [ '+ '%.3f' % ((dram_stat/float(total_non_comp))*100)+'% ]\n')

    fp.close()


#Write relevant PMU counters on a per XE basis
def processXeStats(XE_files):

    XE_stat_objs = []

    for f in XE_files:
        #Sanity check for swap files TODO same for tilde files
        if f[-3:] == "swp":
            continue

        fp = open(f, 'r')
        lines = fp.readlines()
        fp.close()
        #TODO get rack, cube, socket, cluster, IDs as well
        ID = getComponentId("XE", f)
        block = getComponentBlockNumber("XE", f)
        curXE = XE(ID, block, 0, 0, 0, 0, 0, 0, 0, 0, 0)

        for i in range(0, len(lines)):
            if lines[i] == "Performance Monitoring Unit:\n":
                #iterate over PMU counters, extracting counter
                for j in range(i+1, len(lines)):

                    pmu = lines[j].split()

                    if   pmu[0] == "LOCAL_READ_COUNT":
                        curXE.localReads = int(pmu[2])
                    elif pmu[0] == "REMOTE_READ_COUNT":
                        curXE.remoteReads = int(pmu[2])
                    elif pmu[0] == "LOCAL_WRITE_COUNT":
                        curXE.localWrites = int(pmu[2])
                    elif pmu[0] == "REMOTE_WRITE_COUNT":
                        curXE.remoteWrites = int(pmu[2])
                    elif pmu[0] == "NET_ARRIVED_WRITE_COUNT":
                        curXE.incNetWrites = int(pmu[2])
                    elif pmu[0] == "NET_ARRIVED_READ_COUNT":
                        curXE.incNetReads = int(pmu[2])
                    elif pmu[0] == "LOCAL_ATOMICS_COUNT":
                        curXE.localAtomics = int(pmu[2])
                    elif pmu[0] == "REMOTE_ATOMICS_COUNT":
                        curXE.remoteAtomics = int(pmu[2])
                    elif pmu[0] == "INSTRUCTIONS_EXECUTED":
                        curXE.instructions = int(pmu[2])
                    #New PMU counters go here
                    #This below tag is where PMU counters end in file so we break.
                    elif pmu[0] == "NET_ICMPXCHG_COUNT":
                        break
                break
        XE_stat_objs.append(curXE)

    return XE_stat_objs


def processNetworkStats(network_files):
    global allTraffic
    network_stat_objs = []

    for f in network_files:
        machLevel = getNetworkMachineLevel(f)
        if machLevel == None:
            continue
        if machLevel == "BLOCK":
            blockId = getComponentBlockNumber("NETWORK", f)
        else:
            blockId = None

        fp = open(f, 'r')
        lines = fp.readlines()
        fp.close

        for i in range(0, len(lines)):
            if lines[i] == "Network results for traffic grid\n":
                for j in range(i, len(lines)):
                    if "Message Grid Counts\n" in lines[j]:
                        ID = extractNetGridID(lines[j])
                        dim = maxInt(lines[j+1])
                        grid = init2dGrid(dim)

                        netObj = NETWORK(ID, blockId, machLevel, grid)

                        populateNetworkGrid(netObj, lines, (j+1), (j+dim+3))
                        network_stat_objs.append(netObj)

    #TODO: Unused for now, but ready to go if we need to differentiate L/S/A
    totalLoads   = initTrafficGrid(len(SRC_TAGS)-1, len(DEST_TAGS)-1)
    #TODO: Unused for now, but ready to go if we need to differentiate L/S/A
    totalStores  = initTrafficGrid(len(SRC_TAGS)-1, len(DEST_TAGS)-1)
    #TODO: Unused for now, but ready to go if we need to differentiate L/S/A
    totalAtomics = initTrafficGrid(len(SRC_TAGS)-1, len(DEST_TAGS)-1)

    cumulative   = initTrafficGrid(len(SRC_TAGS)-1, len(DEST_TAGS)-1)

    for i in network_stat_objs:
        for j in range(len(i.grid)):
            for k in range(len(i.grid)):

                numBytes = int(i.grid[j][k])

                if i.level == "BLOCK":
                    srcTag = BLOCK_TAGS[j]
                    dstTag = BLOCK_TAGS[k]
                    srcIdx, dstIdx = getNetworkAgents(i, srcTag, dstTag)
                    if srcIdx == -1:
                        continue
                    if i.ID in LOADS:
                        totalLoads[srcIdx][dstIdx]+=numBytes
                    elif i.ID in STORES:
                        totalStores[srcIdx][dstIdx]+=numBytes
                    elif i.ID in ATOMICS:
                        totalAtomics[srcIdx][dstIdx]+=numBytes
                elif i.level == "DRAM":
                    srcTag = DRAM_TAGS[j]
                    dstTag = DRAM_TAGS[k]
                    srcIdx, dstIdx = getNetworkAgents(i, srcTag, dstTag)
                    if srcIdx == -1:
                        continue
                    if i.ID in LOADS:
                        totalLoads[srcIdx][dstIdx]+=numBytes
                    elif i.ID in STORES:
                        totalStores[srcIdx][dstIdx]+=numBytes
                    elif i.ID in ATOMICS:
                        totalAtomics[srcIdx][dstIdx]+=numBytes
                elif i.level == "IPM":
                    srcTag = IPM_TAGS[j]
                    dstTag = IPM_TAGS[k]
                    srcIdx, dstIdx = getNetworkAgents(i, srcTag, dstTag)
                    if srcIdx == -1:
                        continue
                    if i.ID in LOADS:
                        totalLoads[srcIdx][dstIdx]+=numBytes
                    elif i.ID in STORES:
                        totalStores[srcIdx][dstIdx]+=numBytes
                    elif i.ID in ATOMICS:
                        totalAtomics[srcIdx][dstIdx]+=numBytes
                elif i.level == "NVM":
                    srcTag = NVM_TAGS[j]
                    dstTag = NVM_TAGS[k]
                    srcIdx, dstIdx = getNetworkAgents(i, srcTag, dstTag)
                    if srcIdx == -1:
                        continue
                    if i.ID in LOADS:
                        totalLoads[srcIdx][dstIdx]+=numBytes
                    elif i.ID in STORES:
                        totalStores[srcIdx][dstIdx]+=numBytes
                    elif i.ID in ATOMICS:
                        totalAtomics[srcIdx][dstIdx]+=numBytes

    for i in range(len(SRC_TAGS)):
        for j in range(len(DEST_TAGS)):
            cumulative[i][j] += (totalStores[i][j] + totalLoads[i][j] + totalAtomics[i][j])

    ceSrc = INV_SRC_TAGS["CE"]
    ceDst = INV_DEST_TAGS["CE"]

    cumulative[ceSrc][ceDst] += cumulative[ceSrc][INV_DEST_TAGS["LOCAL SL1"]]

    allTraffic = cumulative

    return network_stat_objs


#Calculate instructions/XE, and percentage totals per category
def processInstructionCounts(blocks, xe_files):

    searchTitle = "Executed instruction counts"
    hist = {'class_memory':0 , 'class_atomic':0 , 'class_float':0 , 'class_control':0 , 'class_integer':0 , 'class_logical':0 , 'class_bitwise':0 , 'class_misc':0}
    histList = []

    for i in range(len(list(set(blocks)))):
        histList.append([{} for i in range(len(xe_files)/len(list(set(blocks))))])


    for i in range(len(list(set(blocks)))):

        for f in xe_files:

            if int(getComponentBlockNumber('XE', f)) != i:
                continue

            xeID = getComponentId("XE", f)
            running = {'class_memory':0 , 'class_atomic':0 , 'class_float':0 , 'class_control':0 , 'class_integer':0 , 'class_logical':0 , 'class_bitwise':0 , 'class_misc':0}
            title_found = False
            fd = open(f, 'r')
            lines = fd.readlines()
            for line in lines:

                if title_found:
                    if((line.split()[0]).split(":")[0] in class_memory):
                        if int(line.split()[1]) != 0:
                            hist['class_memory'] += int(line.split()[1])
                            running['class_memory'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_atomic):
                        if int(line.split()[1]) != 0:
                            hist['class_atomic'] += int(line.split()[1])
                            running['class_atomic'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_float):
                        if int(line.split()[1]) != 0:
                            hist['class_float'] += int(line.split()[1])
                            running['class_float'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_control):
                        if int(line.split()[1]) != 0:
                            hist['class_control'] += int(line.split()[1])
                            running['class_control'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_integer):
                        if int(line.split()[1]) != 0:
                            hist['class_integer'] += int(line.split()[1])
                            running['class_integer'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_logical):
                        if int(line.split()[1]) != 0:
                            hist['class_logical'] += int(line.split()[1])
                            running['class_logical'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_bitwise):
                        if int(line.split()[1]) != 0:
                            hist['class_bitwise'] += int(line.split()[1])
                            running['class_bitwise'] += int(line.split()[1])
                    elif((line.split()[0]).split(":")[0] in class_misc):
                        if int(line.split()[1]) != 0:
                            hist['class_misc'] += int(line.split()[1])
                            running['class_misc'] += int(line.split()[1])
                    # New class code goes here
                    else:
                        #Simple check should suffice to omit erroneous fsim shutdown strings from reporting false warnings
                        if len(line.split()) != 2:
                            pass
                        else:
                            print "Warning: Inst " + (line.split()[0]).split(":")[0]+ "not part of isa !\n"
                if searchTitle in line:
                    title_found = True

            fd.close()
            if title_found == False :
                print " Error : Log file is missing supllied search string in file: "+str(f)+".\n Turn TG_TRACE_DUMP_STATS trace on and recapture logs !"
            else:
                histList[i][int(xeID)] = running

    return hist, histList


#Write ISA intruction per XE breakdown and summary
def writeInstructionCsv(per_xe_instruction_stats):
    fp = open(os.getcwd()+'/results/instruction_breakdown.csv', 'w')
    fp.write(',memory,atomic,float,control,integer,logical,bitwise,misc\n')

    for i in range(len(per_xe_instruction_stats)):

        for j in range(len(per_xe_instruction_stats[0])):
            xe = per_xe_instruction_stats[i][j]
            fp.write('BLK0'+str(i)+'-XE'+str(j)+','+str(xe['class_memory'])+','+str(xe['class_atomic'])+','+str(xe['class_float'])+',')
            fp.write(str(xe['class_control'])+','+str(xe['class_integer'])+','+str(xe['class_logical'])+',')
            fp.write(str(xe['class_bitwise'])+','+str(xe['class_misc'])+'\n')
    fp.close()


#Write L/S traffic grid to csv
def writeTrafficCsv():
    fp = open(os.getcwd()+'/results/net_traffic.csv', 'w')
    fp.write(',')
    for i in range(len(DEST_TAGS)):
        fp.write(DEST_TAGS[i]+',')
    fp.write('\n')

    for i in range(len(allTraffic)):
        for j in range(len(allTraffic[i])):
            if j == 0:
                fp.write(SRC_TAGS[i]+',')
            fp.write(str(allTraffic[i][j])+',')
        fp.write('\n')
    fp.close()


#Write PMU counters, and energy totals
def writeAggregateData(uniqBlocks, xe, energy, instr):


#===========================   XE_STATS  ===========================#
    #Print XE stats for each block
    #Pretty inefficient, but tg's limitations will keep these lists from growing very big
    for block in uniqBlocks:
        fp = open(os.getcwd()+"/results/xe_pmu_counters_summary_block_"+str(block)+".txt", 'w')
        fp.write("============================================================================\n")
        fp.write("================================   BLOCK: "+str(block)+"   =============================\n" )
        fp.write("============================================================================\n\n")
        for i in range(XES_PER_BLOCK):

            for j in xe:

                #Check types make sure what is being compared are the same type
                if int(j.ID) == int(i) and int(j.block) == int(block):

                    fp.write('\t============== XE: '+str(i)+' ==============\n')
                    fp.write('\tLocal Reads = '+str(j.localReads)+"\n")
                    fp.write('\tRemote Reads = '+str(j.remoteReads)+"\n")
                    fp.write('\tLocal Writes = '+str(j.localWrites)+"\n")
                    fp.write('\tRemote Writes = '+str(j.remoteWrites)+"\n")
                    fp.write('\tWrites Arriving Over Network = '+str(j.incNetWrites)+"\n")
                    fp.write('\tReads Arriving Over Network = '+str(j.incNetReads)+"\n")
                    fp.write('\tLocal Atomics = '+str(j.localAtomics)+"\n")
                    fp.write('\tRemote Atomics = '+str(j.remoteAtomics)+"\n")
                    fp.write('\tInstructions = '+str(j.instructions)+"\n")
                    fp.write('\n\n\n')
        fp.close()
#===================================================================#



#===========================    ENERGY   ===========================#
    if energy != None:
        fp = open(os.getcwd()+"/results/approximate_power_and_energy.txt", 'w')
        timeStats = energy[0]
        energyStats = energy[1]
        powerStats = energy[2]

        fp.write("=== Simulation Time Values ===\n\n")
        fp.write("\tTotal Instructions: "+str(timeStats[0])+"\n")
        fp.write("\tWall clock time (usec): "+str(timeStats[1])+"\n")
        fp.write("\tCPU time?? (need to double check with Samkit) (usec): "+str(timeStats[2])+"\n\n\n")

        fp.write(" === Approximate Energy Statistics ===\n\n")
        fp.write("\tTotal Energy (pJ): "+str(energyStats[TOTAL_EP])+"\n")
        fp.write("\tStatic Energy (pJ): "+str(energyStats[STATIC_EP])+"\n")
        fp.write("\tDynamic Energy (pJ): "+str(energyStats[DYNAMIC_EP])+"\n")
        fp.write("\tNetwork Attributed Energy (pJ): "+str(energyStats[NETWORK_EP])+"\n")
        fp.write("\tMemory Attributed Energy (pJ): "+str(energyStats[MEMORY_EP])+"\n\n\n")

        fp.write(" === Approximate Power Statistics ===\n\n")
        fp.write("\tTotal Power (pW): "+str(powerStats[TOTAL_EP])+"\n")
        fp.write("\tStatic Power (pW): "+str(powerStats[STATIC_EP])+"\n")
        fp.write("\tDynamic Power (pW): "+str(powerStats[DYNAMIC_EP])+"\n")
        fp.write("\tNetwork Attributed Power (pW): "+str(powerStats[NETWORK_EP])+"\n")
        fp.write("\tMemory Attributed Power (pW): "+str(powerStats[MEMORY_EP])+"\n\n\n")

        fp.close()
#===================================================================#



#==========================  INSTRUCTIONS  =========================#
    if instr != None:
        fp = open(os.getcwd()+"/results/isa_instruction_summary.txt", 'w')
        totalInstr = sum(instr.values())
        fp.write(" === Instruction Classification Summary ===\n\n\n")
        fp.write("\tTotal Instructions Executed: "+str(totalInstr)+"\n\n")
        for i in instr:
            key  = i
            val  = instr[i]
            perc = (val/float(totalInstr))*100
            fp.write("\t"+str(key)[6:]+": "+str(val)+"  [ "+ '%.2f' % perc+"% ]\n")

        fp.close()
#===================================================================#


#Calculate pJ/Inst and write alongside power summary
def calcPPW(blocks, power_objects, xe_stats, energy_stats, instruction_stats):

    dyn_energy = [[] for i in range(len(blocks))]
    instr_cts  = [[]for i in range(len(blocks))]

    for i in range(len(blocks)):
        block_energy = [0 for ID in range(XES_PER_BLOCK)]
        block_cts    = [0 for ID in range(XES_PER_BLOCK)]
        for j in range(XES_PER_BLOCK):
            for k in power_objects:
                if k.agent != 'XE':
                    continue
                if int(k.blockId) == i and int(k.ID) == j:
                    block_energy[j] = k.energy[DYNAMIC_EP]


            block_cts[j] = sum(instruction_stats[i][j].values())

        dyn_energy[i] = block_energy
        instr_cts[i]  = block_cts

    fp = open('results/approximate_power_and_energy.txt', 'a')
    fp.write('\n=============== Energy(pJ)/Instruction ===============\n\n')

    overall_energy = 0
    overall_instr  = 0

    for i in range(len(blocks)):
        fp.write('\n====== BLOCK: '+str(i)+' ======\n\n')
        for j in range(XES_PER_BLOCK):
            pjw = float(dyn_energy[i][j])/float(instr_cts[i][j])
            fp.write("\tXE"+str(j)+": "+ '%.4f' % pjw+" pJ/Instruction\n")

        totalpjw = float(sum(dyn_energy[i]))/float(sum(instr_cts[i]))
        overall_energy += sum(dyn_energy[i])
        overall_instr += sum(instr_cts[i])
        fp.write('\n\tTotal: '+'%.4f' % totalpjw+" pJ/Instruction\n")


    overall_total = float(overall_energy)/float(overall_instr)
    fp.write('\n\nOverall Total: '+'%.4f' % overall_total+" pJ/Instruction\n")


#============ MAIN ==============
def main():

    #TODO arg parser - decide on user options. Just reading/dumping everything right now
    if len(sys.argv) == 2:
        tgLogPath = sys.argv[1]
        drawGrid = False
    elif len(sys.argv) == 3 and sys.argv[2] == '-h':
        tgLogPath = sys.argv[1]
        drawGrid = True
    else:
        print "\nERROR: Incorrect Usage\n"
        print "Correct Usage: python tgStats.py <path_to_tg_log_files_parent_directory> <options>\n"
        print "\t Current Options:"
        print "\t\t-h :   display inter-agent payload traffic heatmap (in bytes)\n"
        sys.exit(0)

    #Avoid I/O errors by ensuring log directory path is suffixed with '/'
    if tgLogPath[-1] != '/':
        tgLogPath+='/'

    XE_files = []
    energy_files = []
    network_files = []

    #Create lists of all relevant files per machine component
    for filename in os.listdir(tgLogPath):
        if "XE" in filename and "EVENT_STATS" not in filename:
            XE_files.append(tgLogPath+filename)

        elif "std.out" in filename:
            energy_files.append(tgLogPath+filename)

        elif "network" in filename and "EVENT_STATS" not in filename:
            if filename[-7:] == "network":
                network_files.append(tgLogPath+filename)

    #Gather agent to agent traffic data
    net_stats = processNetworkStats(network_files)

    #Gather XE PMU counters
    xe_stats = processXeStats(XE_files)

    #Get Unique block ID list from all XE objects
    blocks = []
    for obj in xe_stats:
        blocks.append(int(obj.block))
    blocks = list(set(blocks))

    #Create a directory for summary files
    if not os.path.exists(str(os.getcwd())+'/results'):
        os.mkdir(str(os.getcwd())+'/results')

    #Gather timing, energy, and power data
    power_objects = processEnergyStats(tgLogPath, energy_files)
    energy_stats = calcPowerTotals(power_objects)

    writeEnergyCsv(power_objects, blocks)

    #Gather ISA instruction data
    instruction_stats, per_xe_instruction_stats = processInstructionCounts(blocks, XE_files)
    writeInstructionCsv(per_xe_instruction_stats)


    #Aggregate data and create output files
    writeAggregateData(blocks, xe_stats, energy_stats, instruction_stats)

    calcPPW(blocks, power_objects, xe_stats, energy_stats, per_xe_instruction_stats)

    writeTrafficCsv()

    if drawGrid == True:
        print "Loading network heatmap..."
        root = tk.Tk()
        t = trafficGrid(root)
        root.mainloop()


if __name__ == "__main__":
    main()
