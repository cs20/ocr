#!/usr/bin/env python

import sys
import os
import time
import itertools

from operator import itemgetter
from pprint import pprint

#Indices for parsing log records
NUM_TIMESTAMP_POINTS = 9
PD_ID_INDEX = 5
WRKR_ID_INDEX      =  8
TIMESTAMP_INDEX   = 11
TRACE_TYPE_INDEX   = 14
TRACE_ACTION_INDEX = 17
DEQ_ADDR_INDEX     = 20
EDT_GUID_INDEX  = 20
GUID_TAKEN_INDEX = 20
DEQUE_TAKEN_INDEX = 23
SCHED_DEQUE_INDEX = 23
RUNNABLE_GUID_INDEX = 20
MSG_SEND_GUID_INDEX = 20
MSG_RCV_GUID_INDEX  = 20
SCHED_INVOKE_GUID_INDEX  = 20
SCHED_HEUR_INVOKE_GUID_INDEX  = 20
EXE_GUID_INDEX = 20
WRKR_SPIN_START_TIME_INDEX = 20
WRKR_SPIN_END_TIME_INDEX = 23
FUNC_PTR_INDEX = 20

#globals for tracking scheduling overheads
avail_count = []
work_req_count = []
total_exe_time = 0
first_exe_time = 0
last_exe_time = 0

workSeekingMap = [[]]
workWriteList = [[[]]]
uniqPds = []
uniqWrkrs = []
uniqDeqs = []

class WORKER:

    def __init__(self, wrkrId, isSeeking, isStealing, dequeHasWork, spinTime, pdId, edts):
        self.wrkrId       = wrkrId
        self.isSeeking    = isSeeking
        self.isStealing   = isStealing
        self.dequeHasWork = dequeHasWork
        self.spinTime     = spinTime
        self.pdId         = pdId
        self.edts         = edts

class DEQUE:

    def __init__(self, pdId, addrStr, hasWork, owner, workCount):
        self.pdId      = pdId
        self.addrStr   = addrStr
        self.hasWork   = hasWork
        self.owner     = owner
        self.workCount = workCount

class EDT:

    def __init__(self, guid, pdId, createTime, runnableTime, schedSendTime, schedRcvTime, schedInvokeTime, schedHeurTime, scheduledTime, pickupTime, exeTime, funcPtr):
        self.guid = guid
        self.pdId = pdId
        self.createTime      = createTime
        self.runnableTime    = runnableTime
        self.schedSendTime   = schedSendTime
        self.schedRcvTime    = schedRcvTime
        self.schedInvokeTime = schedInvokeTime
        self.schedHeurTime   = schedHeurTime
        self.scheduledTime   = scheduledTime
        self.pickupTime      = pickupTime
        self.exeTime         = exeTime
        self.funcPtr          = funcPtr


def initGlobalData(uniqPds, uniqWrkrs):

    global workSeekingMap
    global avail_count
    global work_req_count
    global workWriteList

    workSeekingMap = [[None for i in range(len(uniqWrkrs))]for j in range(len(uniqPds))]
    workWriteList  = [[[]*0 for i in range(len(uniqWrkrs))]for j in range(len(uniqPds))]
    avail_count    = [0 for i in range(len(uniqPds))]
    work_req_count = [0 for i in range(len(uniqPds))]

    for i in range(len(uniqPds)):
        for j in range(len(uniqWrkrs)):
            workSeekingMap[i][j] = False


def setTotalExeTime(logFile):
    global total_exe_time
    global first_exe_time
    global last_exe_time

    exes = []

    for i in logFile:
        if i.split()[TRACE_ACTION_INDEX] == "EXECUTE":
            exes.append(i)

    #Store total/first/last times as globals
    first_exe_time = int(exes[0].split()[TIMESTAMP_INDEX])
    last_exe_time  = int(exes[-1].split()[TIMESTAMP_INDEX])
    total_exe_time =  last_exe_time - first_exe_time


def sortAscendingTime(logFile):
    logCopy = logFile
    logCopy.sort(key = lambda x: x.split()[TIMESTAMP_INDEX])
    return logCopy


def sortUniqPds(pds):
    sortedPds = [None]*len(pds)
    intVals = []

    #Sort PDs by id for uniform indexing
    for i in pds:
        intVal = int(i, 0)
        intVals.append(intVal)
        sortedPds[intVal] = i

    return sortedPds


def postProcessLog(log,):
    global first_exe_time
    global avail_count
    global work_req_count
    global workWriteList

    uniqPds   = []
    uniqWrkrs = []

    edtObjs    = []
    edtObjsIdxMap = []

    #Initialize EDT object per EDT creation
    for line in log:
        uniqPds.append(line.split()[PD_ID_INDEX])
        uniqWrkrs.append(line.split()[WRKR_ID_INDEX])
        if(line.split()[TRACE_TYPE_INDEX] == "EDT") and (line.split()[TRACE_ACTION_INDEX] == "CREATE"):
            guid = line.split()[EDT_GUID_INDEX]
            pd   = line.split()[PD_ID_INDEX]
            time = int(line.split()[TIMESTAMP_INDEX])
            edtObjs.append(EDT(guid, pd, time, 0, 0, 0, 0, 0, 0, 0, 0, 0))
            edtObjsIdxMap.append(guid)

    #Store in dictionary {key = PD id : value = list of EDT objects}
    dictMap = dict(itertools.izip(edtObjsIdxMap, edtObjs))


    uniqPds = set(uniqPds)
    uniqPds = sortUniqPds(uniqPds)
    uniqWrkrs = sorted(set(uniqWrkrs))

    #initialize global lists for unique, sorted PDs and workers for uniform indexing
    initGlobalData(uniqPds, uniqWrkrs)

    #initialize list to contain worker objects
    workerObjs = [[None for i in range(len(uniqWrkrs))]for j in range(len(uniqPds))]

    for i in xrange(len(uniqPds)):
        for j in xrange(len(uniqWrkrs)):
            workerObjs[i][j] = WORKER(j, False, False, False, 0, i, [])

    #Arrange data respective to trace type/action
    for line in log:

        curRec = line.split()

        if curRec[TRACE_TYPE_INDEX] == "EDT":

            if curRec[TRACE_ACTION_INDEX] == "CREATE":
                pass

            elif curRec[TRACE_ACTION_INDEX] == "RUNNABLE":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[RUNNABLE_GUID_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.runnableTime = time


            elif curRec[TRACE_ACTION_INDEX] == "SCHEDULED":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[EDT_GUID_INDEX]
                deq   = curRec[SCHED_DEQUE_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.scheduledTime = time
                #One more EDT available
                avail_count[pdIdx] += 1

            elif curRec[TRACE_ACTION_INDEX] == "EXECUTE":
                pdIdx   = int(curRec[PD_ID_INDEX], 0)
                wIdx    = int(curRec[WRKR_ID_INDEX])
                guid    = curRec[EXE_GUID_INDEX]
                time    = int(curRec[TIMESTAMP_INDEX])
                funcPtr = curRec[FUNC_PTR_INDEX]
                edtObj = dictMap[guid]
                edtObj.exeTime = time
                edtObj.funcPtr = funcPtr
                wrkrObj = workerObjs[pdIdx][wIdx]
                wrkrObj.edts.append(edtObj)

            else:
                continue

        elif curRec[TRACE_TYPE_INDEX] == "SCHEDULER":

            if curRec[TRACE_ACTION_INDEX] == "MSG_SEND":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[MSG_SEND_GUID_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.schedSendTime = time

            elif curRec[TRACE_ACTION_INDEX] == "MSG_RECEIVE":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[MSG_RCV_GUID_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.schedRcvTime = time

            elif curRec[TRACE_ACTION_INDEX] == "INVOKE":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[SCHED_INVOKE_GUID_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.schedInvokeTime = time

            elif curRec[TRACE_ACTION_INDEX] == "HEURISTIC_INVOKE":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[SCHED_HEUR_INVOKE_GUID_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.schedHeurTime = time

            else:
                continue

        elif curRec[TRACE_TYPE_INDEX] == "WORKER":

            if curRec[TRACE_ACTION_INDEX] == "SPIN":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                time  = int(curRec[TIMESTAMP_INDEX])
                workerObj = workerObjs[pdIdx][wIdx]
                startTime = int(curRec[WRKR_SPIN_START_TIME_INDEX])
                endTime = int(curRec[WRKR_SPIN_END_TIME_INDEX])

                #Ignore spin time if timestamp was before first EDT or after last EDT
                if( (startTime > first_exe_time) and (startTime < last_exe_time) ):
                    spin = endTime - startTime
                    workerObj.spinTime += spin

            elif curRec[TRACE_ACTION_INDEX] == "TAKEN":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                guid  = curRec[GUID_TAKEN_INDEX]
                deq   = curRec[DEQUE_TAKEN_INDEX]
                time  = int(curRec[TIMESTAMP_INDEX])
                edtObj = dictMap[guid]
                edtObj.pickupTime = time

                workWriteList[pdIdx][wIdx].append(str(pdIdx)+','+str(wIdx)+',TAKEN,'+str(time)+'\n')
                workSeekingMap[pdIdx][wIdx] = False
                #One less worker requesting, one less EDT available
                work_req_count[pdIdx] -= 1
                avail_count[pdIdx] -=1

            elif curRec[TRACE_ACTION_INDEX] == "REQUEST":
                pdIdx = int(curRec[PD_ID_INDEX], 0)
                wIdx  = int(curRec[WRKR_ID_INDEX])
                time  = int(curRec[TIMESTAMP_INDEX])

                workWriteList[pdIdx][wIdx].append(str(pdIdx)+','+str(wIdx)+',REQUEST,'+str(time)+','+str(avail_count[pdIdx])+'\n')
                workSeekingMap[pdIdx][wIdx] = True
                #One more worker requesting
                work_req_count[pdIdx]+=1

            else:
                continue

        else:
            continue


    itemsToReturn = []

    itemsToReturn.append(uniqPds)
    itemsToReturn.append(uniqWrkrs)
    itemsToReturn.append(edtObjs)
    itemsToReturn.append(edtObjsIdxMap)
    itemsToReturn.append(workerObjs)

    return itemsToReturn


def writeEdtData(uniqPds, uniqWorkers, workerObjs):

    #Create one file per PD
    for i in range(len(uniqPds)):

        excelData = open('timeData_pd'+str(i)+'.csv', 'w')
        excelData.write('sep=,\n')

        #Write each EDTs time intervals to file
        for j in range(len(uniqWorkers)):
            workerObj = workerObjs[i][j]
            for edt in workerObj.edts:
                csvString = ""

                csvString += str(edt.guid) + ','
                csvString += str(edt.funcPtr) + ','
                csvString += str((edt.runnableTime - edt.createTime)) + ','
                csvString += str((edt.schedSendTime - edt.runnableTime)) + ','
                csvString += str((edt.schedRcvTime - edt.schedSendTime)) + ','
                csvString += str((edt.schedInvokeTime - edt.schedRcvTime)) + ','
                csvString += str((edt.scheduledTime - edt.schedInvokeTime)) + ','
                csvString += str((edt.pickupTime - edt.scheduledTime)) + ','
                csvString += str((edt.exeTime - edt.pickupTime)) + '\n'
                excelData.write(csvString)

        excelData.close()


def writeWorkerData(uniqPds, uniqWrkrs):
    global workWriteList
    workActivityFiles = {}

    #Create one file per PD
    for i in xrange(len(uniqPds)):
        workFile = open('workActivityLog_pd'+str(i)+'.csv', 'w')
        workFile.write('sep=,\n')
        workFile.write("PD Id,Wrkr Id,Action,Time,EDTs Avail\n")
        workActivityFiles[i] = workFile

    #Write work request/taken times to file
    for i in range (len(uniqPds)):
        for j in range (len(uniqWrkrs)):
            for k in workWriteList[i][j]:
                workActivityFiles[i].write(k)

    #close files
    for f in range(len(workActivityFiles)):
        workActivityFiles[f].close()


def usage():
    print "Incorrect Usage..."
    print "To run: ./analyzeSchedOverhead.py <trace_output_file>"


###########################################################
###########################################################

def main():
    global total_exe_time

    if len(sys.argv) < 2:
        usage()

    #Open and read trace file
    logFP = open(sys.argv[1], 'r')
    log = logFP.readlines()
    if(len(log) == 0):
        sys.exit("No scheduler TRACE events found in log")

    #Sort by timestamp
    sortedRet = sortAscendingTime(log)
    print "log sorted"

    #Setup global start/end/total times
    setTotalExeTime(sortedRet)

    #Post process trace log and arrange for statistics output
    sanatizedData = postProcessLog(sortedRet)

    uniqPds       = sanatizedData[0]
    uniqWorkers   = sanatizedData[1]
    edtObjs       = sanatizedData[2]
    edtObjsIdxMap = sanatizedData[3]
    workerObjs    = sanatizedData[4]
    print "Data Sanatized"

    #Write EDT statistics
    writeEdtData(uniqPds, uniqWorkers, workerObjs)
    #Write work seek/taken statistics
    writeWorkerData(uniqPds, uniqWorkers)
    print "csv files written"

    sys.exit(0)

if __name__ == "__main__":
    main()
