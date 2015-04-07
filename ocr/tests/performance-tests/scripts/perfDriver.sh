#!/bin/bash

#
# Main driver to compile and run OCR performance tests
#
# For help invoke: ./perfDriver.sh -help
# or check the -help option below
#

SCRIPT_NAME=${0##*/}

if [[ -z "${SCRIPT_ROOT}" ]]; then
    echo "error: ${SCRIPT_NAME} environment SCRIPT_ROOT is not defined"
    exit 1
fi

#
# Environment variables default values
#

# Number of runs to perform
export NB_RUN=${NB_RUN-"3"}

# Core scaling sweep
export CORE_SCALING=${CORE_SCALING-"2 4 8 16"}

# Number of nodes to use
export OCR_NUM_NODES=${OCR_NUM_NODES-"1"}

# Base OCR cfg file to use for core scaling
# The runner script replaces "1w" by nb of required cores
export OCR_CONFIG_BASE=${OCR_CONFIG_BASE-mach-hc-1w-bind-mallocproxy.cfg}

# Default runlog and report naming
export LOGDIR=${LOGDIR-$PWD}
export RUNLOG_FILENAME_BASE=${RUNLOG_FILENAME_BASE-"runlog"}
export REPORT_FILENAME_BASE=${REPORT_FILENAME_BASE-"report"}

#
# Option Parsing and Checking
#

SWEEP_OPT="no"
SWEEPFILE_OPT="no"
SWEEPFILE_ARG=""
DEFSFILE_OPT="no"
DEFSFILE_ARG=""
PROGRAMS=""

SWEEP_FOLDER="configSweep"
TEST_FOLDER="ocr"

if [[ ! -d ${SWEEP_FOLDER} ]]; then
    echo "error: ${SCRIPT_NAME} cannot find sweep config folder ${SWEEP_FOLDER}"
    exit 1
fi

if [[ ! -d ${TEST_FOLDER} ]]; then
    echo "error: ${SCRIPT_NAME} cannot find test folder ${TEST_FOLDER}"
    exit 1
fi

while [[ $# -gt 0 ]]; do
    if [[ "$1" = "-sweep" ]]; then
        shift
        SWEEP_OPT="yes"
    elif [[ "$1" = "-sweepfile" && $# -ge 2 ]]; then
        shift
        SWEEPFILE_OPT="yes"
        SWEEPFILE_ARG=("$@")
        if [[ ! -f ${SWEEPFILE_ARG} ]]; then
            echo "error: ${SCRIPT_NAME} cannot find sweepfile ${SWEEPFILE_ARG}"
            exit 1
        fi
        shift
    elif [[ "$1" = "-defaultfile" && $# -ge 2 ]]; then
        shift
        DEFSFILE_OPT="yes"
        DEFSFILE_ARG=("$@")
        if [[ ! -f ${DEFSFILE_ARG} ]]; then
            echo "error: ${SCRIPT_NAME} cannot find defaultfile ${DEFSFILE_ARG}"
            exit 1
        fi
        shift
    elif [[ "$1" = "-help" ]]; then
        echo "usage: ${SCRIPT_NAME} [-defaultfile file] [-sweep] [-sweepfile file] [programs...]"
        echo "       -sweep             : lookup for a sweep file matching the progname under configSweep/"
        echo "       -sweepfile file    : Use the specified sweep file for all the programs"
        echo "       -defaultfile file  : parse file to find the test's define defaults"
        echo ""
        echo "Defines resolution order:"
        echo "       sweep, sweepfile, defaultfile, defaults.mk"
        exit 0
    else
        # stacking remaining program arguments
        PROGRAMS="${PROGRAMS} $1"
        shift
    fi
done

if [[ -z "$PROGRAMS" ]]; then
    #No programs specified, scan the ocr/ folder
    PROGRAMS=`find ${TEST_FOLDER} -name "*.c"`
fi

function matchDefaultSweepFile() {
    local __resultvar=$1
    local prog=$2
    local isFound=""
    local sweepfile=${SWEEP_FOLDER}/${prog}.sweep
    if [[ -f ${sweepfile} ]]; then
        echo ">>> ${prog}: Use defines from sweep file ${sweepfile}"
        isFound=${sweepfile}
    fi
    eval $__resultvar="$isFound"
}

function matchSweepFile() {
    local __resultvar=$1
    local prog=$2
    isFound=""
    if [[ -f ${SWEEPFILE_ARG} ]]; then
        echo ">>> ${prog}: Use defines from sweep file ${SWEEPFILE_ARG}"
        isFound="${SWEEPFILE_ARG}"
    fi
    eval $__resultvar="$isFound"
}

function matchDefaultFile() {
    local __resultvar=$1
    local prog=$2
    # Switch to a map when bash 4.x
    isFound=""
    while read line; do
        array=($line)
        testFile=${array[0]}
        defines=${array[@]:1}
        if [ "${prog}.c" = "$testFile" ]; then
            echo ">>> ${prog}: Use defines from default file ${DEFSFILE_ARG}"
            isFound="$defines"
            break
        fi
    done < ${DEFSFILE_ARG}
    eval $__resultvar="'$isFound'"
}

function run() {
    for prog in `echo "$PROGRAMS"`; do
        local found=""
        local runnerArgs=""
        # Try to match a sweep file for the program
        if [ "${SWEEP_OPT}" = "yes" ]; then
            matchDefaultSweepFile found $prog
            if [ -n "$found" ]; then
                runnerArgs="-sweepfile $found"
            fi
        fi
        # Try to match a general sweep file
        if [ "$found" = "" -a "${SWEEPFILE_OPT}" = "yes" ]; then
            matchSweepFile found $prog
            if [ -n "$found" ]; then
                runnerArgs="-sweepfile $found"
            fi
        fi
        # Try to match an entry for program in a default define file
        if [ "$found" = "" -a "${DEFSFILE_OPT}" = "yes" ]; then
            matchDefaultFile found $prog
            # to be picked up by the runner script
            if [ -n "$found" ]; then
                export CUSTOM_BOUNDS="$found"
            fi
        fi
        # else rely on default values from defaults.mk
        if [ -z "$found" ]; then
            echo ">>> ${prog}: Use defines from defaults.mk"
        fi

        runlogFilename=${RUNLOG_FILENAME_BASE}-${prog}
        reportFilename=${REPORT_FILENAME_BASE}-${prog}
        ${SCRIPT_ROOT}/runner.sh -nbrun ${NB_RUN} -logdir ${LOGDIR} -runlog ${runlogFilename} -report ${reportFilename} ${runnerArgs} ${prog}
    done
}

run