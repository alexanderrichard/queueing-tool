#!/bin/bash

LOGFILE=$1
MAXTHREADS=$2
MAXMEMORY=$3
TIMELIMIT=$4
GPUID=$5
ABORT_ON_TIME_LIMIT=$6
SCRIPT=$7

mkdir -p q.log
LOGFILE="q.log/${LOGFILE}"

echo "---------------- start ----------------" > $LOGFILE
echo "$(date)" >> $LOGFILE
echo "max threads: $MAXTHREADS" >> $LOGFILE
echo "max memory: $MAXMEMORY MB" >> $LOGFILE
echo "time limit: $TIMELIMIT hours" >> $LOGFILE
if [ $GPUID -ge 0 ]; then
    echo "gpu device id: $GPUID" >> $LOGFILE
else
    echo "use gpu: false" >> $LOGFILE
fi
echo "$SCRIPT" >> $LOGFILE
echo "---------------------------------------" >> $LOGFILE
echo "" >> $LOGFILE

KB_MEM=`python -c "print $MAXMEMORY * 1024"`
# ignore memory limit when GPU is requested (cuda's unified addressing messes up virtual memory statistics)
if [ $GPUID -lt 0 ]; then
    ulimit -v $KB_MEM
fi

# set number of threads for open MP
export OMP_NUM_THREADS=$MAXTHREADS

# use the specified GPU
export CUDA_VISIBLE_DEVICES=$GPUID

# execute the job
T_START=$(date +"%s")
if [ "$ABORT_ON_TIME_LIMIT" == "true" ]; then
    eval timeout --signal=KILL ${TIMELIMIT}h bash ${SCRIPT} >> $LOGFILE 2>&1
else
    bash ${SCRIPT} >> $LOGFILE 2>&1
fi

T_END=$(date +"%s")

T_DIFF=$(($T_END - $T_START))
T_SECS=$(( $T_DIFF % 60 ))
T_MINS=$(( ($T_DIFF / 60) % 60 ))
T_HOURS=$(( $T_DIFF / 3600 ))

echo "" >> $LOGFILE
echo "----------------- end -----------------" >> $LOGFILE
echo "$(date)" >> $LOGFILE
printf "elapsed time: %03d:%02d:%02d\n" "$T_HOURS" "$T_MINS" "$T_SECS" >> $LOGFILE
echo "---------------------------------------" >> $LOGFILE
