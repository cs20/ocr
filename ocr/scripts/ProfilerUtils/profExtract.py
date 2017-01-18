from pprint import pprint

import sys
import os
import time
import numpy as np
import itertools
import subprocess

START_STRING = "#### TOTAL ####"
LABEL_STRING = "%Time"
END_STRING = "Call-graph profile"
MIN_PROF = 0
DET_PROF = 1

class PROF_RECORD:

    def __init__(self, time, cum_ms, self_ms, num_calls, avg_cum_ms, std_dev, call_name):
        self.time = time
        self.cum_ms = cum_ms
        self.self_ms = self_ms
        self.num_calls = num_calls
        self.avg_cum_ms = avg_cum_ms
        self.std_dev = std_dev
        self.call_name = call_name


def associateNames(prof_recs, api, pd, task, worker, guid_prov):

    api_w_name = []
    pd_w_name = []
    task_w_name = []
    worker_w_name = []
    gp_w_name = []

    detailed_prof = True
    if pd == None:
        detailed_prof = False

    for i in prof_recs:
        if i.self_ms in api:
            api_w_name.append([i.call_name, i.self_ms])
        if detailed_prof and i.self_ms in pd:
            pd_w_name.append([i.call_name, i.self_ms])
        if detailed_prof and i.self_ms in task:
            task_w_name.append([i.call_name, i.self_ms])
        if detailed_prof and i.self_ms in worker:
            worker_w_name.append([i.call_name, i.self_ms])
        if detailed_prof and i.self_ms in guid_prov:
            gp_w_name.append([i.call_name, i.self_ms])

    return [api_w_name, pd_w_name, task_w_name, worker_w_name, gp_w_name]


def extractProfileData(log):

    start_found = False
    prof_found = False
    profile_records = []

    for line in log:

        #Begin searching for profile labels.
        if start_found:

            #Explicit check of an empty line, marking end of profile info
            if line == '\n':
                break

            #Once profile labels are found, begin reading numbers
            if prof_found:

                rec = line.split()

                time_perc = float(rec[0])
                cum_ms =    float(rec[1])
                self_ms =   float(rec[2])
                num_calls = int(rec[3])
                avg_cum =   float(rec[4])
                std_dev =   float(rec[5])
                call_name = rec[6]

                profile_records.append(PROF_RECORD(time_perc, cum_ms, self_ms, num_calls, avg_cum, std_dev, call_name))


            if LABEL_STRING in line:
                prof_found = True

        if START_STRING in line:
            start_found = True

    return profile_records

def processMinimalData(prof_recs):
    user = []
    api = []
    other = []

    for rec in prof_recs:

        if   'userCode'    in  rec.call_name:
            user.append(rec.self_ms)
        elif 'api_'        in  rec.call_name:
            api.append(rec.self_ms)
        elif 'EVENT_OTHER' in  rec.call_name:
            other.append(rec.self_ms)
    if len(api) >= 3:
        top_api = sorted(api)[-3:]
    else:
        top_api = sorted(api)[-(len(api)):]

    ret = associateNames(prof_recs, top_api, None, None, None)

    ret.append(api)
    ret.append(user)
    ret.append(other)

    return ret


def processDetailedData(prof_recs):
    user = []
    api = []
    other = []
    pd = []
    task = []
    worker = []
    gp = []

    for rec in prof_recs:

        if   'userCode'    in  rec.call_name:
            user.append(rec.self_ms)
        elif 'api_'        in  rec.call_name:
            api.append(rec.self_ms)
        elif 'EVENT_OTHER' in  rec.call_name:
            other.append(rec.self_ms)
        elif 'pd_'         in rec.call_name:
            pd.append(rec.self_ms)
        elif 'ta_'         in rec.call_name:
            task.append(rec.self_ms)
        elif 'wo_'         in rec.call_name:
            worker.append(rec.self_ms)
        elif 'gp_'         in rec.call_name:
            gp.append(rec.self_ms)

    if len(api)  >= 3:
        top_api = sorted(api)[-3:]
    else:
        top_api = sorted(api)[-(len(api)):]

    if len(pd)  >= 3:
        top_pd = sorted(pd)[-3:]
    else:
        top_pd = sorted(pd)[-(len(pd)):]

    if len(task)  >= 3:
        top_task = sorted(task)[-3:]
    else:
        top_task = sorted(task)[-(len(task)):]

    if len(worker)  >= 3:
        top_worker = sorted(worker)[-3:]
    else:
        top_worker = sorted(worker)[-(len(worker)):]

    if len(gp)  >= 3:
        top_guid_prov = sorted(gp)[-3:]
    else:
        top_guid_prov = sorted(gp)[-(len(gp)):]

    ret = associateNames(prof_recs, top_api, top_pd, top_task, top_worker, top_guid_prov)

    ret.append(api)
    ret.append(user)
    ret.append(other)
    ret.append(pd)
    ret.append(task)
    ret.append(worker)
    ret.append(gp)

    return ret

def writeCSV(data, flag, path):
    top_api       = data[0]
    top_pd        = data[1]
    top_task      = data[2]
    top_worker    = data[3]
    top_guid_prov = data[4]
    total_api     = data[5]
    total_user    = data[6]
    total_other   = data[7]

    if flag == MIN_PROF:
        f = open(path+'/min_prof_breakdown.csv', 'w')
        f.write('user_code,sum_api,sum_other,top'+str(len(top_api))+'_api,')

        for i in range(len(top_api)-1):
            f.write(',')

        f.write('times')

        f.write('\n')

        f.write(str(sum(total_user))+','+str(sum(total_api))+','+str(sum(total_other))+',')


        for i in range(len(top_api)):
            f.write(str(top_api[i][0])+',')

        for i in range(len(top_api)):
            f.write(str(top_api[i][1])+',')

    if flag == DET_PROF:
        f = open(path+'/detailed_prof_breakdown.csv', 'w')
        total_pd = data[8]
        total_task = data[9]
        total_worker = data[10]
        total_guid_prov = data[11]
        f.write('user_code,sum_api,sum_pd,sum_task,sum_worker,sum_gp,sum_other,')

        f.write('top'+str(len(top_api))+'_api,')
        for i in range(len(top_api)-1):
            f.write(',')
        f.write('times')
        for i in range(len(top_api)):
            f.write(',')

        f.write('top'+str(len(top_pd))+'_pd,')
        for i in range(len(top_pd)-1):
            f.write(',')
        f.write('times')
        for i in range(len(top_pd)):
            f.write(',')

        f.write('top'+str(len(top_task))+'_task,')
        for i in range(len(top_task)-1):
            f.write(',')
        f.write('times')
        for i in range(len(top_task)):
            f.write(',')

        f.write('top'+str(len(top_worker))+'_worker,')
        for i in range(len(top_worker)-1):
            f.write(',')
        f.write('times')
        for i in range(len(top_worker)):
            f.write(',')

        f.write('top'+str(len(top_guid_prov))+'_gp,')
        for i in range(len(top_guid_prov)-1):
            f.write(',')
        f.write('times')
        for i in range(len(top_guid_prov)):
            f.write(',')


        f.write('\n')
        f.write(str(sum(total_user))+','+str(sum(total_api))+','+str(sum(total_pd))+','+str(sum(total_task))+','+str(sum(total_worker))+','+str(sum(total_guid_prov))+','+str(sum(total_other))+',')

        for i in range(len(top_api)):
            f.write(str(top_api[i][0])+',')
        for i in range(len(top_api)):
            f.write(str(top_api[i][1])+',')

        for i in range(len(top_pd)):
            f.write(str(top_pd[i][0])+',')
        for i in range(len(top_pd)):
            f.write(str(top_pd[i][1])+',')

        for i in range(len(top_task)):
            f.write(str(top_task[i][0])+',')
        for i in range(len(top_task)):
            f.write(str(top_task[i][1])+',')

        for i in range(len(top_worker)):
            f.write(str(top_worker[i][0])+',')
        for i in range(len(top_worker)):
            f.write(str(top_worker[i][1])+',')

        for i in range(len(top_guid_prov)):
            f.write(str(top_guid_prov[i][0])+',')
        for i in range(len(top_guid_prov)):
            f.write(str(top_guid_prov[i][1])+',')


    f.close()

def main():

    DETAILED_PROF = False
    MINIMAL_PROF  = False

    #=========== Open file, sanity check, setup file handle ============#
    if len(sys.argv) == 2:
        path = sys.argv[1]
        path_cp = path
        if "detailedProf" in path:
            DETAILED_PROF = True
        elif "minimalProf" in path:
            MINIMAL_PROF = True
        else:
            print "No profiling information found in: ", path
            sys.exit(0)

        path += '/all.prof'

    else:
        print "ERROR - Incorrect usage - TODO"
        sys.exit(0)

    try:
        fp = open(path, 'r')
    except:
        print "Unable to open file at:  ", path
        sys.exit(0)
    #===================================================================#

    lines = fp.readlines()

    if MINIMAL_PROF:
        file_data = extractProfileData(lines)
        to_write = processMinimalData(file_data)
        writeCSV(to_write, MIN_PROF, path_cp)
    elif DETAILED_PROF:
        file_data = extractProfileData(lines)
        to_write = processDetailedData(file_data)
        writeCSV(to_write, DET_PROF, path_cp)


if __name__ == "__main__":
    main()
