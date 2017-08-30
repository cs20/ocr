#!/bin/bash

mkdir -p ${OCR_BUILD_ROOT}/$1
cd ${OCR_BUILD_ROOT}/$1

if [[ "$1" =~ "tg" ]]; then
    # Force sequential build for TG to work around "clang-3.9 (deleted)" error
    make -j1 all install
else
    make all install
fi

RETURN_CODE=$?

if [ $RETURN_CODE -eq 0 ]; then
    echo "**** Build SUCCESS ****"
else
    echo "**** Build FAILURE ****"
fi

exit $RETURN_CODE
