import Tkinter as tk
import sys
import os
from subprocess import Popen, PIPE
import signal
import threading
import multiprocessing
import Queue
import math
import copy
import time

from operator import itemgetter

#Log string parsing indices
TIME_INDEX = -1
ALLOC_SIZE_INDEX = 1
FREE_SIZE_INDEX  = 0
ALLOC_ADDR_INDEX = 3
FREE_ADDR_INDEX  = 6

#Default GUI window dimensions - future TODO: Make configurable
WINDOW_X = 1200
WINDOW_Y = 900

#Global data structures for sanitized allocation records
L1 = [[]]
L2 = []
L3 = []
DRAM = []
IPM  = []
NVM  = []

ALLOC_STRLEN = 6

#Default pool size values - will be overwritten if configured in logged FSIM app
CE_L1_SIZE = 64000
XE_L1_SIZE = 64000
L2_SIZE    = 2048000
DRAM_SIZE  = 16384000000
IPM_SIZE   = 640000000
NVM_SIZE   = 128000000

ALLOC_ID_STR = 'ALLOCATING'
FREE_ID_STR  = 'FREEING'
MEM_SIZE_STR = 'quickBegin'

DATA_FILE     = 'mem.txt'
SIZE_FILE     = 'size.txt'
SHELL_IN_FILE = 'shellData.txt'


class memMap(object):
    def __init__(self, master, **kwargs):
        frame = tk.Frame(master)
        #Initialize Canvas
        self.canvas = tk.Canvas(master, width=WINDOW_X, height=WINDOW_Y, borderwidth=0, highlightthickness=0)
        self.canvas.pack(side="top", fill="both", expand="true")
        self.cellwidth = 2
        self.cellheight = 2
        self.rect = {}
        self.text = {}

        self.drawHeatmap()


    def hexToDec(self, hexVal):
        return int(hexVal, 16)


    #Compute address of first alloc, watermark, and
    #create a serialized data structure with memory
    #addresses represented as decimal values
    def flattenMemory(self, mem):

        #Return empty values indicating no allocations were made to this memory structure
        if len(mem) == 0:
            return 0, 0, []

        addr = []
        dec  = []
        #Create list of all hex addresses and corresponding decimal values
        for i in mem:
            dec.append(self.hexToDec(i[2]))

        #Determine watermark
        maxSpan = 0
        for i in mem:
            address = self.hexToDec(i[2])
            size    = int(i[1])
            if address+size > maxSpan:
                maxSpan = address+size

        total = (maxSpan) - min(dec)
        flat = {}
        #Initialize dictionary representing all addresses between the lowest address and the watermark
        for i in range(total):
            flat[i] = 0
        return min(dec), total, flat


    #Populate {address : memory access count} dictionary
    def getAllocCounts(self, mem, addrMap, offset):

        localCopy = copy.copy(addrMap)

        for i in range(len(mem)):
            if mem[i][0] == 'Allocate':
                size    = mem[i][1]
                curAddress = self.hexToDec(mem[i][2]) - offset
                for j in range(i, i+int(size)):
                    localCopy[curAddress]+=1
                    curAddress+=1

        return localCopy

    #Associate contiguous memory addresses with pixels as needed
    def consolidateMemory(self, mem, watermark, area, size, test):

        consolidatedMem = {}
        watermarkPixel = int(area)
        watermarkPixelFound = False
        for i in range(int(area)+1):
            consolidatedMem[i] = 0

        #Compute how many addresses will be represented per pixel
        addrPerPixel = (size/float(area))
        addrPPHigh = math.ceil(size/float(area))

        #Get number after decimal - needed for toggle mechanism if addresses per pixel is not a whole number
        dec = float(("%.3f" % float(str(addrPerPixel-int(addrPerPixel))[1:])))*1000

        #Setup a toggle mechanism to map addresses to pixels equally
        toggle = False
        toggleCount = 0
        toggleOffset = 0
        pixel = 0
        curPixelCount = 0

        #Iterate over each pixel in current memory structure
        for i in range(int(area)+1):
            #if addrPerPixel is a whole number, do not actuate toggle mechanism
            if dec == 0:
                consolidatedMem[i] = (i+1)*addrPerPixel

            #toggle
            else:
                if toggle == False:
                    consolidatedMem[i] = (i+1)*addrPPHigh
                else:
                    consolidatedMem[i] = (((i+1)*addrPPHigh) - toggleOffset)
                if consolidatedMem[i] > watermark:
                    if not watermarkPixelFound:
                        watermarkPixel = i
                        watermarkPixelFound = True

                toggleCount+=1
                #Once toggle reaches 1000, switch off
                if toggleCount == 1000:
                    toggle == False
                    toggleCount = 0
                #Once toggle value is reached, switch on
                if toggleCount >= dec:
                    if toggle == False:
                        toggle = True
                    toggleOffset += 1

        return consolidatedMem, watermarkPixel


    #Calculate heat color of a pixel based on allocation counts
    def getHeatColor(self, x, y, minV, maxV, rowLen, memAllocs, pixelMap, pixelCount, test):

        #If this holds, no allocs were made, return no fill
        if maxV == 0:
            return 255, 255, 255

        #Get the pixel relative to current memory structure
        curPixel = (x*rowLen)+y

        allocsInPixel = 0
        #Check how many allocs associeted with current pixel
        try:
            allocsInPixel = pixelCount[curPixel]
        except:
            pass

        #No memory alloced @ current pixel, return no fill
        if allocsInPixel == 0:
            return 255, 255, 255

        ratio = 2*(allocsInPixel-(float(maxV))) / (float(minV) - float(maxV))
        r = int(max(0, 255*(1-ratio)))
        b = int(max(0, 255*(ratio-1)))
        g = 255-b-r

        return r, g, b


    #Calculate the minimum, and maximum allocation counts across all pixels in a memory structure
    def getMinMaxHeatVals(self, memAllocs, watermark, pixelMap, test):
        #Return empty values without going through loop of no memory was allocated in mem structure during app
        if len(memAllocs) == 0:
            return {}, 0, 0

        allocCounts  = {}
        addrPerPixel = pixelMap[0]
        high = 0
        low = -1
        #Iterate through each pixel relative to current memory structure
        for i in pixelMap:
            if i > watermark:
                allocCounts[i] = 0
                continue

            if low != -1:
                low = high
            else:
                low = 0

            high = pixelMap[i]
            curAllocCount = 0
            #Iterate through alloc counts at each address associated with current pixel - add the running counter
            for j in range(int(low), int(high)):
                try:
                    curAllocCount+=memAllocs[j]
                except:
                    pass

            #Add key value pair {pixel : allocCount}
            allocCounts[i] = curAllocCount

        return allocCounts, min(allocCounts.values()), max(allocCounts.values())


    #Fixup memory structure grid dimension on canvas - condense memory to canvas area - display visualization
    def drawHeatmap(self):
        xOff = {}
        yOff = {}
        area = {}

        #Setup dimensions for DRAM overlay grid
        DRAM_START_X = (WINDOW_X-(95*(WINDOW_X/100)))/self.cellwidth
        DRAM_START_Y = (WINDOW_Y-(95*(WINDOW_Y/100)))/self.cellheight
        DRAM_END_X   = (WINDOW_X-(45*(WINDOW_X/100)))/self.cellwidth
        DRAM_END_Y   = (WINDOW_Y-(45*(WINDOW_Y/100)))/self.cellheight
        xOff['DRAM'] = DRAM_START_X
        yOff['DRAM'] = DRAM_START_Y
        DRAM_ROW_LENGTH = DRAM_END_X-DRAM_START_X
        area['DRAM'] = ( (DRAM_END_X-DRAM_START_X)*(DRAM_END_Y-DRAM_START_Y) )

        #Setup dimensions for L2 overlay grid
        L2_START_X = (WINDOW_X-(40*(WINDOW_X/100)))/self.cellwidth
        L2_START_Y = (WINDOW_Y-(58*(WINDOW_Y/100)))/self.cellheight
        L2_END_X   = (WINDOW_X-(5*(WINDOW_X/100)))/self.cellwidth
        L2_END_Y   = (WINDOW_Y-(40*(WINDOW_Y/100)))/self.cellheight
        xOff['L2'] = L2_START_X
        yOff['L2'] = L2_START_Y
        L2_ROW_LENGTH = L2_END_X-L2_START_X
        area['L2'] = ( (L2_END_X-L2_START_X)*(L2_END_Y-L2_START_Y) )

        #Setup dimensions for IPM overlay grid
        IPM_START_X = (WINDOW_X-(40*(WINDOW_X/100)))/self.cellwidth
        IPM_START_Y = (WINDOW_Y-(77*(WINDOW_Y/100)))/self.cellheight
        IPM_END_X   = (WINDOW_X-(5*(WINDOW_X/100)))/self.cellwidth
        IPM_END_Y   = (WINDOW_Y-(59*(WINDOW_Y/100)))/self.cellheight
        xOff['IPM'] = IPM_START_X
        yOff['IPM'] = IPM_START_Y
        IPM_ROW_LENGTH = IPM_END_X-IPM_START_X
        area['IPM'] = ( (IPM_END_X-IPM_START_X)*(IPM_END_Y-IPM_START_Y) )

        #Setup dimensions for NVM overlay grid
        NVM_START_X = (WINDOW_X-(40*(WINDOW_X/100)))/self.cellwidth
        NVM_START_Y = (WINDOW_Y-(96*(WINDOW_Y/100)))/self.cellheight
        NVM_END_X   = (WINDOW_X-(5*(WINDOW_X/100)))/self.cellwidth
        NVM_END_Y   = (WINDOW_Y-(78*(WINDOW_Y/100)))/self.cellheight
        xOff['NVM'] = NVM_START_X
        yOff['NVM'] = NVM_START_Y
        NVM_ROW_LENGTH = NVM_END_X-NVM_START_X
        area['NVM'] = ( (NVM_END_X-NVM_START_X)*(NVM_END_Y-NVM_START_Y) )

        #Setup dimensions for CE L1 overlay grid
        CE_L1_START_X = (WINDOW_X-(40*(WINDOW_X/100)))/self.cellwidth
        CE_L1_START_Y = (WINDOW_Y-(33*(WINDOW_Y/100)))/self.cellheight
        CE_L1_END_X   = (WINDOW_X-(27.5*(WINDOW_X/100)))/self.cellwidth
        CE_L1_END_Y   = (WINDOW_Y-(18*(WINDOW_Y/100)))/self.cellheight
        xOff['CE'] = CE_L1_START_X
        yOff['CE'] = CE_L1_START_Y
        CE_ROW_LENGTH = CE_L1_END_X-CE_L1_START_X
        area['CE'] = ( (CE_L1_END_X-CE_L1_START_X)*(CE_L1_END_Y-CE_L1_START_Y) )

        #Setup dimensions for overlay grid for each XEs L1
        rangeXEsR1   = []
        rangeXEsR2   = []
        XE_rowLength = []
        shift = 0

        for i in range(0, 4):
            xe_s_x = (WINDOW_X-((96.5-(i*12.5)-shift)*(WINDOW_X/100)))/self.cellwidth
            xe_s_y = (WINDOW_Y-(42*(WINDOW_Y/100)))/self.cellheight
            xe_e_x = (WINDOW_X-((96.5-((i+1)*12.5)-shift)*(WINDOW_X/100)))/self.cellwidth
            xe_e_y = (WINDOW_Y-(27*(WINDOW_Y/100)))/self.cellheight
            xOff['XE'+str(i)] = xe_s_x
            yOff['XE'+str(i)] = xe_s_y
            XE_rowLength.append(xe_e_x-xe_s_x)
            area['XE'+str(i)] = ( (xe_e_x - xe_s_x)*(xe_e_y - xe_s_y) )
            rangeXEsR1.append([xe_s_x, xe_s_y, xe_e_x, xe_e_y])
            shift+=1

        shift = 0
        for i in range(0, 4):

            xe_s_x = (WINDOW_X-((96.5-(i*12.5)-shift)*(WINDOW_X/100)))/self.cellwidth
            xe_s_y = (WINDOW_Y-(24*(WINDOW_Y/100)))/self.cellheight
            xe_e_x = (WINDOW_X-((96.5-((i+1)*12.5)-shift)*(WINDOW_X/100)))/self.cellwidth
            xe_e_y = (WINDOW_Y-(9*(WINDOW_Y/100)))/self.cellheight
            XE_rowLength.append(xe_e_x-xe_s_x)
            xOff['XE'+str(i+4)] = xe_s_x
            yOff['XE'+str(i+4)] = xe_s_y
            area['XE'+str(i+4)] = ( (xe_e_x - xe_s_x)*(xe_e_y - xe_s_y) )
            rangeXEsR2.append([xe_s_x, xe_s_y, xe_e_x, xe_e_y])
            shift += 1

        #Flatten all memory structures:
        #    Read the address and size of each allocation made
        #    and convert the hex address to decimal
        CE_offset, CE_watermark, CE_flat   = self.flattenMemory(L1[0])
        L2_offset, L2_watermark, L2_flat   = self.flattenMemory(L2)
        IPM_offset, IPM_watermark, IPM_flat  = self.flattenMemory(IPM)
        NVM_offset, NVM_watermark, NVM_flat  = self.flattenMemory(NVM)
        DRAM_offset, DRAM_watermark, DRAM_flat = self.flattenMemory(DRAM)
        XE_watermarks = []
        XE_offsets = []
        XE_flat = []
        for i in range(1, 9):
            curOffset, curWatermark, curFlat = self.flattenMemory(L1[i])
            XE_watermarks.append(curWatermark)
            XE_offsets.append(curOffset)
            XE_flat.append(curFlat)

        #Condense allocation requests to grid dimensions for each memory structure
        #    Each pixel will represent multiple memory addresses
        print 'Condensing ', CE_L1_SIZE, ' addresses into a ', int(area['CE']), ' pixel grid for CE L1'
        CE_allocs = self.getAllocCounts(L1[0], CE_flat, CE_offset)
        CE_compact, CE_wmpixel = self.consolidateMemory(CE_allocs, CE_watermark, area['CE'], CE_L1_SIZE, False)
        CE_pixels, CE_minV, CE_maxV = self.getMinMaxHeatVals(CE_allocs, CE_wmpixel, CE_compact, False)

        print 'Condensing ', L2_SIZE, ' addresses into a ', int(area['L2']), ' pixel grid for L2'
        L2_allocs = self.getAllocCounts(L2, L2_flat, L2_offset)
        L2_compact, L2_wmpixel = self.consolidateMemory(L2_allocs, L2_watermark, area['L2'], L2_SIZE, True)
        L2_pixels, L2_minV, L2_maxV = self.getMinMaxHeatVals(L2_allocs, L2_wmpixel, L2_compact, False)

        print 'Condensing ', NVM_SIZE, ' addresses into a ', int(area['NVM']), ' pixel grid for NVM'
        NVM_allocs = self.getAllocCounts(NVM, NVM_flat, NVM_offset)
        NVM_compact, NVM_wmpixel = self.consolidateMemory(NVM_allocs, NVM_watermark, area['NVM'], NVM_SIZE, False)
        NVM_pixels, NVM_minV, NVM_maxV = self.getMinMaxHeatVals(NVM_allocs, NVM_wmpixel, NVM_compact, False)

        print 'Condensing ', IPM_SIZE, ' addresses into a ', int(area['IPM']), ' pixel grid for IPM. This will take several minutes if IPM is heavily used in application'
        IPM_allocs = self.getAllocCounts(IPM, IPM_flat, IPM_offset)
        IPM_compact, IPM_wmpixel = self.consolidateMemory(IPM_allocs, IPM_watermark, area['IPM'], IPM_SIZE, False)
        IPM_pixels, IPM_minV, IPM_maxV = self.getMinMaxHeatVals(IPM_allocs, IPM_wmpixel, IPM_compact, False)

        print 'Condensing ', DRAM_SIZE, ' addresses into a ', int(area['DRAM']), ' pixel grid for DRAM. This will take several minutes if DRAM is heavily used in application'
        DRAM_allocs = self.getAllocCounts(DRAM, DRAM_flat, DRAM_offset)
        DRAM_compact, DRAM_wmpixel = self.consolidateMemory(DRAM_allocs, DRAM_watermark, area['DRAM'], DRAM_SIZE, False)
        DRAM_pixels, DRAM_minV, DRAM_maxV = self.getMinMaxHeatVals(DRAM_allocs, DRAM_wmpixel, DRAM_compact, False)


        XE_allocs  = []
        XE_compact = []
        XE_minV    = []
        XE_maxV    = []
        XE_pixels  = []
        XE_watermarkPixels = []
        for i in range(1, 9):
            print 'Condensing ', XE_L1_SIZE, ' addresses into a ', int(area['XE'+str(i-1)]), ' pixel grid for XE', str(i-1), 'L1'
            curAllocCount  = self.getAllocCounts(L1[i], XE_flat[i-1], XE_offsets[i-1])
            curCompact, curWmPixel = self.consolidateMemory(curAllocCount, XE_watermarks[i-1], area['XE'+str(i-1)], XE_L1_SIZE, True)

            if i == 4:
                curPixels, curMin, curMax = self.getMinMaxHeatVals(curAllocCount, curWmPixel, curCompact, True)
            else:
                curPixels, curMin, curMax = self.getMinMaxHeatVals(curAllocCount, curWmPixel, curCompact, False)

            XE_allocs.append(curAllocCount)
            XE_compact.append(curCompact)
            XE_minV.append(curMin)
            XE_maxV.append(curMax)
            XE_pixels.append(curPixels)

        print "\nCalculating heat value colors and launching GUI"
        backgroundColor = '#%02x%02x%02x' % (100, 100, 100)

        #Traverse pixel grid - Check if pixel is within dimensions of a memory structure - Calculate heat color for such pixels
        for i in range(WINDOW_X):
            for j in range(WINDOW_Y):
                xe_found1 = False
                xe_found2 = False
                x1 = i*self.cellwidth
                y1 = j*self.cellheight
                x2 = x1 + self.cellwidth
                y2 = y1 + self.cellheight


                if (i >= DRAM_START_X and i < DRAM_END_X) and (j >= DRAM_START_Y and j < DRAM_END_Y):
                    zeroedY = i-xOff['DRAM']
                    zeroedX = i-yOff['DRAM']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, DRAM_minV, DRAM_maxV, DRAM_ROW_LENGTH, DRAM_allocs, DRAM_compact, DRAM_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                if (i >= L2_START_X and i < L2_END_X) and (j >= L2_START_Y and j < L2_END_Y):
                    zeroedY = i-xOff['L2']
                    zeroedX = j-yOff['L2']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, L2_minV, L2_maxV, L2_ROW_LENGTH, L2_allocs, L2_compact, L2_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                if (i >= IPM_START_X and i < IPM_END_X) and (j >= IPM_START_Y and j < IPM_END_Y):
                    zeroedY = i-xOff['IPM']
                    zeroedX = j-yOff['IPM']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, IPM_minV, IPM_maxV, IPM_ROW_LENGTH, IPM_allocs, IPM_compact, IPM_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                if (i > NVM_START_X and i < NVM_END_X) and (j > NVM_START_Y and j < NVM_END_Y):
                    zeroedY = i-xOff['NVM']
                    zeroedX = j-yOff['NVM']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, NVM_minV, NVM_maxV, NVM_ROW_LENGTH, NVM_allocs, NVM_compact, NVM_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")

                    continue

                if (i >= CE_L1_START_X and i < CE_L1_END_X) and (j >= CE_L1_START_Y and j < CE_L1_END_Y):
                    zeroedY = i-xOff['CE']
                    zeroedX = j-yOff['CE']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, CE_minV, CE_maxV, CE_ROW_LENGTH, CE_allocs, CE_compact, CE_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                for k in range(len(rangeXEsR1)):
                    xe_start_x = rangeXEsR1[k][0]
                    xe_start_y = rangeXEsR1[k][1]
                    xe_end_x = rangeXEsR1[k][2]
                    xe_end_y = rangeXEsR1[k][3]

                    if (i >= xe_start_x and i < xe_end_x) and (j >= xe_start_y and j < xe_end_y):
                        zeroedY = i-xOff['XE'+str(k)]
                        zeroedX = j-yOff['XE'+str(k)]
                        XE_min = XE_minV[k]
                        XE_max = XE_maxV[k]
                        rowLen = XE_rowLength[k]
                        curAllocs = XE_allocs[k]
                        curComp = XE_compact[k]
                        curPixels = XE_pixels[k]

                        xe_found1=True
                        if k == 3:
                            heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, XE_min, XE_max, rowLen, curAllocs, curComp, curPixels, True))
                        else:
                            heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, XE_min, XE_max, rowLen, curAllocs, curComp, curPixels, False))
                        if heatColor == "#ffffff":
                            self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                        else:
                            self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                        break
                if xe_found1:
                    continue

                for k in range(len(rangeXEsR2)):
                    xe_start_x = rangeXEsR2[k][0]
                    xe_start_y = rangeXEsR2[k][1]
                    xe_end_x = rangeXEsR2[k][2]
                    xe_end_y = rangeXEsR2[k][3]

                    if (i >= xe_start_x and i < xe_end_x) and (j >= xe_start_y and j < xe_end_y):
                        zeroedY = i-xOff['XE'+str(k+4)]
                        zeroedX = j-yOff['XE'+str(k+4)]
                        XE_min = XE_minV[k+4]
                        XE_max = XE_maxV[k+4]
                        rowLen = XE_rowLength[k+4]
                        curAllocs = XE_allocs[k+4]
                        curComp = XE_compact[k+4]
                        curPixels = XE_pixels[k+4]

                        xe_found2=True
                        heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, XE_min, XE_max, rowLen, curAllocs, curComp, curPixels, False))
                        if heatColor == "#ffffff":
                            self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                        else:
                            self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                        break
                if xe_found2:
                    continue


                self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=backgroundColor, width=0, tags="rect")


# Use grep (faster for giant log files) to get necessary log data
def runShellStrip(tgLogPath):
    os.system("grep -r " + str(MEM_SIZE_STR) + ' ' + str(tgLogPath) + " > "  + str(SIZE_FILE))
    os.system("grep -r " + str(ALLOC_ID_STR) + ' ' + str(tgLogPath) + " > "  + str(DATA_FILE))
    os.system("grep -r " + str(FREE_ID_STR)  + ' ' + str(tgLogPath) + " >> " + str(DATA_FILE))


# Read log data
def readFile(fname):
    fp = open(fname, 'r')
    lines = fp.readlines()
    fp.close()
    return lines


#Compile TG address translation tool
def buildAddrTranslation(allMemRecs, tgRootPath):
    print "\nBuilding TG address translation tool."
    os.system("gcc -I " + str(tgRootPath) + "common/include/ " + str(tgRootPath) + "utils/xstg-map-decode.c -o decode")
    #os.system("gcc -I " + str(tgRootPath) + "common/include/ " + str(tgRootPath) + "utils/xstg-map-decode.c -o decode")
    print "Translating "+str(len(allMemRecs))+ " TG addresses. May take several minutes...."


# Translate memory addresses reported in logs to TG cononical
# addresses and associated memory structure
def translateAddresses(allMemRecs, tgRootPath, addrDict, queue):
    L1 =   []
    L2 =   []
    L3 =   []
    DRAM = []
    IPM =  []
    NVM =  []
    retList = []
    addrRef = addrDict
    devNull = open(os.devnull, 'w')

    #Launch a shell session
    #FIXME - This may not be portable
    proc = Popen(['/bin/bash'], stdin=PIPE, stdout=PIPE, stderr=devNull)
    matchStrings = ['L1', 'L2', 'L3', 'NVM', 'IPM', 'DRAM']

    #Iterate through this chunk of memory records
    for i in allMemRecs:

        #Check if memory address needs to be decoded
        if addrRef[i[2]] == -1:
            inputStr = ''
            #Launch decode utility
            proc.stdin.write('./decode '+str(i[2])+'\n')

            fail_safe = 0
            while True:
                fail_safe+=1
                line = proc.stdout.readline()
                inputStr+=line
                if "Offset" in line:
                    break
                if fail_safe == 50:
                    break

            memRec = []
            spl = inputStr.split()

            #Associate memory address to appropriate memory structure
            if  ('L1' in spl):
                memRec = spl[spl.index('Agent'):]
                i.append(memRec[3])
                i.append(memRec[6])
                i.append(memRec[2])
                L1.append(i)
                addrRef[i[2]] = i[-3:]
            elif('L2' in spl):
                memRec = spl[spl.index('L2'):]
                i.append(memRec[0])
                i.append(memRec[3])
                L2.append(i)
                addrRef[i[2]] = i[-2:]
            elif('L3' in spl):
                memRec = spl[spl.index('L3'):]
                i.append(memRec[0])
                i.append(memRec[3])
                L3.append(i)
                addrRef[i[2]] = i[-2:]
            elif('DRAM' in spl):
                memRec = spl[spl.index('DRAM'):]
                i.append(memRec[0])
                i.append(memRec[3])
                DRAM.append(i)
                addrRef[i[2]] = i[-2:]
            elif('IPM' in spl):
                memRec = spl[spl.index('IPM'):]
                i.append(memRec[0])
                i.append(memRec[3])
                IPM.append(i)
                addrRef[i[2]] = i[-2:]
            elif('NVM' in spl):
                memRec = spl[spl.index('NVM'):]
                i.append(memRec[0])
                i.append(memRec[3])
                NVM.append(i)
                addrRef[i[2]] = i[-2:]
            else:
                pass
                print "Error: Unable to decode TG address: ", str(i[2]), " - please report this event to nick.m.pepperling@intel.com"
                sys.exit(0)

        #Address already translated; found in cached list
        else:
            cachedRec = addrRef[i[2]]
            for j in cachedRec:
                i.append(j)
            if   "L1"   in cachedRec:
                L1.append(i)
            elif "L2"   in cachedRec:
                L2.append(i)
            elif "L3"   in cachedRec:
                L3.append(i)
            elif "DRAM" in cachedRec:
                DRAM.append(i)
            elif "IPM"  in cachedRec:
                IPM.append(i)
            elif "NVM"  in cachedRec:
                NVM.append(i)
            else:
                print "Error: Unable to decode TG address: ", str(i[2]), " - please report this event to nick.m.pepperling@intel.com"
                sys.exit(0)

    proc.stdin.close()
    proc.wait()

    retList.append(L1)
    retList.append(L2)
    retList.append(L3)
    retList.append(DRAM)
    retList.append(IPM)
    retList.append(NVM)

    #Put in thread join list
    queue.put(retList)


# Read values for memory pool size for each memory structure
def setMemStructureSizes(lines):

    global CE_L1_SIZE
    global XE_L1_SIZE
    global L2_SIZE
    global DRAM_SIZE
    global IPM_SIZE
    global NVM_SIZE

    xeFound = False
    #Iterate poolsize records
    for i in lines:
        if MEM_SIZE_STR in i:
            #Assign poolsize to each memory structure
            sizeIdx = i.split().index(MEM_SIZE_STR)+3
            if "XE" in i:
                if xeFound == False:
                    xeFound = True
                    size = int(i.split()[sizeIdx][:-1], 16)
                    XE_L1_SIZE = size
            else:
                size = int(i.split()[4][:-1], 16)
                lvl = i.split()[6][:-1]
                if   lvl == '1': #L1
                    CE_L1_SIZE = size
                elif lvl == '2': #L2
                    L2_SIZE = size
                elif lvl == '3': #L3 (Not implemented in FSIM)
                    pass
                elif lvl == '4': #IPM
                    IPM_SIZE = size
                elif lvl == '5': #DRAM
                    NVM_SIZE = size


# Sort log records by start timestamp - Not necessary but will be if phased support implemented
def sortAscendingStartTime(logFile):
    stripped = []
    for i in logFile:
        if MEM_SIZE_STR in i:
           continue
        if '>>>' in i:
            stripped.append(i.split(' ', 3)[3])
        else:
            stripped.append(i.split(' ', 1)[1])
    logCopy = stripped
    logCopy.sort(key = lambda x: int(x.split()[TIME_INDEX]))
    return logCopy


# Initialize { Address : mem_structure } dictionary
def buildAddressDict(log):
    addrDict = {}
    for i in log:
        if ALLOC_ID_STR in i:
            addrDict[i.split()[ALLOC_ADDR_INDEX]] = -1

    return addrDict


# Sanitize raw log input - place alloc records into a data structure
def createAllocList(log, timeOffset, addrDict):
    data = []
    alloced = []
    freed = []
    #Iterate through memory records
    for line in log:
        timestamp = line.split()[TIME_INDEX]
        #Hack to mack sure no erroneous I/O strings
        if len(line.split()) != ALLOC_STRLEN:
            continue
        #Free log record
        if FREE_ID_STR in line:
            action = 'Freeing'
            #Get Address
            addr = line.split()[FREE_ADDR_INDEX]
            addrInt = (int(line.split()[FREE_ADDR_INDEX], 0))

            if addrDict[addr] == -1:
                #Should not ever happen
                print "log indicated a free @ address where no memory is allocated - please report this event to nick.m.pepperling@intel.com"
                continue

            freed.append(addrDict[addr])
            #Reset the memory address
            tempSize = addrDict[addr]
            addrDict[addr] = -1

        #Alloc log record
        else:
            action = 'Allocate'
            #Get Address
            addr = line.split()[ALLOC_ADDR_INDEX]
            addrInt = (int(line.split()[ALLOC_ADDR_INDEX], 0))
            tempSize = int(line.split()[ALLOC_SIZE_INDEX])
            alloced.append(tempSize)

            #TODO Need to account for realloc?
            if addrDict[addr] != -1:
                #realloc() found
                pass

            #Set the memory address to alloced size for corresponding free reference.
            addrDict[addr] = tempSize

        #Add record
        numBytes = tempSize
        time = int(line.split()[TIME_INDEX]) - timeOffset
        data.append([action, numBytes, addr, time])

    return data

def getTotalBytes(mem):
    numBytes = 0
    for i in mem:
        numBytes += int(i[1])
    return numBytes


#Create a file summarizing memory usage in each strucutre
def writeSummaryFile():

    fp = open('summary.txt', 'w')

    fp.write('========== DRAM ==========\n')
    if len(DRAM) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        fp.write('\tPool Size: '+str("%.2f" % DRAM_SIZE/float(1000000))+'\n')
        fp.write('\tTotal Allocation Requests: '+str(len(DRAM))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(DRAM))+'\n\n')

    fp.write('========== NVM ==========\n')
    if len(NVM) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        fp.write('\tPool Size: '+str("%.2f" % NVM_SIZE/float(1000000))+'MB\n')
        fp.write('\tTotal Allocation Requests: '+str(len(NVM))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(NVM))+'\n\n')

    fp.write('========== IPM ==========\n')
    if len(IPM) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        fp.write('\tPool Size: '+str("%.2f" % (IPM_SIZE/float(1000000)))+'MB\n')
        fp.write('\tTotal Allocation Requests: '+str(len(IPM))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(IPM))+'\n\n')

    fp.write('========== L2 ==========\n')
    if len(L2) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        fp.write('\tPool Size: '+str("%.2f" % (L2_SIZE/float(1000000)))+'MB\n')
        fp.write('\tTotal Allocation Requests: '+str(len(L2))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(L2))+'\n\n')

    for i in range(len(L1)):
        curL1 = L1[i]
        if i == 0: #CE
            fp.write('========== CE - L1 ==========\n')
            if len(curL1) == 0:
                fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
            else:
                fp.write('\tPool Size: '+str("%.2f" % (CE_L1_SIZE/float(1000)))+'KB\n')
                fp.write('\tTotal Allocation Requests: '+str(len(curL1))+'\n')
                fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(curL1))+'\n\n')

        else: #XE
            fp.write('========== XE'+str(i-1)+' - L1 ==========\n')
            if len(curL1) == 0:
                fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
            else:
                fp.write('\tPool Size: '+str("%.2f" % (XE_L1_SIZE/float(1000)))+'KB\n')
                fp.write('\tTotal Allocation Requests: '+str(len(curL1))+'\n')
                fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(curL1))+'\n\n')

    fp.close()


# Cleanup files generated by script
def cleanup():
    os.remove(DATA_FILE)
    os.remove(SIZE_FILE)
    os.remove(os.getcwd()+'/decode')

def main():
    try:
        tgRootPath = os.environ['TG_ROOT']
    except:
        print "Error: TG_ROOT must be appropriatelly set \n"
        sys.exit(0)

    if len(sys.argv) != 2:
        print "Error.....  Usage: python timeline.py  <inputFile>"
        sys.exit(0)

    tgLogPath = sys.argv[1]
    if tgLogPath[-1] != '/':
        tgLogPath+='/'
    if tgRootPath[-1] != '/':
        tgRootPath+='/'

    #Read relevant log data
    print "Reading log files; stripping allocation records..."
    runShellStrip(tgLogPath)
    lines = readFile(DATA_FILE)
    sizes = readFile(SIZE_FILE)

    #Setup pool size for memory structures and sort
    setMemStructureSizes(sizes)
    srted = sortAscendingStartTime(lines)
    timeOffset = int(srted[0].split()[TIME_INDEX])

    #Initialize address dictionaries
    addrDict = buildAddressDict(srted)
    addrDictCopy = copy.deepcopy(addrDict)

    #Parse alloc and free entries
    allMemRecs = createAllocList(srted, timeOffset, addrDict)

    #Fire off a subprocess to compile the fsim address decode tool
    buildAddrTranslation(allMemRecs, tgRootPath)

    #Get thread count and divide memory entries into equal sublists
    cpuCount = multiprocessing.cpu_count()
    divCount = int(math.ceil(len(allMemRecs)/(cpuCount-1)))
    splList = [allMemRecs[i:i + divCount] for i in range(0, len(allMemRecs), divCount)]
    queue = Queue.Queue()
    threads = []
    #Parallelize the address translation
    for i in range(len(splList)):
        t = threading.Thread(target=translateAddresses, args=[splList[i], tgRootPath, addrDictCopy, queue],)
        threads.append(t)
        t.start()

    #Join threads and combine address into single data structure
    joinList = []
    for i in range(len(splList)):
        t = threads[i]
        ret = queue.get()
        joinList.append(ret)
        t.join
    print "\nDONE! ...",

    global L1
    global L2
    global L3
    global DRAM
    global IPM
    global NVM

    #Initialize L1 list -- [0]: CE's L1  1 [1-8]: XE0 - XE7 L1
    L1 = [[] for j in range(9)]

    #Place memory records into their global memory structures
    for i in joinList:
        for j in i[0]:
            xe_id = int(j[6], 16)
            L1[xe_id].append(j)
        for j in i[1]:
            L2.append(j)
        for j in i[2]:
            L3.append(j)
        for j in i[3]:
            DRAM.append(j)
        for j in i[4]:
            IPM.append(j)
        for j in i[5]:
            NVM.append(j)

    writeSummaryFile()
    cleanup()
    print "Initializing visualization library objects\n"
    #Launch Visualization
    root = tk.Tk()
    app = memMap(root)
    root.mainloop()

main()
