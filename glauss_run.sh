#!/usr/bin/env bash
RUN="./build/glauss_client"
DEBUG="gdb -ex=r --args"
if [ ! -f $PROGRAM ]; then
    PROGRAM="glauss_client"
fi

if [ "${@: -1}" = "--debug" ] || [ "${@: -1}" = "-d" ]; then
    RUN=$DEBUG" "$RUN
fi

$RUN $@
