/*
 * Types.hh
 *
 *  Created on: 24.03.2014
 *      Author: richard
 */

#ifndef TYPES_HH_
#define TYPES_HH_

#include <iostream>
#include <vector>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long int u64;
typedef signed long int s64;
typedef float f32;
typedef double f64;

struct Job {
	u32 id;
	u32 pid;
	std::string name;
	std::string user;
	time_t time;
	char status;
	bool useGpu;
	s32 gpuId; // -1: no gpu, else assigned gpu id
	u32 threads;
	u32 maxMemory; // in MB
	u32 timeLimit; // in hours
	std::vector<u32> dependsOn; // ids of jobs this job depends on (= has to wait for before starting) // TODO: this also has to be saved!!!
	std::vector<u32> parallelJobs; // ids of jobs that belong to the same block as this job // TODO: this also has to be saved!!!
	std::string script;
	std::string directory;
	u32 priorityClass;

	void removeDependenceOn(u32 id) {
		for (u32 i = 0; i < dependsOn.size(); i++) {
			if (dependsOn.at(i) == id) {
				dependsOn.erase(dependsOn.begin() + i);
				// if list is now empty switch status to wait
				if (dependsOn.size() == 0)
					status = 'w';
				return;
			}
		}
	}

	void removeFromParallelJobs(u32 id) {
		for (u32 i = 0; i < parallelJobs.size(); i++) {
			if (parallelJobs.at(i) == id) {
				parallelJobs.erase(parallelJobs.begin() + i);
				return;
			}
		}
	}
};

struct QueueConfiguration {
	u32 availableMemory;
	u32 availableThreads;
	u32 availableGpus;
	u32 occupiedMemory;
	u32 occupiedThreads;
	std::vector<bool> isGpuOccupied;
	bool abortOnTimeLimit;
	f32 decayFactor;
	u32 maxPriorityClass;
};

static const char* root = "/usr/queue";

#endif /* TYPES_HH_ */
