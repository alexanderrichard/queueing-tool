#!/usr/bin/python3

import datetime, random, re
from typing import List, Dict, Tuple

class GPUTreeNode(object):
    def __init__(self, cuda_id=None, left=None, right=None):
        self.cuda_id = cuda_id
        self.left = left
        self.right = right
        self._free = 1 if cuda_id is not None else None

    @property
    def free(self):
        if self._free is not None:
            return self._free
        else:
            ret_val = 0
            ret_val += self.left.free if self.left is not None else 0
            ret_val += self.right.free if self.right is not None else 0
            return ret_val

    @free.setter
    def free(self, free: bool):
        if self._free is not None:
            self._free = int(free)

    def get_continuous_free(self, gpu_count):
        gpus = self.get_gpus()
        count = 0
        for i in range(max(1,1+len(gpus) - gpu_count)):
            free = sum([g.free for g in gpus[i:gpu_count + i]])
            if free == gpu_count:
                count = free
        return count

    def total(self):
        total, used, l_used, r_used = 0,0,0,0
        if self.cuda_id is not None:
            total += 1
            used = int(not bool(self.free))
        else:
            l, l_used, _, __ = self.left.total() if self.left is not None else 0
            r, r_used, _, __ = self.right.total() if self.right is not None else 0
            total += l + r
            used = l_used + r_used
        return total, used, l_used, r_used

    def get_free_gpus(self, amount, traverse_dir=True, continuous=True):
        gpus = []
        first_trav = self.left if traverse_dir else self.right
        second_trav = self.right if traverse_dir else self.left
        gpu_max_amount, _, _, _ = self.total()
        amount = gpu_max_amount if amount >= gpu_max_amount else amount
        cont_free = self.get_continuous_free(amount) if continuous else amount
        if cont_free >= amount:
            if self.cuda_id is None:
                fir_ids = first_trav.get_free_gpus(amount, traverse_dir, continuous)
                if len(fir_ids) >= amount:
                    return fir_ids
                sec_ids = second_trav.get_free_gpus(amount - len(fir_ids), traverse_dir, continuous)
                if len(sec_ids) >= amount:
                    return sec_ids
                gpus = fir_ids + sec_ids if traverse_dir else sec_ids + fir_ids
            elif self.free:
                self.free = False
                gpus.append(self)
        elif self.free and self.cuda_id is not None:
            self.free = False
            gpus.append(self)
        return gpus

    def get_gpus(self):
        gpus = []
        if self.cuda_id is None:
            l_gpus = self.left.get_gpus()
            r_gpus = self.right.get_gpus()
            gpus = l_gpus + r_gpus
        else:
            gpus.append(self)
        return gpus

    def __str__(self):
        return f"GPU {self.cuda_id} {'free' if self.free > 0 else 'occupied'} at {id(self)}"


class GPUTree(object):

    def __init__(self):
        self._head: GPUTreeNode = None

    def build_tree(self, cuda_ids: list):  # list of tuples. first val cuda id, second val: state (occupied/free)
        cuda_ids.sort()
        bottom_leaves = list()
        for c_id in cuda_ids:
            node = GPUTreeNode(c_id[0])
            node.free = c_id[1]
            bottom_leaves.append(node)
        roots = list()
        while len(roots) != 1:
            roots = list()
            while len(bottom_leaves) > 0:
                left = bottom_leaves.pop(0)
                right = bottom_leaves.pop(0) if bottom_leaves else None
                roots.append(GPUTreeNode(left=left, right=right))
            bottom_leaves = roots
        self._head = roots.pop(0)

    def reserveGPUs(self, amount):
        total_gpus, total_used, left_used, right_used = self._head.total()  # used means at least partly used, doesn't mean its fully used.
        direction = True
        direction_needed = (left_used > 0) ^ (right_used > 0)
        if direction_needed:
            direction = left_used > 0 and not right_used > 0  # Tree traversal direction: Go left if the left half is already (partly) used. Same for right.
        else:
            direction = bool(random.randint(0, 1))  # If none of the halves or both are used: coin flip
        #split requests: 1 request for a multiple of 2, one for the rest (if uneven number)
        even = (amount // 2) * 2
        uneven = amount % 2
        gpu_list = list()
        if even:
            reserved_gpus = self._head.get_free_gpus(even, direction)
            if not reserved_gpus:
                reserved_gpus = self._head.get_free_gpus(even, not direction)
            if not reserved_gpus:
                reserved_gpus = self._head.get_free_gpus(even, direction, False)
            gpu_list += reserved_gpus
        if uneven:
            gpu_list += self._head.get_free_gpus(uneven, direction, False)
        if not gpu_list:
            print(amount)
        return gpu_list

    def total(self):
        count, a, b, c = self._head.total()
        return count

    @property
    def free(self):
        return self._head.free


class Resources(object):

    def __init__(self, gpus, threads, memory):
        self.n_gpus = gpus
        self.threads = threads
        self.memory = memory

    def __add__(self, other):
        return Resources(self.n_gpus + other.n_gpus, self.threads + other.threads, self.memory + other.memory)

    def __le__(self, other):
        return (self.n_gpus <= other.n_gpus) and (self.threads <= other.threads) and (self.memory <= other.memory)


class Job(object):

    def __init__(self, job_id: int, address, n_gpus: int, threads: int, memory: int, hours: int, name: str, user: str, depends_on: List[int]):
        self.job_id = job_id
        self.resources = Resources(n_gpus, threads, memory)
        self.address = address
        self.gpus: List[GPUTreeNode] = []
        self.hours: int = hours
        self.name: str = name
        self.user: str = user
        self.depends_on: List[int] = depends_on
        self.priority = 0.0
        self.reset_elapsed_time()

    def to_string(self, job_id: int, status: str, verbose=False):
        s = '|' + str(job_id).zfill(7)
        s += ' ' + self.name[0:16].rjust(16)
        s += '  ' + self.time.strftime('%d-%m-%Y %H:%M:%S')
        s += '       ' + status
        s += '  ' + self.user[0:11].rjust(11)
        # priority only exists for waiting jobs
        if status == 'w':
            s += ('%.5f' % self.priority).rjust(10)
        else:
            s += '-'.rjust(10)
        if verbose:
            s += '  ' + str(self.resources.threads).rjust(7)
            s += '  ' + str(self.resources.memory).rjust(6) + 'mb'
            s += '  ' + str(self.hours).rjust(9) + 'h'
            s += '  ' + str(self.resources.n_gpus).rjust(4)
        s += '|'
        return s

    def fits(self, resources):
        return self.resources <= resources

    def reset_elapsed_time(self):
        self.time = datetime.datetime.now()

    @property
    def elapsed_time(self):
        return max(1.0, (datetime.datetime.now() - self.time).total_seconds())  # waiting time/running time of the job


class Scheduler(object):

    def __init__(self, gpus: List[Tuple[int, bool]], threads: int, memory: int, abort_on_time_limit: bool):
        # dict mapping job_ids to jobs
        self.jobs: Dict[int, Job] = dict()
        self.running_jobs: List[int] = []
        self.waiting_jobs: List[int] = []
        self.held_jobs: List[int] = []
        # resources
        self.gpu_tree = GPUTree()
        self.gpu_tree.build_tree(gpus)
        self.resources = Resources(self.gpu_tree.total(), threads, memory)
        # free resources
        self.free_resources = Resources(self.gpu_tree.total(), threads, memory)
        self.abort_on_time_limit = abort_on_time_limit
        self.on_job_start_callback = lambda a: a

    def update_resources(self, resources: Resources):
        self.free_resources = resources

    def update_priorities(self):
        now = datetime.datetime.now()
        joblist = [self.jobs[job_id] for job_id in self.waiting_jobs]
        if len(joblist) == 0:
            return
        # sum of all requests
        n_gpus = max(1, sum([job.resources.n_gpus for job in joblist]))
        threads = sum([job.resources.threads for job in joblist])
        memory = sum([job.resources.memory for job in joblist])
        hours = sum([job.hours for job in joblist])
        # waiting times of each job and sum of all waiting times (measure in hours)
        waiting_times = [max(1, int((now - job.time).total_seconds() / 3600)) for job in joblist]
        acc_waiting_time = sum(waiting_times)
        # priority for each job is the waiting time divided by the requested resources
        for idx, job in enumerate(joblist):
            job.priority = float(waiting_times[idx]) / acc_waiting_time
            job.priority /= max(float(job.resources.n_gpus) / n_gpus, float(job.resources.threads) / threads,
                                float(job.resources.memory) / memory) + float(job.hours) / hours
        ## if a user already has a running job, decrease his priority
        for job in joblist:
            user_job_fraction = 0.0
            if len(self.running_jobs) > 0:
                user_job_fraction = len([0 for j in self.running_jobs if self.jobs[j].user == job.user]) / float(
                    len(self.running_jobs))
            running_jobs_penalty = max(0.001, 1.0 - user_job_fraction)
            job.priority *= running_jobs_penalty
        # normalize such that max priority is one
        max_priority = max([job.priority for job in joblist])
        for job in joblist:
            job.priority = job.priority / max_priority

    def waiting_time(self, job: Job, running_jobs: List[Job]):
        # sort jobs by remaining runtime (in seconds)
        runtimes = [max(1, j.hours * 3600 - j.elapsed_time) for j in running_jobs]
        runtimes, sorted_jobs = list(zip(*sorted(zip(runtimes, running_jobs))))
        # determine waiting time until job can be submitted
        tmp_resources = self.free_resources
        for idx in range(len(sorted_jobs)):
            tmp_resources = tmp_resources + sorted_jobs[idx].resources
            if job.fits(tmp_resources):
                return runtimes[idx]
        return 0

    def get_next_job(self, jobs: Dict[int, Job], waiting_ids: List[int], running_ids: List[int]):
        try:
            waiting_jobs = [jobs[job_id] for job_id in waiting_ids]
            if waiting_jobs:
                # sort waiting jobs by priority (first key) and job_id (second key in case of equal priority)
                priorities = [job.priority for job in waiting_jobs]
                job_ids = [job.job_id for job in waiting_jobs]
                priorities, job_ids, waiting_jobs = list(
                    zip(*sorted(zip(priorities, job_ids, waiting_jobs), key=lambda sl: (-sl[0], sl[1]))))
                # determine next job to schedule
                if waiting_jobs[0].fits(self.free_resources):
                    return waiting_jobs[0].job_id
                else:
                    # if job does not fit compute waiting time and possibly submit another job that runs in the meantime
                    running_jobs = [jobs[job_id] for job_id in running_ids]
                    time_slot = self.waiting_time(waiting_jobs[0], running_jobs)
                    for job in waiting_jobs[1:]:
                        if job.fits(self.free_resources) and job.hours * 3600 <= time_slot:
                            return job.job_id
        except Exception as e:
            return None

    def start_job(self, job_id: int):
        # start_jobs assume that job fits into free resources
        self.jobs[job_id].reset_elapsed_time()  # running time starts now
        self.free_resources.threads -= self.jobs[job_id].resources.threads
        self.free_resources.memory -= self.jobs[job_id].resources.memory
        self.jobs[job_id].gpus = self.gpu_tree.reserveGPUs(self.jobs[job_id].resources.n_gpus)
        self.free_resources.n_gpus = self.gpu_tree.free
        self.on_job_start_callback(self.jobs[job_id])

    def schedule(self):
        job_id = self.get_next_job(self.jobs, self.waiting_jobs, self.running_jobs)
        while job_id is not None:
            self.start_job(job_id)
            self.waiting_jobs.remove(job_id)
            self.running_jobs.append(job_id)
            self.update_priorities()
            #self.update_resources(self.free_resources)
            job_id = self.get_next_job(self.jobs, self.waiting_jobs, self.running_jobs)

    def delete_job(self, job_id: int, reschedule=True):

        # remove job_id from dependencies
        held = list(set(self.held_jobs) - {job_id})
        for j in held:
            if job_id in self.jobs[j].depends_on:
                self.jobs[j].depends_on.remove(job_id)
                # if no dependencies left, held jobs switches to waiting
                if len(self.jobs[j].depends_on) == 0:
                    self.held_jobs.remove(j)
                    self.waiting_jobs.append(j)
                    self.jobs[j].reset_elapsed_time()  # waiting time starts now
                    self.update_priorities()

        # remove job from job container
        job = self.jobs.pop(job_id, None)

        ### delete a held or waiting job ###
        if job_id in self.waiting_jobs:

            self.waiting_jobs.remove(job_id)
            self.update_priorities()
        elif job_id in self.held_jobs:

            self.held_jobs.remove(job_id)
        elif job_id in self.running_jobs:

            # free resources
            for gpu in job.gpus:
                gpu.free = True
            self.free_resources = self.free_resources + job.resources
            self.running_jobs.remove(job_id)
            if reschedule:
                self.schedule()

    def submit_job(self, job_id: int, job: Job):
        # check if job can be executed with the given resources
        fits_resources = job.fits(self.resources)
        if fits_resources:  # put job into queue
            job.reset_elapsed_time()
            self.jobs[job_id] = job
            if len(job.depends_on) == 0:
                self.waiting_jobs.append(job_id)
                self.update_priorities()
                self.schedule()
            else:
                self.held_jobs.append(job_id)
        return fits_resources

    def find_jobs(self, specifier: str, to_delete: str):
        joblist = []
        # find relevant jobs
        for job_id in self.jobs:
            if (specifier == 'name') and (re.match(to_delete, self.jobs[job_id].name) is not None):
                joblist.append(job_id)
            elif (specifier == 'user') and (re.match(to_delete, self.jobs[job_id].user) is not None):
                joblist.append(job_id)
            elif (specifier == 'id') and (job_id == int(to_delete)):
                joblist.append(job_id)
        return joblist
