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
DDRMEM   = []
MCDRAM  = []

ALLOC_STRLEN = 6

#Default pool size values - will be overwritten if configured in logged FSIM app
#TODO: Shouldn't be hard coded - shortcut for quick one-off expiriment for XS report
MCDRAM_SIZE = 2065694720
DDRMEM_SIZE = 4065694720

MCD_START_ADDR = 0
DDR_START_ADDR = 0

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
        flatList = [0 for i in range(total)]
        return min(dec), total, flatList



    #Populate {address : memory access count} dictionary
    def getAllocCounts(self, mem, addrMap, offset):
        localCopy = addrMap

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

        #Setup dimensions for MCDRAM overlay grid
        MCD_START_X = (WINDOW_X-(95*(WINDOW_X/100)))/self.cellwidth
        MCD_START_Y = (WINDOW_Y-(35*(WINDOW_Y/100)))/self.cellheight
        MCD_END_X   = (WINDOW_X-(5*(WINDOW_X/100)))/self.cellwidth
        MCD_END_Y   = (WINDOW_Y-(5*(WINDOW_Y/100)))/self.cellheight
        xOff['MCD'] = MCD_START_X
        yOff['MCD'] = MCD_START_Y
        MCD_ROW_LENGTH = MCD_END_X-MCD_START_X
        area['MCD'] = ( (MCD_END_X-MCD_START_X)*(MCD_END_Y-MCD_START_Y) )

        #Setup dimensions for High Bandwidth Memory overlay grid
        DDRM_START_X = (WINDOW_X-(95*(WINDOW_X/100)))/self.cellwidth
        DDRM_START_Y = (WINDOW_Y-(95*(WINDOW_Y/100)))/self.cellheight
        DDRM_END_X   = (WINDOW_X-(5*(WINDOW_X/100)))/self.cellwidth
        DDRM_END_Y   = (WINDOW_Y-(40*(WINDOW_Y/100)))/self.cellheight
        xOff['DDRM'] = DDRM_START_X
        yOff['DDRM'] = DDRM_START_Y
        DDRM_ROW_LENGTH = DDRM_END_X-DDRM_START_X
        area['DDRM'] = ( (DDRM_END_X-DDRM_START_X)*(DDRM_END_Y-DDRM_START_Y) )


        MCD_offset, MCD_watermark, MCD_flat = self.flattenMemory(MCDRAM)
        print 'Condensing ', MCDRAM_SIZE, ' addresses into a ', int(area['MCD']), ' pixel grid for MCDRAM. This will take several minutes if DRAM is heavily used in application'
        MCD_allocs = self.getAllocCounts(MCDRAM, MCD_flat, MCD_offset)
        MCD_compact, MCD_wmpixel = self.consolidateMemory(MCD_allocs, MCD_watermark, area['MCD'], MCDRAM_SIZE, False)
        MCD_pixels, MCD_minV, MCD_maxV = self.getMinMaxHeatVals(MCD_allocs, MCD_wmpixel, MCD_compact, False)


        DDRM_offset, DDRM_watermark, DDRM_flat = self.flattenMemory(DDRMEM)
        print 'Condensing ', DDRMEM_SIZE, ' addresses into a ', int(area['DDRM']), ' pixel grid for DDR. This will take several minutes if DRAM is heavily used in application'
        DDRM_allocs = self.getAllocCounts(DDRMEM, DDRM_flat, DDRM_offset)
        DDRM_compact, DDRM_wmpixel = self.consolidateMemory(DDRM_allocs, DDRM_watermark, area['DDRM'], DDRMEM_SIZE, False)
        DDRM_pixels, DDRM_minV, DDRM_maxV = self.getMinMaxHeatVals(DDRM_allocs, DDRM_wmpixel, DDRM_compact, False)


        print "\nCalculating heat value colors and launching GUI"
        backgroundColor = '#%02x%02x%02x' % (100, 100, 100)

        #Traverse pixel grid - Check if pixel is within dimensions of a memory structure - Calculate heat color for such pixels
        for i in range(WINDOW_X):
            for j in range(WINDOW_Y):
                x1 = i*self.cellwidth
                y1 = j*self.cellheight
                x2 = x1 + self.cellwidth
                y2 = y1 + self.cellheight


                if (i >= MCD_START_X and i < MCD_END_X) and (j >= MCD_START_Y and j < MCD_END_Y):
                    zeroedY = i-xOff['MCD']
                    zeroedX = j-yOff['MCD']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, MCD_minV, MCD_maxV, MCD_ROW_LENGTH, MCD_allocs, MCD_compact, MCD_pixels, True))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=backgroundColor, width=0, tags="rect")


                if (i >= DDRM_START_X and i < DDRM_END_X) and (j >= DDRM_START_Y and j < DDRM_END_Y):
                    zeroedY = i-xOff['DDRM']
                    zeroedX = j-yOff['DDRM']
                    heatColor = '#%02x%02x%02x' % (self.getHeatColor(zeroedX, zeroedY, DDRM_minV, DDRM_maxV, DDRM_ROW_LENGTH, DDRM_allocs, DDRM_compact, DDRM_pixels, False))
                    if heatColor == "#ffffff":
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=1, tags="rect")
                    else:
                        self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=heatColor, width=0, tags="rect")
                    continue

                self.rect[i,j] = self.canvas.create_rectangle(x1,y1,x2,y2, fill=backgroundColor, width=0, tags="rect")




# Use grep (faster for giant log files) to get necessary log data
def runShellStrip(tgLogPath):
    os.system("grep " + str(MEM_SIZE_STR) + ' ' + str(tgLogPath) + " > "  + str(SIZE_FILE))
    os.system("grep " + str(ALLOC_ID_STR) + ' ' + str(tgLogPath) + " > "  + str(DATA_FILE))
    os.system("grep " + str(FREE_ID_STR)  + ' ' + str(tgLogPath) + " >> " + str(DATA_FILE))


# Read log data
def readFile(fname):
    fp = open(fname, 'r')
    lines = fp.readlines()
    fp.close()
    return lines

def hexToDec(hexVal):
    if hexVal == 0 :
        return 0
    return int(hexVal, 16)

def decToHex(decVal):
    if devVal == 0:
        return 0x0
    else:
        return '0x'+hex(decVal).split('x')[-1]

# Read values for memory pool size for each memory structure
def setStartAddresses(lines):
    #TODO - This is dependent on hardcoded size values - shortcut for XS report
    global MCD_START_ADDR
    global DDR_START_ADDR

    for i in lines:
        line = i.split()
        if "WARN" in line:
            continue
        if str(MCDRAM_SIZE) in line:
            MCD_START_ADDR = line[-3]
        elif str(DDRMEM_SIZE) in line:
            DDR_START_ADDR = line[-3]



# Sort log records by start timestamp - Not necessary but will be if phased support implemented
def sortAscendingStartTime(logFile):
    stripped = []
    for i in logFile:
        if MEM_SIZE_STR in i:
           continue
        else:
            stripped.append(i)
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

def translateAddresses(memRecs):

    global DDRMEM
    global MCDRAM
    ddrStart = hexToDec(DDR_START_ADDR)
    mcdStart = hexToDec(MCD_START_ADDR)

    for i in memRecs:
        curAddr = hexToDec(i[2])
        if   curAddr >= ddrStart and curAddr <= (ddrStart+DDRMEM_SIZE):
            DDRMEM.append(i)
        elif curAddr >= mcdStart and curAddr <= (mcdStart+MCDRAM_SIZE):
            MCDRAM.append(i)
        else:
            print "Could not map memory address", i[2], 'to DDR or MCDRAM address pool. skip'

def truncatePoolSizes():
    global DDRMEM_SIZE
    global MCDRAM_SIZE
    minDDR = []
    minMCD = []
    maxAddrDDR = '0x0'
    maxAddrMCD = '0x0'

    maxAllocDDR = 0
    maxAllocMCD = 0

    for i in DDRMEM:
        if int(i[2], 16) > int(maxAddrDDR, 16):
            minDDR.append(int(i[2], 16))
            maxAddrDDR = i[2]
    for i in MCDRAM:
        if int(i[2], 16) > int(maxAddrMCD, 16):
            minMCD.append(int(i[2], 16))
            maxAddrMCD = i[2]

    for i in MCDRAM:
        if i[2] == maxAddrMCD:
            if i[1] > maxAllocMCD:
                maxAllocMCD = i[1]
    for i in DDRMEM:
        if i[2] == maxAddrDDR:
            if i[1] > maxAllocDDR:
                maxAllocDDR = i[1]
    try:
        DDRMEM_SIZE = int(maxAddrDDR, 16) + maxAllocDDR - min(minDDR)
    except:
        pass

    try:
        MCDRAM_SIZE = int(maxAddrMCD, 16) + maxAllocMCD - min(minMCD)
    except:
        pass


def getTotalBytes(mem):
    numBytes = 0
    for i in mem:
        numBytes += int(i[1])
    return numBytes

def writeSummaryFile():

    fp = open('summary.txt', 'w')

    fp.write('========== DDR ==========\n')
    if len(DDRMEM) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        mySize = DDRMEM_SIZE/float(1000000)
        fp.write('\tPool Size: '+str("{0:.2f}".format(mySize))+'\n')
        fp.write('\tTotal Allocation Requests: '+str(len(DDRMEM))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(DDRMEM))+'\n\n')

    fp.write('========== MCDRAM ==========\n')
    if len(MCDRAM) == 0:
        fp.write('\tMEMORY STRUCTURE UNUSED \n\n')
    else:
        mySize2 = MCDRAM_SIZE/float(1000000)
        fp.write('\tPool Size: '+str("{0:.2f}".format(mySize2))+'MB\n')
        fp.write('\tTotal Allocation Requests: '+str(len(MCDRAM))+'\n')
        fp.write('\tTotal Bytes Allocated: '+str(getTotalBytes(MCDRAM))+'\n\n')

    fp.close()



# Cleanup files generated by script
def cleanup():
    os.remove(DATA_FILE)
    os.remove(SIZE_FILE)

def main():

    if len(sys.argv) != 2:
        print "Error.....  Usage: python timeline.py  <inputFile>"
        sys.exit(0)

    fname = sys.argv[1]

    #Read relevant log data
    print "Reading log files; stripping allocation records..."
    runShellStrip(fname)
    lines = readFile(DATA_FILE)
    sizes = readFile(SIZE_FILE)

    setStartAddresses(sizes)

    srted = sortAscendingStartTime(lines)

    timeOffset = int(srted[0].split()[TIME_INDEX])

    #Initialize address dictionaries
    addrDict = buildAddressDict(srted)
    addrDictCopy = copy.deepcopy(addrDict)

    #Parse alloc and free entries
    allMemRecs = createAllocList(srted, timeOffset, addrDict)

    translateAddresses(allMemRecs)

    truncatePoolSizes()

    cleanup()
    print "Initializing visualization library objects\n"
    #Launch Visualization
    writeSummaryFile()

    root = tk.Tk()
    app = memMap(root)
    root.mainloop()

main()
