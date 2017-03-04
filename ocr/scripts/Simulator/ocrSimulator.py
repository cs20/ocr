#!/usr/bin/env python

#import warnings
#warnings.filterwarnings('error')
import sys
import os
import copy
import itertools
import math
import time
#import numpy as np

import numpy.polynomial.polynomial as poly
import numpy as np

import scipy.optimize as optimize
from scipy.odr import odrpack as odr
from scipy.odr import models

from operator import itemgetter

#Globals to be read in from config file
inputFiles = []
appParams = []
pathToBinary = ""

#Dictionary mapping template guids to the func ptr they associate with
templateFpRefs = {}

#=========================================================================================#
# Below we define a host of global constants and lookup tables to simplify postprocessing #
#=========================================================================================#

#Tolerance value to determine whether or not to equalize API call counts accross input sizes
TOLERANCE = 3

#Common indices for parsing
PD_ID        =  5
WRKR_ID      =  8
EDT_GUID     = 11
TIMESTAMP    = 14
TTYPE        = 17
TACTION      = 20

#EDT Finish indices (perf counters)
FIN_GUID         = 23
FIN_EDT_FP       = 26
FIN_COUNT        = 29
FIN_CYCLES       = 32
FIN_CACHE_REFS   = 35
FIN_CACHE_MISSES = 38
FIN_FP_OPS       = 41
FIN_EDT_CREATES  = 43
FIN_DB_TOTAL     = 47
FIN_DB_CREATES   = 50
FIN_DB_DESTROYS  = 53
FIN_EVT_SATS     = 56

#HW Counter Indices for output
OUT_CYCLES = 0
OUT_CACHE_REFS = 1
OUT_CACHE_MISSES = 2
OUT_FP_OPS = 3

#EDT execution record indices
EXE_GUID = 23
EXE_FP = 26
EXE_PARAMC = 32
EXE_PARAMV = 36

#Template create record indices
TEMPLATE_GUID = 23
TEMPLATE_FP = 26

#Helpful indexing constants
EDT_CREATES = 0
TEMPLATE_CREATES = 1
EVENT_CREATES = 2
EVENT_SATS = 3
EVENT_DESTROYS = 4
ADD_DEPS = 5
DB_CREATES = 6
DB_RELEASES = 7
DB_DESTROYS = 8
AFF_GET_CUR = 9
AFF_GET_AT = 10
AFF_COUNT = 11
AFF_QUERY = 12
HINT_INITS = 13
HINT_SET_VALS = 14
GUID_RANGE_CREATES = 15

#Lookup table for resolving syntactically correct ocr calls from trace records
apiNameLookupTable =    {"API_EDTCREATE" : "ocrEdtCreate",
                         "API_EDTTEMPLATE_CREATE" : "ocrEdtTemplateCreate",
                         "EDTEXECUTE" : "EXECUTE",
                         "EDTFINISH" : "FINISH",
                         "API_EVENTCREATE" : "ocrEventCreate",
                         "API_EVENTSATISFY" : "ocrEventSatisfy",
                         "API_EVENTADD_DEP" : "ocrAddDependence",
                         "API_EVENTDESTROY" : "ocrEventDestroy",
                         "API_DATABLOCKCREATE" : "ocrDbCreate",
                         "API_DATABLOCKDATA_RELEASE" : "ocrDbRelease",
                         "API_DATABLOCKDESTROY" : "ocrDbDestroy",
                         "API_AFFINITYGET_CURRENT" : "ocrAffinityGetCurrent",
                         "API_AFFINITYGET_AT" : "ocrAffinityGetAt",
                         "API_AFFINITYGET_COUNT" : "ocrAffinityCount",
                         "API_AFFINITYQUERY" : "ocrAffinityQuery",
                         "API_HINTINIT" : "ocrHintInit",
                         "API_HINTSET_VAL" : "ocrSetHintValue",
                         "API_GUIDRANGE_CREATE" : "ocrGuidRangeCreate"}

# Lookup table mapping api calls to list indices
# (defined in above constants) for data structures created
nameDict = {
    "ocrEdtCreate" : EDT_CREATES,
    "ocrEdtTemplateCreate" : TEMPLATE_CREATES,
    "ocrEventCreate" : EVENT_CREATES,
    "ocrEventSatisfy" : EVENT_SATS,
    "ocrEventDestroy" : EVENT_DESTROYS,
    "ocrAddDependence" : ADD_DEPS,
    "ocrDbCreate" : DB_CREATES,
    "ocrDbRelease" : DB_RELEASES,
    "ocrDbDestroy" : DB_DESTROYS,
    "ocrAffinityGetCurrent" : AFF_GET_CUR,
    "ocrAffinityGetAt" : AFF_GET_AT,
    "ocrAffinityCount" : AFF_COUNT,
    "ocrAffinityQuery" : AFF_QUERY,
    "ocrHintInit" : HINT_INITS,
    "ocrSetHintValue" : HINT_SET_VALS,
    "ocrGuidRangeCreate" : GUID_RANGE_CREATES}

# Inverse of above lookup table
invNameDict = {
    EDT_CREATES : "ocrEdtCreate",
    TEMPLATE_CREATES : "ocrEdtTemplateCreate",
    EVENT_CREATES : "ocrEventCreate",
    EVENT_SATS : "ocrEventSatisfy",
    EVENT_DESTROYS : "ocrEventDestroy",
    ADD_DEPS : "ocrAddDependence",
    DB_CREATES : "ocrDbCreate",
    DB_RELEASES : "ocrDbRelease",
    DB_DESTROYS : "ocrDbDestroy",
    AFF_GET_CUR : "ocrAffinityGetCurrent",
    AFF_GET_AT : "ocrAffinityGetAt",
    AFF_COUNT : "ocrAffinityCount",
    AFF_QUERY : "ocrAffinityQuery",
    HINT_INITS : "ocrHintInit",
    HINT_SET_VALS : "ocrSetHintValue",
    GUID_RANGE_CREATES : "ocrGuidRangeCreate"}


#EDT object to characterize the each task and its actions
class EDT:

    def __init__(self, guid, funcPtr, pdId, wId, paramc, paramv, calls, callParams, callCounts):
        self.guid = guid
        self.funcPtr = funcPtr
        self.pdId = pdId
        self.wId = wId
        self.paramc = paramc
        self.paramv = paramv
        self.calls = calls
        self.callParams = callParams
        self.callCounts = callCounts


#Sort entire log by timestamp
def sortAscendingTime(logFile):

    logCopy = logFile
    logCopy.sort(key = lambda x: x.split()[TIMESTAMP])

    return logCopy


#Lookup name of API call for current log record
def getApiCallName(logRecord):
    ttype = logRecord.split()[TTYPE]
    taction = logRecord.split()[TACTION]
    return apiNameLookupTable[ttype+taction]


#Lookup function name associated with a function pointer from the app's symbol table
def getNameFromFuncPtr(fp):
    try:
        name = os.popen("nm "+str(pathToBinary[0])+" | grep "+str(fp)[2:]).read()
    except:
        print "ERROR: Unable to open application binary... Double check that the path to the application binary is correct"
    return name.split()[-1]


#Get parameters passed to each call from log record
def storeApiCallParams(logRec):
    cur = logRec.split()

    ttype = cur[TTYPE]
    taction = cur[TACTION]
    callName = apiNameLookupTable[ttype+taction]
    if callName == "EXECUTE" or callName == "FINISH":
        return []

    params = []

    if callName == "ocrEdtCreate":
        #template guid
        params.append(cur[23])
        #depc
        params.append(cur[26])
        #paramc
        params.append(cur[29])
    elif callName == "ocrEdtTemplateCreate":
        #func ptr
        params.append(cur[23])
        #depc
        params.append(cur[26])
        #paramc
        params.append(cur[29])
    elif callName == "ocrEventCreate":
        #event type
        params.append(cur[23])
    elif callName == "ocrEventSatisfy":
        #evt Guid:
        params.append(cur[23])
        #satisfyee
        params.append(cur[26])
    elif callName == "ocrEventDestroy":
        #evt Guid
        params.append(cur[23])
    elif callName == "ocrAddDependence":
        #src
        params.append(cur[23])
        #dest
        params.append(cur[26])
        #slot
        params.append(cur[29])
        #access mode
        params.append(cur[32])
    elif callName == "ocrDbCreate":
        #size
        params.append(cur[23])
    elif callName == "ocrDbRelease":
        #guid to release
        params.append(cur[23])
    elif callName == "ocrDbDestroy":
        #guid to destroy
        params.append(cur[23])
    #TODO: Add these params in tracing if needed. Not immediatelly critical
    elif callName == "ocrAffinityGetCurrent":
        pass
    elif callName == "ocrAffinityGetAt" :
        pass
    elif callName == "ocrAffinityCount" :
        pass
    elif callName == "ocrAffinityQuery" :
        pass
    elif callName == "ocrHintInit" :
        pass
    elif callName == "ocrSetHintValue" :
        pass
    elif callName == "ocrGuidRangeCreate" :
        pass

    return params


#Create EDT objects and compute the names and counts of API calls made per EDT
def getCallsInEdt(log):

    edtObjs = []
    edtObjsIdxMap = []
    uniqFps = []
    instCounts = {}
    templateGuids = {}
    apiFlags = {}

    global templateFpRefs
    totalSum = 0
    #Start by looping over log, finding EDTs that executed and initializing and EDT object with basic info
    for line in log:
        if line.split()[TACTION] == "EXECUTE":
            guid = line.split()[EXE_GUID]
            pd   = line.split()[PD_ID]
            fp   = line.split()[EXE_FP]

            uniqFps.append(fp)
            instCounts[fp] = 0
            wId  = line.split()[WRKR_ID]
            #Get paramc and paramv from this log record
            retList = getEdtParams(line)
            paramc = retList[0]
            paramv = retList[1]
            edtObjs.append(EDT(guid, fp, pd, wId, paramc, paramv, [], [],  [0 for j in range(0, len(nameDict))]))
            edtObjsIdxMap.append(guid)

    #Create dictionary mapping EDT guids to respective EDT object.
    uniqFps = list(set(uniqFps))
    dictMap = dict(itertools.izip(edtObjsIdxMap, edtObjs))

    #Initialze count values for executing funcPtr to funcPtr referenced in templateCreate() call
    for i in uniqFps:
        templateFpRefs[i] = {}
        apiFlags[i] = False
        for j in uniqFps:
            templateFpRefs[i][j] = 0

    #Loop over log and fully populate inter-EDT api calls and call counts
    curExeFp = ""

    for line in log:
        curRec = line.split()
        ttype = curRec[TTYPE]
        taction = curRec[TACTION]
        if ttype == "EDT" and taction == "TEMPLATE_CREATE":
            templGuid = curRec[TEMPLATE_GUID]
            templFp = curRec[TEMPLATE_FP]
            templateGuids[templGuid] = templFp

            #Condition to handle template create call made by runtime for mainEdt
            if curExeFp != "":
                templateFpRefs[curExeFp][templFp] += 1

        if ttype[:3] == "API" or taction == "EXECUTE" or taction == "FINISH":
            guid = curRec[EDT_GUID]
            if taction == "EXECUTE":
                fp = curRec[EXE_FP]
                curExeFp = fp
                instCounts[fp]+=1

            #Condition to avoid key lookup error if trace event happened prior to mainEdt (runtime routines)
            if guid == "0x0":
                continue

            dictMap[guid].calls.append(line)
            dictMap[guid].callParams.append(storeApiCallParams(line))

            if taction == "EXECUTE" or taction == "FINISH":
                continue
            else:
                apiFlags[dictMap[guid].funcPtr] = True

            callName = getApiCallName(line)
            callCountIdx = nameDict[callName]

            dictMap[guid].callCounts[callCountIdx] += 1
            totalSum += 1

    uniqFps = list(set(uniqFps))
    return [dictMap, uniqFps, instCounts, templateGuids, apiFlags]


#Get paramc and paramv for each EDT execution
def getEdtParams(exeRec):
    paramc = int(exeRec.split()[EXE_PARAMC])
    paramvStart = EXE_PARAMV
    paramvEnd = paramvStart + (2*paramc)

    paramv = []
    pvEle = EXE_PARAMV
    for i in range(0, paramc):
        paramv.append(exeRec.split()[pvEle])
        pvEle += 2

    return [paramc, paramv]


# Basic callable test fit functions called by curve_fit
def allFit(xData, a, b, c, d, e, f, g):
    return a*(xData**-3) + b*(xData**-2) + c*(xData**-1) + d*(xData**0) + e*(xData**1) + f*(xData**2) + g*(xData**3)

# Get coefficients for polynomial via scipy curve fit function
def tryAll(x, y):

    #Here we have to do a few checks and a small generalization.  This accounts for situations
    #Where y values have an erroneous off by one value (e.g. y = [1, 1, 1, 1, 1, 1, 2])
    #The polynomial produced from trying to curve fit these instances will be enormous and cause
    #unpredictable and innaccurate behavior in simulated applications, when it would be much more
    #accurate to just assume that the y-value should all be a constant 1. So we make an assumption
    #And equalize all value if each value differs from the average y-value by a narrow tolerance.

    allEq = all(i==y[0] for i in y)
    m = np.mean(y)

    #If y values are not all equal
    if not allEq:
        resetFlag = True

        #If each element is with +or- TOLERANCE - reset all y values to rounded average.
        for i in y:
            if i > (m-TOLERANCE) and i < (m+TOLERANCE):
                continue
            else:
                resetFlag = False
                break

        if resetFlag:
            newY = []
            for j in y:
                newY.append(round(m))
            newY = np.array(newY, dtype=float)
            y = newY

    popt, pcov = optimize.curve_fit(allFit, x, y)
    return popt


# Generate polynomial to determine relationships between application
# paramaters and number of API calls that occur within each EDT
def generatePolynomial(appToEdtDict, uniqFps, fp, apiCallType, instCounts, fpNames, templateDict):

    xs = []
    ys = []
    multX = []
    multY = []

    #seperate x,y pairs into seperate lists
    dummyCounts = []
    for i in appToEdtDict:
        dummyCount = 0
        curY = 0
        nameList = []
        nameDict = {}
        for j in appToEdtDict[i]:
            edt =  appToEdtDict[i][j]
            if edt.funcPtr != fp:
                continue
            dummyCount += 1
            curY += edt.callCounts[apiCallType]

            # Handle cases in which edtCreate or templateCreate make multiple calls associated
            # with different function pointers
            if apiCallType == EDT_CREATES:
                for x in range(len(edt.calls)):
                    curCall = edt.calls[x].split()
                    ttype = curCall[TTYPE]
                    taction = curCall[TACTION]
                    if ttype == "API_EDT" and taction == "CREATE":
                        apiCallParams = edt.callParams[x]
                        templateGuid = apiCallParams[0]

                        curParamDict = templateDict[i]
                        #print templateDict
                        #sys.exit(0)
                        curFp = curParamDict[templateGuid]
                        curName = fpNames[curFp]
                        nameList.append(curName)

                        if curName not in nameDict:
                            nameDict[curName] = 1
                        else:
                            nameDict[curName] += 1

            #Handle Multiple template creates with different functions
            if apiCallType == TEMPLATE_CREATES:
                for x in range(len(edt.calls)):
                    curCall = edt.calls[x].split()
                    ttype = curCall[TTYPE]
                    taction = curCall[TACTION]
                    if ttype == "API_EDT" and taction == "TEMPLATE_CREATE":
                        apiCallParams = edt.callParams[x]
                        curFp = apiCallParams[0]
                        curName = fpNames[curFp]
                        nameList.append(curName)
                        if curName not in nameDict:
                            nameDict[curName] = 1
                        else:
                            nameDict[curName] += 1

        #removed duplicate edt names
        nameList = list(set(nameList))

        if apiCallType == EDT_CREATES or apiCallType == TEMPLATE_CREATES:

            #initialize list of x and ys for each edt name
            if len(multX) == 0:
                multX = [[] for arg in range(len(nameDict))]
            if len(multY) == 0:
                multY = [[] for arg in range(len(nameDict))]
            #append x, y, lists to list of each edt created
            for ii in range(len(nameList)):
                multX[ii].append(i)
                multY[ii].append({nameList[ii] : nameDict[nameList[ii]]})

        xs.append(i)
        ys.append(curY)

    # for edtCreates() and templateCreates() return a dictionary containing multiple
    # sets of coefficients for x, y pairs per respective call type
    if apiCallType == EDT_CREATES or apiCallType == TEMPLATE_CREATES:
        assert len(multX) == len(multY)
        ret = {}
        #arrange [ [x,y], ... ] pairs
        for i in range(len(multX)):
            curX = multX[i]
            curY = multY[i]
            vals = []
            name = ''

            for j in curY:
                vals.append(j.values()[0])
                name = j.keys()[0]

            curY = vals

            #Sort tuples by x value
            zipped = zip(curX, curY)
            zipped = sorted(zipped, key=itemgetter(0))
            unzipped = zip(*zipped)

            xs = np.array(unzipped[0], dtype=float)
            ys = np.array(unzipped[1], dtype=float)

            for i in range(len(ys)):
                try:
                    ys[i] = round(ys[i]/instCounts[xs[i]][fp])
                except:
                    continue
                #Force round up if and only if it rounds down to zero.  Otherswise this API call will be totally ignored
                if ys[i] == 0:
                    ys[i] = 1

            #This check adds a 0 count to the input files for API calls that were made at 1
            #or more input sizes, but not at 1 or more others
            if len(ys) < 7:
                tempX = xs.tolist()
                tempY = ys.tolist()
                for m in appParams:
                    if m not in tempX:
                        tempX.append(m)
                        tempY.append(0)
                xs = np.array(tempX, dtype=float)
                ys = np.array(tempY, dtype=float)

            coeffs = tryAll(xs, ys)
            ret[name] = coeffs

        return ret

    else:
        #Sort tuples by x val
        zipped = zip(xs, ys)
        zipped = sorted(zipped, key=itemgetter(0))
        unzipped = zip(*zipped)

        xs = np.array(unzipped[0], dtype=float)
        ys = np.array(unzipped[1], dtype=float)

        for i in range(len(ys)):
            try:
                ys[i] = math.ceil(ys[i]/instCounts[xs[i]][fp])
            except:
                continue

        if len(ys) < 7:
            tempX = xs.tolist()
            tempY = ys.tolist()
            for m in appParams:
                if m not in tempX:
                    tempX.append(m)
                    tempY.append(0)
            xs = np.array(tempX, dtype=float)
            ys = np.array(tempY, dtype=float)

        coeffs = tryAll(xs, ys)
        return coeffs

# Arrange polynomial data for each API call made within each EDT
def resolveApiCallPolynomials(appToEdtDict, constCallDict, uniqFps, instCounts, fpNames, templateDict):

    fpPoly = {}

    # Use all EDT executions from all trace files to calculate a polynomial for each
    # EDT type / API call pair to determine generic relationships between application
    # paramaters, and how many times each API call is made during EDT execution
    for fp in uniqFps:

        #Initialze dictionary for fp:apiCall relationship coefficients
        fpPoly[fp] = [None for i in range(0, len(nameDict))]

        polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, EDT_CREATES, instCounts, fpNames, templateDict)
        fpPoly[fp][EDT_CREATES] = polyIdx

        polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, TEMPLATE_CREATES, instCounts, fpNames, templateDict)
        fpPoly[fp][TEMPLATE_CREATES] = polyIdx

        if EVENT_CREATES not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, EVENT_CREATES, instCounts, fpNames, templateDict)
            fpPoly[fp][EVENT_CREATES] = polyIdx

        if EVENT_SATS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, EVENT_SATS, instCounts, fpNames, templateDict)
            fpPoly[fp][EVENT_SATS] = polyIdx

        if EVENT_DESTROYS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, EVENT_DESTROYS, instCounts, fpNames, templateDict)
            fpPoly[fp][EVENT_DESTROYS] = polyIdx

        if ADD_DEPS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, ADD_DEPS, instCounts, fpNames, templateDict)
            fpPoly[fp][ADD_DEPS] = polyIdx

        if DB_CREATES not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, DB_CREATES, instCounts, fpNames, templateDict)
            fpPoly[fp][DB_CREATES] = polyIdx

        if DB_RELEASES not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, DB_RELEASES, instCounts, fpNames, templateDict)
            fpPoly[fp][DB_RELEASES] = polyIdx

        if DB_DESTROYS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, DB_DESTROYS, instCounts, fpNames, templateDict)
            fpPoly[fp][DB_DESTROYS] = polyIdx

        if AFF_GET_CUR not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, AFF_GET_CUR, instCounts, fpNames, templateDict)
            fpPoly[fp][AFF_GET_CUR] = polyIdx

        if AFF_GET_AT not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, AFF_GET_AT, instCounts, fpNames, templateDict)
            fpPoly[fp][AFF_GET_AT] = polyIdx

        if AFF_COUNT not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, AFF_COUNT, instCounts, fpNames, templateDict)
            fpPoly[fp][AFF_COUNT] = polyIdx

        if AFF_QUERY not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, AFF_QUERY, instCounts, fpNames, templateDict)
            fpPoly[fp][AFF_QUERY] = polyIdx

        if HINT_INITS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, HINT_INITS, instCounts, fpNames, templateDict)
            fpPoly[fp][HINT_INITS] = polyIdx

        if HINT_SET_VALS not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, HINT_SET_VALS, instCounts, fpNames, templateDict)
            fpPoly[fp][HINT_SET_VALS] = polyIdx

        if GUID_RANGE_CREATES not in constCallDict[fp]:
            polyIdx = generatePolynomial(appToEdtDict, uniqFps, fp, GUID_RANGE_CREATES, instCounts, fpNames, templateDict)
            fpPoly[fp][GUID_RANGE_CREATES] = polyIdx


    return fpPoly


# number/type of API calls each instance
def resolveConstantNumCalls(edtData):

    #Initialize dictionary
    toReturn = {}

    for i in edtData:
        toReturn[i] = []

    #Iterate each EDT type
    for i in edtData:
        edtCreates = []
        templateCreates = []
        eventCreates = []
        eventSats = []
        eventDestroys = []
        addDeps = []
        dbCreates = []
        dbReleases = []
        dbDestroys = []
        affGetCurs = []
        affGetAts = []
        affCounts = []
        affQueries = []
        hintInits = []
        hintSets = []
        guidRangeCreates = []

        #Iterate through through call counts per run
        for j in edtData[i]:
            ec  = j[EDT_CREATES]
            #check if call count observed in the current EDT has already been observed
            if ec not in edtCreates:
                #If not, add to a list. If the list grows > 1 we know it has a non-constant number of calls
                edtCreates.append(ec)

            tc  = j[TEMPLATE_CREATES]
            if tc not in templateCreates:
                templateCreates.append(tc)

            evc = j[EVENT_CREATES]
            if evc not in eventCreates:
                eventCreates.append(evc)

            evs = j[EVENT_SATS]
            if evs not in eventSats:
                eventSats.append(evs)

            ed = j[EVENT_DESTROYS]
            if ed not in eventDestroys:
                eventDestroys.append(ed)

            ad  = j[ADD_DEPS]
            if ad not in addDeps:
                addDeps.append(ad)

            dc  = j[DB_CREATES]
            if dc not in dbCreates:
                dbCreates.append(dc)

            dr  = j[DB_RELEASES]
            if dr not in dbReleases:
                dbReleases.append(dr)

            dd  = j[DB_DESTROYS]
            if dd not in dbDestroys:
                dbDestroys.append(dd)

            agc  = j[AFF_GET_CUR]
            if agc not in affGetCurs:
                affGetCurs.append(agc)

            aga  = j[AFF_GET_AT]
            if aga not in affGetAts:
                affGetAts.append(aga)

            ac  = j[AFF_COUNT]
            if ac not in affCounts:
                affCounts.append(ac)

            aq  = j[AFF_QUERY]
            if aq not in affQueries:
                affQueries.append(aq)

            hi  = j[HINT_INITS]
            if hi not in hintInits:
                hintInits.append(hi)

            hsv  = j[HINT_SET_VALS]
            if hsv not in hintSets:
                hintSets.append(hsv)

            grc  = j[GUID_RANGE_CREATES]
            if grc not in guidRangeCreates:
                guidRangeCreates.append(grc)

        #If only a single call count was detected add api call constant to a list to return
        if len(edtCreates) == 1:
            toReturn[i].append(EDT_CREATES)
        if len(templateCreates) == 1:
            toReturn[i].append(TEMPLATE_CREATES)
        if len(eventCreates) == 1:
            toReturn[i].append(EVENT_CREATES)
        if len(eventSats) == 1:
            toReturn[i].append(EVENT_SATS)
        if len(eventDestroys) == 1:
            toReturn[i].append(EVENT_DESTROYS)
        if len(addDeps) == 1:
            toReturn[i].append(ADD_DEPS)
        if len(dbCreates) == 1:
            toReturn[i].append(DB_CREATES)
        if len(dbReleases) == 1:
            toReturn[i].append(DB_RELEASES)
        if len(dbDestroys) == 1:
            toReturn[i].append(DB_DESTROYS)
        if len(affGetCurs) == 1:
            toReturn[i].append(AFF_GET_CUR)
        if len(affGetAts) == 1:
            toReturn[i].append(AFF_GET_AT)
        if len(affCounts) == 1:
            toReturn[i].append(AFF_COUNT)
        if len(affQueries) == 1:
            toReturn[i].append(AFF_QUERY)
        if len(hintInits) == 1:
            toReturn[i].append(HINT_INITS)
        if len(hintSets) == 1:
            toReturn[i].append(HINT_SET_VALS)
        if len(guidRangeCreates) == 1:
            toReturn[i].append(GUID_RANGE_CREATES)


    return toReturn


# Get order of API calls made in EDT.
def getCallOrder(edtObj):
    ordered = []
    for call in edtObj.calls:
        cur = call.split()
        ttype = cur[TTYPE]
        taction = cur[TACTION]
        callName = apiNameLookupTable[ttype+taction]
        #NOTE This is an optimistic attempt to preserve order. Not effective if an EDT
        #     has any alternating API calls and probably impossible without compiler support.
        if callName not in ordered:
            ordered.append(callName)
    return ordered


# Write loop variable decleration based on polynomial
def getLoopVarStr(pCoeffs):

    correctedCoeffs = []

    #Round coefficients whose dec. is within .01 of a whole number
    for i in pCoeffs:
        curS = str(i)
        curF = i

        pastDec = False
        newStr = ''

        for j in curS:
            if j == '.':
                pastDec = True
                continue
            if pastDec == True:
                newStr+=j

        if newStr[:2] == "99" or newStr[:2] == "00":
            curF = round(curF)
        correctedCoeffs.append(curF)

    pCoeffs = correctedCoeffs

    #Create polynomial string, ommitting insignificant values
    polyStr = "(int)floor("
    if abs(pCoeffs[0]) >= 1:
        polyStr += str(pCoeffs[0])+"*pow(swArg, -3.0) + "
    if abs(pCoeffs[1]) >= 1:
        polyStr += str(pCoeffs[1])+"*pow(swArg, -2.0) + "
    if abs(pCoeffs[2]) >= 1:
        polyStr += str(pCoeffs[2])+"*pow(swArg, -1.0) + "
    if abs(pCoeffs[3]) >= 1:
        polyStr += str(pCoeffs[3])+"*pow(swArg,  0.0) + "
    if abs(pCoeffs[4]) >= 1:
        polyStr += str(pCoeffs[4])+"*pow(swArg, 1.0) + "
    if abs(pCoeffs[5]) >= 1:
        polyStr += str(pCoeffs[5])+"*pow(swArg, 2.0) + "
    if abs(pCoeffs[6]) >= 1:
        polyStr += str(pCoeffs[6])+"*pow(swArg, 3.0) + "
    polyStr += ")"

    #Order not guaranteed, remove trailing '+' character
    spl = polyStr.split()
    #below check is if all coeffs == 0, return an empty string if so
    if len(spl) == 1:
        return ''
    if spl[-2] == "+":
        spl[-2] = ''
    polyStr = "".join(spl)

    return polyStr

# Arrange preamble information fro API calls (i.e declaring/initializing guids)
def getPreApiStr(callName, hintVar, *perf):

    apiStr = []

    #edtCreates handled in a special way, as we add average HW counters to each EDT created through the hints API
    if callName == "ocrEdtCreate":
        if perf:
            perfStats = perf[0]

            apiStr.append("\n\t#ifdef INCLUDE_SCALABILITY_HINTS")
            apiStr.append("ocrHint_t hint"+str(hintVar)+";")
            apiStr.append("ocrHintInit(&hint"+str(hintVar)+", OCR_HINT_EDT_T);")
            apiStr.append("ocrSetHintValue(&hint"+str(hintVar)+", OCR_HINT_EDT_STATS_HW_CYCLES, "+str(perfStats[OUT_CYCLES])+");")
            apiStr.append("ocrSetHintValue(&hint"+str(hintVar)+", OCR_HINT_EDT_STATS_CACHE_REFS, "+str(perfStats[OUT_CACHE_REFS])+");")
            apiStr.append("ocrSetHintValue(&hint"+str(hintVar)+", OCR_HINT_EDT_STATS_CACHE_MISSES, "+str(perfStats[OUT_CACHE_MISSES])+");")
            apiStr.append("ocrSetHintValue(&hint"+str(hintVar)+", OCR_HINT_EDT_STATS_FLOAT_OPS, "+str(perfStats[OUT_FP_OPS])+");")
            apiStr.append("#endif\n")

    return apiStr


# Get paramaters to each API call and arrange C syntax for writing
def getApiCallArgsStr(callName, loopVarStr, hintVar, *edtName):
    apiStr = []

    if callName == "ocrEdtCreate":
        apiStr.append("ocrGuid_t taskGuid;")
        if edtName:
            curName = edtName[0]
            apiStr.append("#ifdef INCLUDE_SCALABILITY_HINTS")
            apiStr.append(str(callName)+"(&taskGuid, "+curName+", 1, nparamv, 0, NULL, EDT_PROP_NONE, &hint"+str(hintVar)+", NULL);")
            apiStr.append("#else")
            apiStr.append(str(callName)+"(&taskGuid, "+curName+", 1, nparamv, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);")
            apiStr.append("#endif")
        else:
            #Should not get hit, if so fix coefficient dictionary creation in EDT_CREATES
            curName = "FIXME"
            apiStr.append(str(callName)+"(&taskGuid, "+curName+", 0, NULL, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);")

    elif callName == "ocrEdtTemplateCreate":
        if edtName:
            curName = edtName[0]
            apiStr.append(str(callName)+"(&ocrTask, "+curName+", 1, 0);")
        else:
            #Should not get hit, if so fix coefficient dictionary creation in TEMPLATE_CREATES
            curName = "FIXME"
            apiStr.append(str(callName)+"(&taskGuid, "+curName+", 0, NULL, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);")

    elif callName == "ocrEventCreate":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_OBJ_CREATIONS")
        apiStr.append("ocrGuid_t evt;")
        apiStr.append(str(callName)+"(&evt, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG);")
        apiStr.append("#endif")

    elif callName == "ocrEventSatisfy":
        apiStr.append(str(callName)+"(NULL_GUID, NULL_GUID);")

    elif callName == "ocrAddDependence":
        apiStr.append(str(callName)+"(NULL_GUID, NULL_GUID, 0, 0);")

    elif callName == "ocrDbCreate":
        apiStr.append("#ifdef INCLUDE_OBJ_CREATIONS")
        apiStr.append("ocrGuid_t db;")
        apiStr.append("void *ptr;")
        apiStr.append(str(callName)+"(&db, (void**)&ptr, 1, 0, NULL_HINT, NO_ALLOC);")
        apiStr.append("#endif")

    elif callName == "ocrDbDestroy":
        apiStr.append(str(callName)+"(NULL_GUID);")

    elif callName == "ocrDbRelease":
        apiStr.append(str(callName)+"(NULL_GUID);")

    elif callName == "ocrAffinityGetCurrent":
        apiStr.append("#ifdef INCLUDE_AFFINITY_CALLS")
        apiStr.append(str(callName)+"(NULL);")
        apiStr.append("#endif")

    elif callName == "ocrAffinityGetAt":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_AFFINITY_CALLS")
        apiStr.append(str(callName)+"(AFFINITY_CURRENT, 0, NULL);")
        apiStr.append("#endif")

    elif callName == "ocrAffinityCount":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_AFFINITY_CALLS")
        apiStr.append(str(callName)+"(AFFINITY_SIM, NULL);")
        apiStr.append("#endif")

    elif callName == "ocrAffinityQuery":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_AFFINITY_CALLS")
        apiStr.append(str(callName)+"(NULL_GUID, NULL, NULL);")
        apiStr.append("#endif")

    elif callName == "ocrHintInit":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_HINT_CALLS")
        apiStr.append(str(callName)+"(NULL, OCR_HINT_UNDEF_T);")
        apiStr.append("#endif")

    elif callName == "ocrSetHintValue":
        #Arbitrary values
        apiStr.append("#ifdef INCLUDE_HINT_CALLS")
        apiStr.append(str(callName)+"(NULL, OCR_HINT_EDT_DISPERSE, 0);")
        apiStr.append("#endif")

    elif callName == "ocrGuidRangeCreate":
        #Arbitrary values
        apiStr.append(str(callName)+"(NULL, 1, GUID_USER_EDT);")

    return apiStr


# Create data structures containing total perf counters and instance counts for all EDTs
def computeTotalPerfCounts(allEdtData, uniqFps):
    perfCtrsPerEdt = {}
    instCounts = {}
    for i in uniqFps:
        perfCtrsPerEdt[i] = [0 for j in range(0, 4)]
        instCounts[i] = 0

    for i in range(len(allEdtData)):
        for j in allEdtData[i].itervalues():
            fp = j.funcPtr
            finishRec = j.calls[-1]
            if finishRec.split()[TACTION] != "FINISH":
#                for c in j.calls:
#                    print c
                #print "WTF: ", fp
                continue
            assert(finishRec.split()[TACTION] == "FINISH")
            perfCtrsPerEdt[fp][0] += int(finishRec.split()[FIN_CYCLES])
            perfCtrsPerEdt[fp][1] += int(finishRec.split()[FIN_CACHE_REFS])
            perfCtrsPerEdt[fp][2] += int(finishRec.split()[FIN_CACHE_MISSES])
            perfCtrsPerEdt[fp][3] += int(finishRec.split()[FIN_FP_OPS])
            instCounts[fp] += 1

    return instCounts, perfCtrsPerEdt


# Compute perf counter averages and C syntax to be written
def getAvePerfCtrs(fp, instCounts, perfCounts):

    #TODO  Future if needed: simplifying assumption for initial implementation with Stencil.

    #      The fine granularity of compute observed in Stencil2D renders it sufficient to
    #      just take averages for performance counters accross all problem sizes.  In the
    #      future, if we "skeletonize" other applications, we will need to define these values
    #      based on a polynomial of application input, much like API call counts are handled.
    curCount = instCounts[fp]
    cycles = perfCounts[fp][0]/curCount
    cacheRefs = perfCounts[fp][1]/curCount
    cacheMisses = perfCounts[fp][2]/curCount
    fpOps = perfCounts[fp][3]/curCount

    perfStats = []

    perfStats.append(cycles)
    perfStats.append(cacheRefs)
    perfStats.append(cacheMisses)
    perfStats.append(fpOps)

    return perfStats


# Write the finish task: Basically a sink edt that is satisfied/runs once all other EDTs have run
# Workaround to ensure the app finishes, since finish conditions are often dictated by data.
def writeFinishTask(outFile):
    outFile.write("ocrGuid_t finishTask( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {\n")
    outFile.write("\tPRINTF(\"FINISH\\n\");\n")
    outFile.write("\tocrShutdown();\n")
    outFile.write("\treturn NULL_GUID;\n\n")
    outFile.write("}\n\n")


# Write "proxy" for mainEdt. Essentially spawns off a spawner task that acts as a proxy for main
# with the EDT_PROP_FINISH flag
def writeProxyMain(outFile):

    outFile.write("ocrGuid_t mainEdt( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {\n\n")

    outFile.write("\tu32 lVal;\n")
    outFile.write("\ts32 swArg = (s32) atoi(getArgv(depv[0].ptr, 1));\n")
    outFile.write("\tu64 nparamv[1] = {swArg};\n\n")

    outFile.write("\tocrGuid_t spawnerTemplate;\n")
    outFile.write("\tocrGuid_t spawnerEdt;\n")
    outFile.write("\tocrGuid_t outEvt;\n\n")

    outFile.write("\tocrEdtTemplateCreate(&spawnerTemplate, spawnerTask, 1, 0);\n")
    outFile.write("\tocrEdtCreate(&spawnerEdt, spawnerTemplate, 1, nparamv, 0, NULL, EDT_PROP_FINISH, NULL_HINT, &outEvt);\n\n")

    outFile.write("\tocrGuid_t finishTemplate;\n")
    outFile.write("\tocrGuid_t finishEdt;\n\n")

    outFile.write("\tocrGuid_t finishDepv[1] = {outEvt};\n")
    outFile.write("\tocrEdtTemplateCreate(&finishTemplate, finishTask, 0, 1);\n")
    outFile.write("\tocrEdtCreate(&finishEdt, finishTemplate, 0, NULL, 1, finishDepv, EDT_PROP_FINISH, NULL_HINT, NULL);\n\n")

    outFile.write("\treturn NULL_GUID;\n")
    outFile.write("}\n")


# Resolve necessary output from Data Structures. Arrange appropriately, and write to a .c file
def writeToFile(uniqFps, edtOrdering, toOutput, instCounts, perfCounts, fpNames):
    global mainEdtFp
    global templateFpRefs

    outFile = open('simApp.c', 'w')

    #write includes to C file
    outFile.write("#include \"ocr.h\"\n")
    outFile.write("#include <math.h>\n")
    outFile.write("#include <stdlib.h>\n\n\n")

    #write all function signatures asside from mainEdt.
    for i in uniqFps:
        funcName =  fpNames[i]
        if funcName != "mainEdt":
            outFile.write("ocrGuid_t "+funcName+"(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);\n")
    outFile.write("ocrGuid_t finishTask(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);\n")
    outFile.write("ocrGuid_t spawnerTask(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);\n")
    outFile.write("\n\n\n")

    mainEdtStr = []
    funcStrs = []

    loopVarCount = 0
    guidVarCount = 0
    hintVarCount = 0

    ##################################################################################################
    # Bulk of the C syntax arrangement occurs in the following loop.  Effectively what is performed  #
    # by this loop entails looping over each function pointer, retrieving API calls made by that EDT #
    # associated with the FP, retrieving the coefficient for counts of each API call, writing loop   #
    # variable declerations, and syntactically correct API calls with necessary parameters to allow  #
    # runtime to compile and link.                                                                   #
    ##################################################################################################
    for fp in uniqFps:
        curName = fpNames[fp]
        templateGuidsPerFp = []
        #Handle main seperately, as we need to write the spawner task
        if curName == "mainEdt":

            mainEdtStr.append("ocrGuid_t spawnerTask( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {\n\n")
            mainEdtStr.append("\tu64 swArg = paramv[0];\n")
            mainEdtStr.append("\tu64 nparamv[1] = {swArg};\n")
            mainEdtStr.append("\tu32 lVal;\n")

            for apiCall in edtOrdering[fp]:
                if apiCall == "EXECUTE" or apiCall == "FINISH":
                    continue
                else:
                    index = nameDict[apiCall]
                    callInfo = toOutput[fp][index]

                    callName = callInfo[0]

                    perfStats = getAvePerfCtrs(fp, instCounts, perfCounts)
                    preApiStr = getPreApiStr(callName, hintVarCount, perfStats)
                    apiStr = getApiCallArgsStr(callName, ("loopVar"+str(loopVarCount)), hintVarCount)

                    if len(callInfo) == 2:
                        #Constant number of calls
                        callCount = callInfo[1]

                        mainEdtStr.append("\tu32 loopVar"+str(loopVarCount)+";\n")

                        for line in preApiStr:
                            mainEdtStr.append("\t"+line+"\n")
                        mainEdtStr.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < "+str(callCount)+"; loopVar"+str(loopVarCount)+"++){\n")

                        for line in apiStr:
                            mainEdtStr.append("\t\t"+line+"\n")
                        mainEdtStr.append("\t}\n\n")
                        loopVarCount+=1
                    else:
                        #variable num calls
                        polyCoeffs = callInfo[2]

                        # This var type will only be a dictionary if multiple types of EDTs and/or templates are being
                        # generated, and needs to be handled uniquely.
                        if type(polyCoeffs) is dict:

                            curFuncSub = []
                            if apiCall == "ocrEdtCreate":
                                for i in polyCoeffs:

                                    curName = i

                                    inv_map = {v: k for k, v in fpNames.iteritems()}
                                    curFp = inv_map[curName]
                                    curCoeffs = polyCoeffs[i]

                                    curFuncSub.append("\tocrGuid_t templateGuid"+str(guidVarCount)+";\n")
                                    templateGuidsPerFp.append("templateGuid"+str(guidVarCount))

                                    curFuncSub.append("\tocrEdtTemplateCreate(&templateGuid"+str(guidVarCount)+", "+str(curName)+", 1, 0);\n\n")

                                    perfStats = getAvePerfCtrs(curFp, instCounts, perfCounts)
                                    preApiStr = getPreApiStr(callName, hintVarCount, perfStats)
                                    apiStr = getApiCallArgsStr(callName, ("loopVar"+str(loopVarCount)), hintVarCount, "templateGuid"+str(guidVarCount))
                                    hintVarCount+=1

                                    lVarStr = getLoopVarStr(curCoeffs)
                                    curFuncSub.append("\tu32 loopVar"+str(loopVarCount)+";\n")
                                    curFuncSub.append("\tlVal = " + lVarStr + ";\n")
                                    for line in preApiStr:
                                        curFuncSub.append("\t"+line+"\n")
                                    curFuncSub.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < lVal; loopVar"+str(loopVarCount)+"++){\n")
                                    for line in apiStr:
                                        curFuncSub.append("\t\t"+line+"\n")
                                    curFuncSub.append("\t}\n\n")

                                    loopVarCount += 1
                                    guidVarCount += 1
                                for i in curFuncSub:
                                    mainEdtStr.append(i)

                            elif apiCall == "ocrEdtTemplateCreate":
                                pass


                        else:
                            lVarStr = getLoopVarStr(polyCoeffs)

                            mainEdtStr.append("\tu32 loopVar"+str(loopVarCount)+";\n")
                            mainEdtStr.append("\tlVal = " + lVarStr + ";\n")
                            for line in preApiStr:
                                mainEdtStr.append("\t"+line+"\n")
                            mainEdtStr.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < lVal; loopVar"+str(loopVarCount)+"++){\n")
                            for line in apiStr:
                                mainEdtStr.append("\t\t"+line+"\n")
                            mainEdtStr.append("\t}\n\n")
                            loopVarCount+=1

            for templ in templateGuidsPerFp:
                mainEdtStr.append("\tocrEdtTemplateDestroy("+str(templ)+");\n")

        #Handle functions other than mainEdt slightly differently
        else:
            curFunc = []
            curFunc.append("ocrGuid_t "+str(curName)+"( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {\n\n")

            #If edtOrdering for current function is <= 2. No api calls were made.  Don't declare loop vars
            try:
                if len(edtOrdering[fp]) > 2:
                    curFunc.append("\tu64 swArg = paramv[0];\n")
                    curFunc.append("\tu64 nparamv[1] = {swArg};\n")
                    curFunc.append("\tu32 lVal;\n")
            except:
                continue

            for apiCall in edtOrdering[fp]:
                if apiCall == "EXECUTE" or apiCall == "FINISH":
                    continue
                else:
                    index = nameDict[apiCall]
                    callInfo = toOutput[fp][index]

                    callName = callInfo[0]

                    perfStats = getAvePerfCtrs(fp, instCounts, perfCounts)
                    preApiStr = getPreApiStr(callName, hintVarCount, perfStats)
                    apiStr = getApiCallArgsStr(callName, ("loopVar"+str(loopVarCount)), hintVarCount)

                    if len(callInfo) == 2:
                        #Constant number of calls
                        callCount = callInfo[1]

                        curFunc.append("\tu32 loopVar"+str(loopVarCount)+";\n")
                        for line in preApiStr:
                            curFunc.append("\t"+line+"\n")
                        curFunc.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < "+str(callCount)+"; loopVar"+str(loopVarCount)+"++){\n")
                        for line in apiStr:
                            curFunc.append("\t\t"+line+"\n")
                        curFunc.append("\t}\n\n")
                        loopVarCount+=1
                    else:
                        #variable num calls
                        polyCoeffs = callInfo[2]

                        # This var type will only be a dictionary if multiple types of EDTs and/or templates are being
                        # generated, and needs to be handled uniquely.
                        if type(polyCoeffs) is dict:
                            curFuncSub = []
                            if apiCall == "ocrEdtCreate":

                                tcIdx = nameDict["ocrEdtTemplateCreate"]

                                tcs = []
                                for i in toOutput[fp][tcIdx][2]:
                                    tcs.append(i)

                                for i in polyCoeffs:

                                    curName = i

                                    #if there is not template create associated with this EDT creation in this EDT write one manually
                                    if curName != fpNames[fp]:
                                        inv_map = {v: k for k, v in fpNames.iteritems()}
                                        curFp = inv_map[curName]

                                        curFuncSub.append("\tocrGuid_t templateGuid"+str(guidVarCount)+";\n")
                                        templateGuidsPerFp.append("templateGuid"+str(guidVarCount))

                                        curFuncSub.append("\tocrEdtTemplateCreate(&templateGuid"+str(guidVarCount)+", "+str(curName)+", 1, 0);\n\n")

                                        curCoeffs = polyCoeffs[i]

                                        perfStats = getAvePerfCtrs(curFp, instCounts, perfCounts)
                                        preApiStr = getPreApiStr(callName, hintVarCount, perfStats)

                                        apiStr = getApiCallArgsStr(callName, ("loopVar"+str(loopVarCount)), hintVarCount, "templateGuid"+str(guidVarCount))
                                        hintVarCount+=1

                                        lVarStr = getLoopVarStr(curCoeffs)
                                        curFuncSub.append("\tu32 loopVar"+str(loopVarCount)+";\n")
                                        curFuncSub.append("\tlVal = " + lVarStr + ";\n")
                                        for line in preApiStr:
                                            curFuncSub.append("\t"+line+"\n")
                                        curFuncSub.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < lVal; loopVar"+str(loopVarCount)+"++){\n")
                                        for line in apiStr:
                                            curFuncSub.append("\t\t"+line+"\n")
                                        curFuncSub.append("\t}\n\n")

                                        loopVarCount += 1
                                        guidVarCount += 1

                                    else:
                                        curFuncSub.append("\t//FIXME: Template and EDT create calls with that same function pointer as its parent unsupported by this tool.  Consider adding code manually.\n\n")


                                for line in curFuncSub:
                                    curFunc.append(line)

                            elif apiCall == "ocrEdtTemplateCreate":
                                pass

                        else:
                            lVarStr = getLoopVarStr(polyCoeffs)
                            curFunc.append("\tu32 loopVar"+str(loopVarCount)+";\n")
                            curFunc.append("\tlVal = " + lVarStr + ";\n")
                            for line in preApiStr:
                                curFunc.append("\t"+line+"\n")
                            curFunc.append("\tfor(loopVar"+str(loopVarCount)+" = 0; loopVar"+str(loopVarCount)+" < lVal; loopVar"+str(loopVarCount)+"++){\n")
                            for line in apiStr:
                                curFunc.append("\t\t"+line+"\n")
                            curFunc.append("\t}\n\n")
                            loopVarCount+=1

            for templ in templateGuidsPerFp:
                curFunc.append("\tocrEdtTemplateDestroy("+str(templ)+");\n")

            funcStrs.append(curFunc)

    for i in funcStrs:
        for j in i:
            outFile.write(j)
        outFile.write("\treturn NULL_GUID;\n")
        outFile.write("}\n\n")
    writeFinishTask(outFile)
    for i in mainEdtStr:
        outFile.write(i)
    outFile.write("\treturn NULL_GUID;\n")
    outFile.write("}\n\n")
    writeProxyMain(outFile)

    outFile.close()


#Begin aggragating data to arrange application reconstructions
def reconstructSkeleton(allEdtData, allFpCallCounts, uniqFps, constCallDict, fpPoly, fpNames, apiFlags, maxCallTypes):

    #Get an EDT of each type to refer to for getting constant number of calls
    sampleEdtTypes = {}
    for i in uniqFps:
        for j in allEdtData[0]:
            if allEdtData[0][j].funcPtr == i:

                # Here we account for varying EDT behavior
                # Iterate over each function pointer (EDT) and count how many unique API calls are
                # made. Once we have encountered and EDT that matches the greatest observed number
                # of unique API calls, we select that EDT as our sample to represent the EDT's behavior
                uniqCalls = 0
                for k in allEdtData[0][j].callCounts:
                    if k != 0:
                        uniqCalls += 1

                if(uniqCalls != maxCallTypes[i]):
                    continue

                sampleEdtTypes[i] = allEdtData[0][j]
                break

    #Get API call order for sample EDTs
    sampleEdtOrdering = {}
    for i in sampleEdtTypes:
        order = getCallOrder(sampleEdtTypes[i])
        sampleEdtOrdering[i] = order

    #Get number of calls of samples to refer to if api call type is determined to be constant
    sampleEdtCounts = {}
    for i in uniqFps:
        for j in allFpCallCounts[i]:
            sampleEdtCounts[i] = j
            break

    #Aggragate polynomial data to a single data structure
    toOutput = {}
    for i in uniqFps:
        curFp = i
        toOutput[curFp] = [None for x in range(0, len(nameDict))]
        for j in constCallDict[curFp]:
            callName  = invNameDict[j]
            callIdx   = j

            callCount = sampleEdtCounts[i][j]
            toOutput[curFp][j] = [callName, callCount]

        #Kind of hacky. Count will basically iterate over each API name constant
        # This is done to avoid another big switch
        count = -1
        for x in fpPoly[curFp]:
            count +=1
            if x is None:
                continue
            else:
                callName = invNameDict[count]
            toOutput[curFp][count] = [callName, callCount, x]

    #Arrange data structure for average performance counters
    instCounts, perfCounts = computeTotalPerfCounts(allEdtData, uniqFps)

    #Write OCR app to .c file
    writeToFile(uniqFps, sampleEdtOrdering, toOutput, instCounts, perfCounts, fpNames)

# Print All EDT intance counts, and total API calls made observed per trace file (problem size)
#TODO The way this is printed could be implemented in a significantly neater way. Probably not needed though
def printAnalysis(appToEdtDict, uniqFps, fpNames, instCounts):

    print "Printing Analysis"

    for i in appToEdtDict:
        curEdtList = appToEdtDict[i]
        print "============================================================="
        print "                    APPLICATION INPUT SIZE: "+str(i)+"               "
        print "=============================================================\n\n"
        for fp in uniqFps:
            values = [0 for callType in range(len(nameDict))]
            curInstCount = instCounts[i][fp]
            print ' ---------- ' +str(fpNames[fp])+ ' (Executed ' +str(curInstCount)+' times) ------------\n'
            for edt in curEdtList:
                edtObj = curEdtList[edt]
                if edtObj.funcPtr == fp:
                    call = edtObj.callCounts

                    values[EDT_CREATES]        +=  call[EDT_CREATES]
                    values[TEMPLATE_CREATES]   +=  call[TEMPLATE_CREATES]
                    values[EVENT_CREATES]      +=  call[EVENT_CREATES]
                    values[EVENT_SATS]         +=  call[EVENT_SATS]
                    values[EVENT_DESTROYS]     +=  call[EVENT_DESTROYS]
                    values[ADD_DEPS]           +=  call[ADD_DEPS]
                    values[DB_CREATES]         +=  call[DB_CREATES]
                    values[DB_RELEASES]        +=  call[DB_RELEASES]
                    values[DB_DESTROYS]        +=  call[DB_DESTROYS]
                    values[AFF_GET_CUR]        +=  call[AFF_GET_CUR]
                    values[AFF_GET_AT]         +=  call[AFF_GET_AT]
                    values[AFF_COUNT]          +=  call[AFF_COUNT]
                    values[AFF_QUERY]          +=  call[AFF_QUERY]
                    values[HINT_INITS]         +=  call[HINT_INITS]
                    values[HINT_SET_VALS]      +=  call[HINT_SET_VALS]
                    values[GUID_RANGE_CREATES] +=  call[GUID_RANGE_CREATES]

            if values[EDT_CREATES] > 0:
                print "Total calls to ocrEdtCreate(): "+str(values[EDT_CREATES])
            if values[TEMPLATE_CREATES] > 0:
                print "Total calls to ocrEdtTemplateCreate(): "+str(values[TEMPLATE_CREATES])
            if values[EVENT_CREATES] > 0:
                print "Total calls to ocrEventCreate(): "+str(values[EVENT_CREATES])
            if values[EVENT_SATS] > 0:
                print "Total calls to ocrEventSatisfy(): "+str(values[EVENT_SATS])
            if values[EVENT_DESTROYS] > 0:
                print "Total calls to ocrEventDestroy(): "+str(values[EVENT_DESTROYS])
            if values[ADD_DEPS] > 0:
                print "Total calls to ocrAddDependence(): "+str(values[ADD_DEPS])
            if values[DB_CREATES] > 0:
                print "Total calls to ocrDbCreate(): "+str(values[DB_CREATES])
            if values[DB_RELEASES] > 0:
                print "Total calls to ocrDbRelease(): "+str(values[DB_RELEASES])
            if values[DB_DESTROYS] > 0:
                print "Total calls to ocrDbDestroy(): "+str(values[DB_DESTROYS])
            if values[AFF_GET_CUR] > 0:
                print "Total calls to ocrAffinityGetCurrent(): "+str(values[AFF_GET_CUR])
            if values[AFF_GET_AT] > 0:
                print "Total calls to ocrAffinityGetAt(): "+str(values[AFF_GET_AT])
            if values[AFF_COUNT] > 0:
                print "Total calls to ocrAffinityCount(): "+str(values[AFF_COUNT])
            if values[AFF_QUERY] > 0:
                print "Total calls to ocrAffinityQuery(): "+str(values[AFF_QUERY])
            if values[HINT_INITS] > 0:
                print "Total calls to ocrHintInit(): "+str(values[HINT_INITS])
            if values[HINT_SET_VALS] > 0:
                print "Total calls to ocrHintSetValue(): "+str(values[HINT_SET_VALS])
            if values[GUID_RANGE_CREATES] > 0:
                print "Total calls to ocrGuidRangeCreate(): "+str(values[GUID_RANGE_CREATES])
            print '\n\n\n'


def processConfig(config):
    global appParams
    global inputFiles
    global pathToBinary

    inputFiles = (config[0].replace(',','')).split()
    appParams = (config[1].replace(',','')).split()
    pathToBinary = config[2].split()

    inputFiles.pop(0)
    appParams.pop(0)
    temp = []
    #Possible check that first item provided in comma seperated config params is deliniated by a space.
    for i in appParams:
        temp.append(int(i))
    appParams = temp

    pathToBinary.pop(0)


#####################################################################
#                              MAIN                                 #
#####################################################################
def main():

    #will be populated per input file
    allCallCounts = []
    perEdtCallCounts = []
    allEdtData = []
    allInstCounts = []
    allTemplateGuids = []
    templGuidDict = {}
    apiFlags = {}

    if(len(sys.argv) < 2):
        print "ERROR: No config file provided\n"
        print "\t Usage: ./ocrSimulator.py <config_file>\n\n"

    configFP = open(sys.argv[1])
    config = configFP.readlines()
    processConfig(config)

    if len(inputFiles) < 7:
        print "ERROR: Insufficient number of input files. Atleast 7 required to calculate call count polynomials"
        sys.exit(0)

    if len(appParams) != len(inputFiles):
        print "ERROR: Mismatch in input file count and corresponding application params... "+str(len(inputFiles)-1)+" input files != "+str(len(appParams)-1)+" app params"
        sys.exit(0)

    print "\n\nConfig File Processed..."
    print "Input Files  : ", inputFiles
    print "Input Params : ", appParams, "\n"


    #Create data structures from each trace file
    #for i in range(1, len(sys.argv)):
    for i in range(0, len(inputFiles)):
        print "Processing file: ", inputFiles[i]
        log = []
        #logFP = open(sys.argv[i], 'r')
        logFP = open(inputFiles[i], 'r')
        log = logFP.readlines()

        sortedLog = sortAscendingTime(log)
        retVal = getCallsInEdt(sortedLog)

        edtCallDict = retVal[0]
        uniqFps = retVal[1]
        instCounts = retVal[2]
        templateGuid_FpMap = retVal[3]
        apiFlags = retVal[4]

        allEdtData.append(edtCallDict)
        allInstCounts.append(instCounts)
        allTemplateGuids.append(templateGuid_FpMap)

        templGuidDict[appParams[i]] = templateGuid_FpMap

        logFP.close()

    print "All files read... Begin populating data structures"

    #Sanity check to ensure there is no mismatch in
    if len(appParams) != len(allInstCounts):
        print "Number of trace files and number of app paramaters mismatch. See README for usage instructions" #TODO write a README
        sys.exit(0)

    #fpNames --- Dictionary --- Key: Function pointer | Value: Name of function associated
    fpNames = {}
    for fp in uniqFps:
        fpNames[fp] = getNameFromFuncPtr(fp)

    #maxCallTypes --- Dictionary --- Key: Function pointer | Value: greatest number of unique API calls observed per EDT
    maxCallTypes = {}
    for i in uniqFps:
        maxCallTypes[i] = 0

    for i in allEdtData[0]:
        nonZero = 0
        curFp = allEdtData[0][i].funcPtr
        for j in allEdtData[0][i].callCounts:
            if j != 0:
                nonZero += 1
        curNumCalls = len(set(allEdtData[0][i].calls))
        if nonZero > maxCallTypes[curFp]:
            maxCallTypes[curFp] = nonZero

    #allTemplateGuids: list of dictionaries - {Template Guid : FuncPtr}
    allTemplateGuids = templGuidDict

    #appToInstCounts --- Dictionary --- Key: App paramater | Value: Dictionary { Func Ptr. : Instance Counts }
    appToInstCounts = {}
    for i in range(len(appParams)):
        appToInstCounts[appParams[i]] = allInstCounts[i]

    #allInstCounts --- Dictionary --- Key: app param | Value: Dictionary { Key: funcPtr | Val: Number of times EDT associated with FP executed with current app param }
    allInstCounts = appToInstCounts

    #appToEdtDict --- Dictionary ---  Key: current app param | Value: List of all EDT objects that executed with that param configuration
    appToEdtDict = {}
    for i in range(len(appParams)):
        appToEdtDict[appParams[i]] = allEdtData[i]

    #allFpCallCounts --- Dictionary --- Key: Function PTR  Value: list of all call count lists for all param configurations
    allFpCallCounts = {}
    for i in uniqFps:
        allFpCallCounts[i] = []

    for i in range(len(allEdtData)):
        for j in allEdtData[i].itervalues():
            funcPtr = j.funcPtr
            callCounts = j.callCounts
            allFpCallCounts[funcPtr].append(callCounts)


    #constCallDict --- Dictionary --- Key: FP  Value:  list of (int) value corresponding to all ocr calls with constant number of across all param configs
    constCallDict = resolveConstantNumCalls(allFpCallCounts)

    #fpPolyDict --- Dictionary --- Key: FP Value 2d list [[polynomial type, [coeffs]], ... ]
    fpPolyDict = resolveApiCallPolynomials(appToEdtDict, constCallDict, uniqFps, allInstCounts, fpNames, allTemplateGuids)

    print "Data structures complete... Begin reconstructing OCR application (simApp.c)"

    #Aggragte all data to be written as skeleton application
    reconstructSkeleton(allEdtData, allFpCallCounts, uniqFps, constCallDict, fpPolyDict, fpNames, apiFlags, maxCallTypes)

    #Uncomment the below line to print out call count statistics for analysis purposes
    #printAnalysis(appToEdtDict, uniqFps, fpNames, allInstCounts)

    sys.exit(0)

if __name__ == "__main__":
    main()
