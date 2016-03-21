/*
 * UserPriorities.cc
 *
 *  Created on: Sep 2, 2015
 *      Author: richard
 */

#include "UserPriorities.hh"
#include "Lock.hh"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdlib.h>

UserPriorities::UserPriorities() :
		needsPriorityUpdate_(true)
{}

void UserPriorities::read(bool ignoreLock) {
	std::string q = std::string(root).append("/.priorities");
	std::ifstream f(q.c_str());
	if (!f.is_open()) {
		std::cerr << "ERROR: Could not open " << root << "/.priorities." << std::endl;
		if (!ignoreLock)
			Lock::unlock();
		exit(1);
	}
	std::string line;
	getline(f, line);
	u32 nUsers = atoi(line.c_str());
	penalty_.resize(nUsers);
	priorityFactor_.resize(nUsers);
	priority_.resize(nUsers);
	// read all user priorities
	for (u32 i = 0; i < nUsers; i++) {
		getline(f, line);
		userToIndex_[line] = i;
		getline(f, line);
		penalty_.at(i) = atof(line.c_str());
		getline(f, line);
		priorityFactor_.at(i) = atof(line.c_str());
	}
	// close the file
	f.close();
}

void UserPriorities::write() {
	std::string q = std::string(root).append("/.priorities");
	std::ofstream f(q.c_str());
	if (!f.is_open()) {
		std::cerr << "ERROR: Could not open " << root << "/.priorities." << std::endl;
		Lock::unlock();
		exit(1);
	}

	f << userToIndex_.size() << std::endl;
	// write all user priorities/penalties
	for (std::map<std::string, u32>::iterator it = userToIndex_.begin(); it != userToIndex_.end(); ++it) {
		f << it->first << std::endl;
		f << penalty_.at(it->second) << std::endl;
		f << priorityFactor_.at(it->second) << std::endl;
	}
	// close the file
	f.close();
}

void UserPriorities::verifyUser(const std::string& user) const {
	if (userToIndex_.find(user) == userToIndex_.end()) {
		std::cerr << "Error: User " << user << " not known. Abort." << std::endl;
		Lock::unlock();
		exit(1);
	}
}

void UserPriorities::computePriority() {
	if (needsPriorityUpdate_) {
		// Factor 1.01 prevents priorities from becoming zero. Only for psychological reasons.
		f32 max = std::max(1.01, (*std::max_element(penalty_.begin(), penalty_.end())) * 1.01);
		for (u32 i = 0; i < penalty_.size(); i++) {
			priority_.at(i) = (1.0 - penalty_.at(i) / max) * priorityFactor_.at(i);
		}
		needsPriorityUpdate_ = false;
	}
}

void UserPriorities::sortByPriorities(const std::vector<Job>& jobs, std::vector<u32>& mapping) {
	mapping.resize(jobs.size());
	computePriority();
	std::vector<JobSortingSturcture> tmp(jobs.size());
	for (u32 i = 0; i < jobs.size(); i++) {
		verifyUser(jobs.at(i).user);
		tmp.at(i).jobIndex = i;
		tmp.at(i).jobId = jobs.at(i).id;
		tmp.at(i).priority = priority_.at(userToIndex_.at(jobs.at(i).user));
		tmp.at(i).priorityClass = jobs.at(i).priorityClass;
	}
	std::sort(tmp.begin(), tmp.end(), jobCompare);
	for (u32 i = 0; i < jobs.size(); i++) {
		mapping.at(i) = tmp.at(i).jobIndex;
	}
}

void UserPriorities::invokePenalty(const Job& j, const std::vector<Job>& jobs, const QueueConfiguration& qConf, bool decay) {
	verifyUser(j.user);
	// decay penalty of all users that do not have a job in the queue
	if (decay) {
		std::vector<f32> userDecayFactor(penalty_.size(), qConf.decayFactor);
		for (u32 i = 0; i < jobs.size(); i++) {
			userDecayFactor.at(userToIndex_[jobs.at(i).user]) = 1.0;
		}
		std::transform(penalty_.begin(), penalty_.end(), userDecayFactor.begin(), penalty_.begin(), std::multiplies<f32>());
	}
	// add penalty score for current job to respective user's penalty
	f32 score = j.timeLimit * std::max((f32)j.maxMemory / (f32)qConf.availableMemory, (f32)j.threads / (f32) qConf.availableThreads);
	penalty_.at(userToIndex_[j.user]) += score;
	// smallest penalty should be zero
	f32 min = (*std::min_element(penalty_.begin(), penalty_.end()));
	std::transform(penalty_.begin(), penalty_.end(), penalty_.begin(), std::bind2nd(std::minus<f32>(), min));
	needsPriorityUpdate_ = true;
}

f32 UserPriorities::getPriority(Job& j) {
	computePriority();
	verifyUser(j.user);
	return priority_.at(userToIndex_[j.user]);
}

void UserPriorities::addUser(const std::string& user, f32 priorityFactor) {
	// if user does not exist set an initial penalty
	if (userToIndex_.find(user) == userToIndex_.end()) {
		userToIndex_[user] = penalty_.size();
		// set penalty to zero
		penalty_.push_back(0);
		priorityFactor_.push_back(priorityFactor);
	}
	// if user exists just update the penalty factor
	else {
		priorityFactor_.at(userToIndex_[user]) = priorityFactor;
	}
}

void UserPriorities::delUser(const std::string& user) {
	verifyUser(user);
	userToIndex_.erase(user);
}

void UserPriorities::reset() {
	for (u32 i = 0; i < penalty_.size(); i++) {
		penalty_.at(i) = 0;
		needsPriorityUpdate_ = true;
	}
}
