#!/bin/bash

function killtree() {
    local parent=$1 child
    for child in $(ps -A -o ppid= -o pid= | awk "\$1==$parent {print \$2}"); do
        killtree $child
    done
    kill -9 $parent
}

PIDS=$@

for PID in $PIDS
do
    killtree ${PID}
done
