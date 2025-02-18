# queueing-tool

A tool for scheduling multiple parallelized jobs to a machine. Jobs are scheduled
according to a priority queue. The priority of a job is small if the amount of
requested resources/computation time is large and high if the amount of requested
resources/computation time is small, respectively. Priority of a job increases with
its waiting time.

################################################################################
# (A) INSTALLATION                                                             #
################################################################################

Open the file *qserver_deamon* and set the available resources
*DEVICE_IDS*, *THREADS*, and *MEMORY* to values that fit for your machine.
Then, install the tool.

(a) Automatic installation: run the script *install.sh* as root.

(b) Manual installation:

Copy the *queue* directory to a location of your choice. The queueing tool requires
to run the *qserver* tool that manages all submitted jobs and allocates the
requested resources. The server can either be started manually or automatically
as a daemon. For the latter case, copy the *qserver_daemon* script to */etc/init.d/*
and invoke

    sudo update-rc.d qserver_daemon defaults

to register it for startup during boot. Note that the path to the queue directory
and the available resources need to be specified in *qserver_daemon* at the point
marked with *TODO*. You can now also start/stop the qserver with

    /etc/init.d/qserver_daemon start/stop

The queue-commands are located the *queue* directory.
In order to submit jobs, it is required that the following line is added either
to your *.bashrc* (will make the queue tools accessible for your account) or to
an empty file in */etc/profiles.d/* (will make the queue tools accessible for all
accounts):

    export PATH=/your/queue/path/queue:$PATH

################################################################################
# (B) QUICKSTART                                                               #
################################################################################

Save the following as `test.sh`:

```bash
    # Simple script that counts up numbers forever:
    #block(name=test-job, threads=1, memory=1000, hours=24)
      counter=0
      while true; do
        echo "Job running: counter=$counter"
        counter=$((counter + 1))
        sleep 1
      done
```

Submit the script: 
```bash
qsub test.sh
```
Check the job status in the queue with `qstat`.  
Once the job starts, output logs are available in the folder `q.log/test-job.<JOB_ID>`.   
You can kill the job with `qdel <JOB_ID>`.
To stop the job, use:
```bash
qdel <JOB_ID>
```

################################################################################
# (C) USAGE                                                                    #
################################################################################

# (1) qserver ##################################################################

Either start the server automatically as described above or manually:

`qserver [--port PORT] [--gpus GPUS] [--threads THREADS] [--memory MEMORY] [--abort_on_time_limit]`

Options:

    --port PORT            port to listen on (default: 1234)
    --gpus GPUS            comma separated list of available gpu device ids
    --ignore_gpus GPUS     comma separated list of externally reserved gpu device ids
    --threads THREADS      number of available threads/cores
    --memory MEMORY        available main memory in mb
    --abort_on_time_limit  kill jobs if time limit is exceeded


# (2) queue-scripts ############################################################

The queue uses bash scripts that are organized in blocks. A block is a part
of the script that forms an independent job and can be specified as follows:

    #block(name=[jobname], threads=[num-threads], memory=[max-memory], subtasks=[num-subtasks], gpus=[num-gpus], hours=[time-limit])

where
   * [name] is the name of the job (default: unknown-job)
   * [threads] is the number of threads to use (default: 1)
   * [memory] is the maximal amount of memory in mb for the job (default: 1024)
   * [subtasks] is the number of subtasks in the block, see below (default: 1)
   * [gpus] is the number of requested GPUs (default: 0)
   * [hours] is the maximal runtime of the job in hours (default: 1)

Each block is scheduled [subtasks] times in parallel, which for instance allows
for easy data parallelism. The subtask of the actual job is specified with the
$SUBTASK_ID variable and the total number of subtasks is contained in
$N_SUBTASKS. Subtask IDs range from 1 to $N_SUBTASKS.

If more than one block is specified in one script, each block is considered to
be dependent on it predecessor, i.e. it is not started before all subtasks of
the preceding block have finished. Subtasks within a block can run in parallel.

Example: 
--------------------------------------------------------------------------------
    #block(name=block-1, threads=2, memory=2000, subtasks=10, hours=24)
      echo "process subtask $SUBTASK_ID of $N_SUBTASKS"
      ./processData data-dir/data.part-$SUBTASK_ID
      
    #block(name=block-2, threads=10, memory=10000, hours=2)
      ./doSomethingThatRequiresTheResultsFromBlock-1
      ./somethingElse
      echo "processed block-2 after all subtasks of block-1"
--------------------------------------------------------------------------------

The first block has 10 subtasks that all process different data, if possible in
parallel. The second block is not started before all subtasks of the first block are
finished. Note that all block parameters that are not specified are set
to the default values.

In order to submit the above script, save it as `your-queue-script.sh` and invoke

     qsub your-queue-script.sh [param1] [param2] [...]

Note that the tool will create a `q.log` folder in the directory you invoked `qsub` in.
In this folder, a file for each job is created, storing some meta information about the
job as well as everything written to stdout and stderr.

# (2) job status ###############################################################

* 'r' (running): The requested resources have been allocated and the job runs.
* 'w' (waiting): The requested resources could not yet be allocated and the job waits for execution.
* 'h' (hold):    The job has to wait for other jobs to be finished before it can start.

# (3) queue-commands ###########################################################

**qsub** *(submit a job to the queue)*

`qsub [options...] script [script params]`

Options:

    script
        Script (plus its parameters) to be submitted. Script
        parameters must not start with '-'. If they do so,
        pass them with escaped quotation marks and an escaped
        leading space like this: \"\ --foo\".

    -l, --local
        Execute the script locally
    -b BLOCK, --block BLOCK
        Only submit/execute the specified block
    -s BLOCK SUBTASK_ID, --subtask BLOCK SUBTASK_ID
        Arguments for this option are a block name and a
        subtask id. Only submit/execute the subtask id of the
        specified block.
    -f FROM_BLOCK, --from_block FROM_BLOCK
        Submit/execute the specified block and all succeeding blocks
    --server_ip SERVER_IP
        Ip address of the server (default: localhost)
    --server_port SERVER_PORT
        Port of the server (default: 1234)

**qstat [-v]**

Prints all jobs that are currently submitted. If option -v is set, output is verbose, i.e. requested resources per job are also displayed.

**qdel** *(delete jobs)*
`qdel [options...] job_specifier(s)`

    jobs
        Jobs to be deleted. Is a space separated list of job names, user names, or jod ids.
        For ids (neither -n nor -u option is set), jobs ranges separated by a '-' are also possible.
    --server_ip SERVER_IP
        ip address of the server (default: localhost)
    --server_port SERVER_PORT
        port of the server (default: 1234)
    -n
        Delete all jobs of the given names. Asterisks can be used as wildcards.
    -u
        Delete all jobs of the given users. Asterisks can be used as wildcards.

Example:

`qdel 2-5` deletes the jobs with id 2,3,4,5

`qdel 1,3,5` deletes the jobs with id 1,3,5

`qdel -n job_A` deletes all jobs with name *job_A*

`qdel -u user_x` deletes all jobs of user *user_x*

Jobs can only be deleted by their owners or by root.

**qinfo**

Outputs some information about the current queue status.
