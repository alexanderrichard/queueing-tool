/*
 * QueueStatistics.hh
 *
 *  Created on: Jul 22, 2014
 *      Author: richard
 */

#ifndef QUEUESTATISTICS_HH_
#define QUEUESTATISTICS_HH_

#include "Types.hh"
#include "Lock.hh"
#include "UserPriorities.hh"
#include <iostream>
#include <string.h>
#include <vector>
#include <map>

class QueueStatistics
{
private:
	u32 runningId_;
	std::vector<Job> jobs_;
	// queue configuration
	QueueConfiguration queueConfiguration_;
	UserPriorities userPriorities_;
private:
	void printJob(Job& j, bool verbose = false);
	u32 getJobIndexById(u32 id);
	bool jobFits(const Job& job) const;
public:
	QueueStatistics();
	void readQueueConfig();
	void read(bool ignoreLock = false);
	void write();

	Job& job(u32 id) { return jobs_.at(getJobIndexById(id)); }
	std::vector<Job>& jobs() { return jobs_; }

	bool findExecutableJob(u32& id);
	void invokePenalty(Job j, bool decay = true);
	bool abortOnTimeLimit() const { return queueConfiguration_.abortOnTimeLimit; }

	u32 addJob(const std::string& name, bool useGpu, u32 threads, u32 maxMemory, u32 timeLimit, const std::string& script, const std::string& directory, char status);
	void removeJob(u32 id);
	void prepareJobForRun(u32 id);
	void reset();
	void print(bool verbose = false);
	void info();

	void addUser(const std::string& user, f32 priorityFactor = 1.0);
	void delUser(const std::string& user);
};

#endif /* QUEUESTATISTICS_HH_ */
