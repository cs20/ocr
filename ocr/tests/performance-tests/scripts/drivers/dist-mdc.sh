#
# DESC
#

if [[ -z "$SCRIPT_ROOT" ]]; then
    echo "SCRIPT_ROOT environment variable is not defined"
    exit 1
fi


unset OCR_CONFIG

. ${SCRIPT_ROOT}/drivers/utils.sh


if [[ -z "${OCRRUN_OPT_TPL_NODEFILE}" ]]; then
	if [[ -f "${PWD}/nodelist" ]]; then
		export OCRRUN_OPT_TPL_NODEFILE="$PWD/nodelist"
	fi
fi

export NB_RUN=10

export LOGDIR=`mktemp -d logs_dist-mdc.XXXXX`
export NAME_EXP="MDC"
export OCR_TYPE=x86-mpi
export NOCLEAN_OPT="yes"
#
# Experiment 0:
#   - 2-2 workers on X86-mpi
#
export CORE_SCALING="2"
export NODE_SCALING="2 4 8 16 32 64 128"

#
# SAT
#
export CFLAGS="-DSYNC_HEAD -DENABLE_EXTENSION_LABELING -DSAT_PRINT -DTEST_NBCONTRIBSPD=${TEST_NBCONTRIBSPD}"
export CFGARG_GUID="LABELED";

# export EXT="sat-baseline-contrib${TEST_NBCONTRIBSPD}"
# export NO_DEBUG=yes; unset OCR_ASAN
# export OCR_INSTALL=~/ocr_install_mpiicc_redevt-nodebug

# export NAME=event3StickyMdc
# export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
# runProg

export EXT="sat-mdc-contrib${TEST_NBCONTRIBSPD}"
export NO_DEBUG=yes;
unset OCR_ASAN
export OCR_INSTALL=~/ocr_install_mpiicc_redevt-mdc-forge-nodebug

export NAME=event3StickyMdc
export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
echo "before runProg $PWD"
runProg

#
# SAT SENDER
#
export CFLAGS="-DSYNC_HEAD -DENABLE_EXTENSION_LABELING -DSATSENDER_PRINT -DTEST_NBCONTRIBSPD=${TEST_NBCONTRIBSPD}"
export CFGARG_GUID="LABELED";

# export EXT="satsender-baseline-contrib${TEST_NBCONTRIBSPD}"
# export NO_DEBUG=yes; unset OCR_ASAN
# export OCR_INSTALL=~/ocr_install_mpiicc_redevt-nodebug

# export NAME=event3StickyMdc
# export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
# runProg

export EXT="satsender-mdc-contrib${TEST_NBCONTRIBSPD}"
export NO_DEBUG=yes;
unset OCR_ASAN
export OCR_INSTALL=~/ocr_install_mpiicc_redevt-mdc-forge-nodebug

export NAME=event3StickyMdc
export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
echo "before runProg $PWD"
runProg

#
# ADD DEP
#
export CFLAGS="-DSYNC_HEAD -DENABLE_EXTENSION_LABELING -DDEP_PRINT -DTEST_NBCONTRIBSPD=${TEST_NBCONTRIBSPD}"

# export EXT="dep-baseline-contrib${TEST_NBCONTRIBSPD}"
# export NO_DEBUG=yes; unset OCR_ASAN
# export OCR_INSTALL=~/ocr_install_mpiicc_redevt-nodebug

# export NAME=event3StickyMdc
# export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
# runProg

export EXT="dep-mdc-contrib${TEST_NBCONTRIBSPD}"
export NO_DEBUG=yes;
unset OCR_ASAN
export OCR_INSTALL=~/ocr_install_mpiicc_redevt-mdc-forge-nodebug

export NAME=event3StickyMdc
export REPORT_FILENAME_EXT="-${OCR_TYPE}${EXT}"
runProg

exit 1
