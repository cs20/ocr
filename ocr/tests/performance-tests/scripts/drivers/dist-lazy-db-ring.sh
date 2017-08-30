#
# OCR Object micro-benchmarks X86 Single Node
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
    else
        echo "error: OCRRUN_OPT_TPL_NODEFILE not set or no 'nodelist' file found"
        exit 1
    fi
fi

export LOGDIR=`mktemp -d logs_dist-lazy-db-ring.XXXXX`
export NAME_EXP="lazyDbRing"
export OCR_TYPE=x86-mpi

export NODE_SCALING="2 4 8"
export CORE_SCALING="2"

export NB_ELEMS_LIST="1 1000 100000"
for elems in `echo ${NB_ELEMS_LIST}`; do
    export C_DEFINES="-DNB_ELEMS=${elems}"

    export NAME=dbLazyRingOn
    export EXT="-LazyOn-elems${elems}"
    export REPORT_FILENAME_EXT="-${OCR_TYPE}-${EXT}"
    runProg

    export NAME=dbLazyRingOff
    export EXT="-LazyOff-elems${elems}"
    export REPORT_FILENAME_EXT="-${OCR_TYPE}-${EXT}"
    runProg
done

