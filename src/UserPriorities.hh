/*
 * UserPriorities.hh
 *
 *  Created on: Sep 2, 2015
 *      Author: richard
 */

#ifndef USERPRIORITIES_HH_
#define USERPRIORITIES_HH_

#include "Types.hh"
#include <iostream>
#include <string.h>
#include <vector>
#include <map>

class UserPriorities
{
private:
	struct JobSortingSturcture {
		u32 jobIndex;
		u32 jobId;
		f32 priority;
		u32 priorityClass;
	};
	static bool jobCompare(const JobSortingSturcture& i, const JobSortingSturcture& j) {
		if (i.priority == j.priority) {
			if (i.priorityClass == j.priorityClass)
				return (i.jobId < j.jobId);
			else
				return (i.priorityClass > j.priorityClass);
		}
		else {
			return (i.priority > j.priority);
		}
	}
private:
	std::map<std::string, u32> userToIndex_;
	std::vector<f32> penalty_;
	std::vector<f32> priority_;
	std::vector<f32> priorityFactor_;
	bool needsPriorityUpdate_;
	void verifyUser(const std::string& user, bool ignoreLock = false) const;
	void computePriority();
public:
	UserPriorities();
	void sortByPriorities(const std::vector<Job>& jobs, std::vector<u32>& mapping);
	void invokePenalty(const Job& j, const std::vector<Job>& jobs, const QueueConfiguration& qConf, bool decay = true);
	f32 getPriority(Job& j);
	void addUser(const std::string& user, f32 priorityFactor);
	void delUser(const std::string& user);
	bool userExists(const std::string& user) const;
	void read(bool ignoreLock = false);
	void write();
	void reset();
};

#endif /* USERPRIORITIES_HH_ */
