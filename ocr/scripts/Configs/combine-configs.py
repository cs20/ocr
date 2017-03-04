#!/usr/bin/env python

import fileinput
import argparse
import os
import sys
import re
import ConfigParser

# This script cleans up a configuration file. It does this by
# removing all earlier entries of each section, whenever a section is duplicated.

parser = argparse.ArgumentParser(description='Combine several config files into a \
            single one by removing earlier entries of duplicated sections in an OCR \
            config file.')
parser.add_argument('--file', dest='output', required=True,
                   help='config file to append to')
parser.add_argument('files', metavar='<other config files>', type=str, nargs='+',
                   help='config files to append')

# Parse args

args = parser.parse_args()
filename = args.output
inputs = args.files

# Get rid of whitespace to keep ConfigParser happy

for line in fileinput.input(filename, inplace=True):
    print line.strip()

for f in inputs:
    for line in fileinput.input(f, inplace=True):
        print line.strip()

objs = ['PolicyDomain', 'Worker', 'Scheduler', 'Allocator']
counts = {}

def ParseId(rangeval):
    parsed = re.split('-', rangeval)
    if len(parsed) == 2:
        return int(parsed[1])-int(parsed[0])+1
    else:
        return 1

def UpdatePD(config):
    sections = config.sections()
    for sec in sections:
        # Count the # of worker, scheduler, allocator and PD
        for ob in objs:
            if re.search(ob+'Inst[0-9]+', sec) != None:
                val = config.get(sec, 'id')
                if(ob in counts.keys()):
                    counts[ob] = counts[ob] + ParseId(val)
                else:
                    counts[ob] = ParseId(val)
                if ob == objs[0]:
                    secPD = sec
                    print secPD

    # Now update the policy domain
    if (counts[objs[0]] == 1):
        for j in range(1,4):
            print config.get(secPD, objs[j])
            if(counts[objs[j]] != 1):
                config.set(secPD, objs[j], "0-"+str(counts[objs[j]]-1))
            else:
                config.set(secPD, objs[j], "0")

# Do the following in a loop
# For each file f provided for appending:
#  Remove all sections in config file that are duplicated in f
#  Append f to config file

config = ConfigParser.RawConfigParser()
config.read(filename)

for f in inputs:
    appendfile = ConfigParser.RawConfigParser()
    appendfile.read(f)
    sections = appendfile.sections()
    for sec in sections:
        if config.has_section(sec):
            config.remove_section(sec)
        config.add_section(sec)
        for opt in appendfile.options(sec):
            config.set(sec, opt, appendfile.get(sec,opt))

# Now update any fields in the policy domain that need to be updated
# due to the changed sections

UpdatePD(config)

with open(filename, 'wb') as cfgfile:
    config.write(cfgfile)
