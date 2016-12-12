#!/usr/bin/python2.7

import datetime

class Resources(object):
    
    def __init__(self, gpus, threads, memory):
        self.gpus = gpus
        self.threads = threads
        self.memory = memory

    def __add__(self, other):
        return Resources(self.gpus + other.gpus, self.threads + other.threads, self.memory + other.memory)


class SchedulerJob(object):
    
    def __init__(self, job, job_id): # job is a Job object from the server
        self.resources = Resources(job.n_gpus, job.threads, job.memory)
        self.hours = job.hours # estimated duration of the job
        self.time = max(1, (datetime.datetime.now() - job.time).total_seconds()) # waiting time/running time of the job
        self.priority = job.priority
        self.job_id = job_id

    def fits(self, resources):
        return (self.resources.gpus <= resources.gpus) and (self.resources.threads <= resources.threads) and (self.resources.memory <= resources.memory)


class Scheduler(object):

    def __init__(self, free_gpus, free_threads, free_memory):
        self.free_resources = Resources(free_gpus, free_threads, free_memory)

    def update_resources(self, free_gpus, free_threads, free_memory):
        self.free_resources = Resources(free_gpus, free_threads, free_memory)

    def waiting_time(self, job, running_jobs):
        # sort jobs by remaining runtime (in seconds)
        runtimes = [ max(1, j.hours * 3600 - j.time) for j in running_jobs ]
        runtimes, sorted_jobs = zip(*sorted(zip(runtimes, running_jobs)))
        # determine waiting time until job can be submitted
        tmp_resources = self.free_resources
        for idx in range(len(sorted_jobs)):
            tmp_resources = tmp_resources + sorted_jobs[idx].resources
            if job.fits(tmp_resources):
                return runtimes[idx]
        return 0

    def schedule(self, jobs, waiting_ids, running_ids):
        try:
            waiting_jobs = [ SchedulerJob(jobs[job_id], job_id) for job_id in waiting_ids ]
            # sort waiting jobs by priority (first key) and job_id (second key in case of equal priority)
            priorities = [ job.priority for job in waiting_jobs ]
            job_ids = [ job.job_id for job in waiting_jobs ]
            priorities, job_ids, waiting_jobs = zip(*sorted(zip(priorities, job_ids, waiting_jobs), key=lambda sl: (-sl[0], sl[1])))
            # determine next job to schedule
            if waiting_jobs[0].fits(self.free_resources):
                return waiting_jobs[0].job_id
            else:
                # if job does not fit compute waiting time and possibly submit another job that runs in the meantime
                running_jobs = [ SchedulerJob(jobs[job_id], job_id) for job_id in running_ids ]
                time_slot = self.waiting_time(waiting_jobs[0], running_jobs)
                for job in waiting_jobs[1:]:
                    if job.fits(self.free_resources) and job.hours * 3600 <= time_slot:
                        return job.job_id
        except:
            return None
