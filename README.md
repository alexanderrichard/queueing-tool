# queueing-tool
A tool for scheduling multiple parallelized jobs to a machine. Jobs are scheduled according to a user's priority which depends on the requested hardware resources.

################################################################################
# (A) INSTALLATION                                                             #
################################################################################

(1) run install.sh as root

(2) add the following line to /etc/sudoers (use visudo for editing)

    ALL ALL=(ALL) NOPASSWD: /usr/queue/_wake.sh

(3) adjust the values in /usr/queue/queue.config
   * threads [int]: the maximal number of threads used by the queue
   * memory [int]: the maximal amount of memory (in MB) used by the queue
   * gpus [int]: the number of CUDA capable gpus on the system (usually 0 or 1)
   * abortOnTimeLimit [true/false]: if true abort jobs at time limit
   * addUnkownUsers [true/false]: if true add a user when he first submits a job, else deny non-added users to submit jobs
   * regeneration-factor [float]: specify how fast priorities recover if a user does not have any job in the queue (close to 0: slow, close to 1: fast)
   * max-priority-class [int]: set highest priority class (see (C) for details)


################################################################################
# (B) USAGE                                                                    #
################################################################################

# (1) queue-scripts ############################################################

The queue uses bash scripts that are organized in blocks. A block is a part
of the script that forms an independent job and can be specified as follows:

    #block(name=[jobname], threads=[num-threads], memory=[max-memory], subtasks=[num-subtasks], gpu=[true/false], hours=[time-limit])

where
   * [name] is the name of the job (default: unknown-job)
   * [threads] is the number of threads to use (default: 1)
   * [memory] is the maximal amount of memory in mb for the job (default: 1024)
   * [subtasks] is the number of subtasks in the block, see below (default: 1)
   * [gpu] specifies if a GPU should be used (default: false)
   * [time-limit] is the maximal runtime of the job in hours (default: 1)
All numbers must be positive integers.

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

The first block has 10 subtasks that all process different data. The
second block is not started before all subtasks of the first block are
finished. Note that all block parameters that are not specified are set
to the default values.

In order to submit the above script, save it as exampleScript.sh and invoke

     qsub exampleScript.sh

# (2) job status ###############################################################

* 'r' (running): The requested resources have been allocated and the job runs.
* 'p' (pending): The requested resources have been allocated but the job did not yet start. If a job is in status 'p' for more than a few seconds it is probably stuck and should be deleted.
* 'w' (waiting): The requested resources could not yet be allocated and the job waits for execution.
* 'h' (hold):    The job has to wait for other jobs to be finished before it can start.

# (3) queue-commands ###########################################################

* qsub [-r] [SCRIPT] [param1] [param2] ...
  
   Submits the script with optional parameters to the queue. Parameters can be accessed with $1, $2, etc. If the option -r is set, the jobs are not queued but run directly in your current terminal.

* qstat [-v]

   Prints all jobs that are currently submitted. If option -v is set, output is verbose, i.e. requested resources per job are also displayed.

* qdel [job-id-1|jobname-1] [job-id-2|jobname-2] [job-id-3|jobname-3] ...

   Deletes all jobs with the given ids or names. Multiple jobs at once can also be deleted with qdel [from]-[to], e.g. qdel 8-12 to delete the jobs with IDs 8, 9, 10, 11, 12. You can only delete your own jobs. If there are problems with other user's jobs, log in as root, change the environment variable USER to the name of the user and invoke qdel with the appropriate job-id.

* qinfo

   Outputs some information about the current queue status.

* qjobinfo [job-id]

   Outputs detailed information for the given job.

* qreset

   Deletes the jobs of all users and resets the queue to its initial state. Needs root privileges.

* qadduser [username] [priority-factor]

   Adds the user [username] to the queue. The priority-factor determines an additional factor that may be used to boost specific users priorities (if greater than 1.0) or limit them (if smaller than 1.0), respectively. To change the priority-factor of an existing user, call qadduser with the respective user name and the new priority-factor. Needs root privileges.

* qdeluser [username]

   Deletes the user [username] from the queue. Needs root privileges


The queue-commands are located in /usr/queue.
For direct access, it is recommended to add the following line to your .bashrc:

    export PATH=/usr/queue:$PATH

You might as well put a .sh file containing this line in /etc/profiles.d, so the
path will be loaded for all users.


################################################################################
# (C) SCHEDULING                                                               #
################################################################################

The scheduling strategy is based on user priorities. Each time a job starts,
the owner's priority decreases.

Each job gets a cost defined as follows:

    cost = time-limit * max(relativeRequestedThreads, relativeRequestedMemory)

For example, if a job requests 10GB, 3 threads, and 12 hours and the machine
provides 30GB and 12 cores, the cost of the job is 12 * max(10/30, 3/12).
The priorities are decreased based on the cost of a job.

If a user does not submit a job for some time, his priorities regenerate
based on the regeneration-factor and eventually may reach the maximal priority
of 1.0 again.

If the job with highest priority does not fit due to its requested resources, a
job with lower priority may be started first to ensure efficient usage of the
machine.
To prevent a high priority job from waiting forever, each time another job
with lower priority is preferred, the high priority job increases its priority
class. If priority class [max-priority-class] is reached, no other jobs with
lower priority are started until there are enough resources for the high
priority job available.
