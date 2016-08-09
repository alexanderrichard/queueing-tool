/*
 * QueueStatistics.cc
 *
 *  Created on: Jul 22, 2014
 *      Author: richard
 */

#include "QueueStatistics.hh"
#include <stdlib.h>
#include <fstream>
#include <ctime>
#include <cmath>
#include <sys/types.h>
#include <iomanip>
#include <sstream>
#include "Lock.hh"
#include "Utils.hh"

QueueStatistics::QueueStatistics() :
	runningId_(0)
{}

void QueueStatistics::readQueueConfig() {
	std::string qconfig = std::string(root).append("/queue.config");
	std::ifstream f(qconfig.c_str());
	if (!f.is_open()) {
		std::cerr << "ERROR: Could not open " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	std::string line, identifier, value;
	getline(f, line);
	identifier = line.substr(0, 7);
	if (identifier.compare("threads") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.availableThreads = atoi(line.substr(8).c_str());
	getline(f, line);
	identifier = line.substr(0, 6);
	if (identifier.compare("memory") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.availableMemory = atoi(line.substr(7).c_str());
	getline(f, line);
	identifier = line.substr(0, 4);
	if (identifier.compare("gpus") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.availableGpus = atoi(line.substr(5).c_str());
	queueConfiguration_.isGpuOccupied.resize(queueConfiguration_.availableGpus, false);
	getline(f, line);
	identifier = line.substr(0, 16);
	if (identifier.compare("abortOnTimeLimit") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.abortOnTimeLimit = (line.substr(17).compare("true") == 0);
	getline(f, line);
	identifier = line.substr(0, 14);
	if (identifier.compare("addUnkownUsers") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.addUnkownUsers = (line.substr(15).compare("true") == 0);
	getline(f, line);
	identifier = line.substr(0, 19);
	if (identifier.compare("regeneration-factor") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.decayFactor = std::max(1.0 - atof(line.substr(13).c_str()), 0.0);
	getline(f, line);
	identifier = line.substr(0, 18);
	if (identifier.compare("max-priority-class") != 0) {
		std::cerr << "ERROR: Wrong format of " << root << "/queue.config." << std::endl;
		Lock::unlock();
		exit(1);
	}
	queueConfiguration_.maxPriorityClass = atoi(line.substr(18).c_str());
	f.close();
}

void QueueStatistics::read(bool ignoreLock) {
	std::string q = std::string(root).append("/.queue");
	std::ifstream f(q.c_str());
	if (!f.is_open()) {
		std::cerr << "ERROR: Could not open " << root << "/.queue." << std::endl;
		if (!ignoreLock)
			Lock::unlock();
		exit(1);
	}
	std::string line;
	// start with global statistics
	getline(f, line);
	runningId_ = atoi(line.c_str());
	getline(f, line);
	queueConfiguration_.occupiedMemory = atoi(line.c_str());
	getline(f, line);
	queueConfiguration_.occupiedThreads = atoi(line.c_str());
	getline(f, line);
	u32 gpuOccupation = atoi(line.c_str());
	for (u32 i = 0; i < queueConfiguration_.availableGpus; i++) {
		u32 k = pow(2, i);
		queueConfiguration_.isGpuOccupied.at(i) = ( (gpuOccupation & k) > 0 ? true : false );
	}
	getline(f, line);
	u32 nJobs = atoi(line.c_str());
	jobs_.clear();
	jobs_.resize(nJobs);
	// read all jobs
	for (u32 i = 0; i < jobs_.size(); i++) {
		getline(f, line);
		jobs_.at(i).id = atoi(line.c_str());
		getline(f, line);
		jobs_.at(i).pid = atoi(line.c_str());
		getline(f, jobs_.at(i).name);
		getline(f, jobs_.at(i).user);
		getline(f, line);
		jobs_.at(i).time = atoi(line.c_str());
		getline(f, line);
		jobs_.at(i).status = line[0];
		getline(f, line);
		jobs_.at(i).useGpu = (atoi(line.c_str()) == 0 ? false : true);
		getline(f, line);
		jobs_.at(i).gpuId = atoi(line.c_str());
		getline(f, line);
		jobs_.at(i).threads = atoi(line.c_str());
		getline(f, line);
		jobs_.at(i).maxMemory = atoi(line.c_str());
		getline(f, line);
		jobs_.at(i).timeLimit = atoi(line.c_str());
		getline(f, jobs_.at(i).script);
		getline(f, jobs_.at(i).directory);
		getline(f, line);
		jobs_.at(i).priorityClass = atoi(line.c_str());
		getline(f, line);
		Utils::stringToIntVector(jobs_.at(i).dependsOn, line);
		getline(f, line);
		Utils::stringToIntVector(jobs_.at(i).parallelJobs, line);
	}
	// close the file
	f.close();

	// read the user priorities
	userPriorities_.read(ignoreLock);
}

void QueueStatistics::write() {
	std::string q = std::string(root).append("/.queue");
	std::ofstream f(q.c_str());
	if (!f.is_open()) {
		std::cerr << "ERROR: Could not open " << root << "/.queue." << std::endl;
		Lock::unlock();
		exit(1);
	}
	// start with global statistics
	f << runningId_ << std::endl;
	f << queueConfiguration_.occupiedMemory << std::endl;
	f << queueConfiguration_.occupiedThreads << std::endl;
	u32 gpuOccupation = 0;
	for (u32 i = 0; i < queueConfiguration_.availableGpus; i++) {
		if (queueConfiguration_.isGpuOccupied.at(i))
			gpuOccupation |= (u32)pow(2, i);
	}
	f << gpuOccupation << std::endl;
	f << jobs_.size() << std::endl;
	// write all jobs
	for (u32 i = 0; i < jobs_.size(); i++) {
		f << jobs_.at(i).id << std::endl;
		f << jobs_.at(i).pid << std::endl;
		f << jobs_.at(i).name << std::endl;
		f << jobs_.at(i).user << std::endl;
		f << jobs_.at(i).time << std::endl;
		f << jobs_.at(i).status << std::endl;
		f << (jobs_.at(i).useGpu ? 1 : 0) << std::endl;
		f << jobs_.at(i).gpuId << std::endl;
		f << jobs_.at(i).threads << std::endl;
		f << jobs_.at(i).maxMemory << std::endl;
		f << jobs_.at(i).timeLimit << std::endl;
		f << jobs_.at(i).script << std::endl;
		f << jobs_.at(i).directory << std::endl;
		f << jobs_.at(i).priorityClass << std::endl;
		for (u32 j = 0; j < jobs_.at(i).dependsOn.size(); j++)
			f << " " << jobs_.at(i).dependsOn.at(j);
		f << std::endl;
		for (u32 j = 0; j < jobs_.at(i).parallelJobs.size(); j++)
			f << " " << jobs_.at(i).parallelJobs.at(j);
		f << std::endl;
	}
	// close the file
	f.close();

	// write the user priorities
	userPriorities_.write();
}

u32 QueueStatistics::getJobIndexById(u32 id) {
	for (u32 i = 0; i < jobs_.size(); i++) {
		if (jobs_.at(i).id == id)
			return i;
	}
	std::cerr << "ERROR: no job with id " << id << " found." << std::endl;
	Lock::unlock();
	exit(1);
}

bool QueueStatistics::jobFits(const Job& job) const {
	bool fits = true;
	fits = fits && (job.threads <= queueConfiguration_.availableThreads - queueConfiguration_.occupiedThreads ? true : false);
	fits = fits && (job.maxMemory <= queueConfiguration_.availableMemory - queueConfiguration_.occupiedMemory ? true : false);
	if (job.useGpu) {
		bool hasFreeGpu = false;
		for (u32 i = 0; i < queueConfiguration_.availableGpus; i++) {
			if (!queueConfiguration_.isGpuOccupied.at(i))
				hasFreeGpu = true;
		}
		fits = fits && hasFreeGpu;
	}
	return fits;
}

bool QueueStatistics::findExecutableJob(u32& id) {
	// sort jobs by priority
	std::vector<u32> sortedIdx;
	userPriorities_.sortByPriorities(jobs_, sortedIdx);

	// check if there are jobs in the highest priority class and schedule them or wait until there is space for them
	for (u32 i = 0; i < sortedIdx.size(); i++) {
		if ((jobs_.at(sortedIdx.at(i)).status == 'w') && (jobs_.at(sortedIdx.at(i)).priorityClass == queueConfiguration_.maxPriorityClass)) {
			if (jobFits(jobs_.at(sortedIdx.at(i)))) {
				id = jobs_.at(sortedIdx.at(i)).id;
				return true;
			}
			else {
				return false;
			}
		}
	}

	// find next fitting job according to priorities
	for (u32 i = 0; i < sortedIdx.size(); i++) {
		if ( (jobs_.at(sortedIdx.at(i)).status == 'w' ) && (jobFits(jobs_.at(sortedIdx.at(i)))) ) {
			id = jobs_.at(sortedIdx.at(i)).id;
			return true;
		}
	}

	// if no job fits...
	return false;
}

void QueueStatistics::invokePenalty(Job j, bool decay) {
	userPriorities_.invokePenalty(j, jobs_, queueConfiguration_, decay);
}

u32 QueueStatistics::addJob(const std::string& name, bool useGpu, u32 threads, u32 maxMemory, u32 timeLimit, const std::string& script, const std::string& directory, char status) {
	if (maxMemory > queueConfiguration_.availableMemory) {
		std::cerr << "ERROR: Requested memory exceeds available memory." << std::endl;
		Lock::unlock();
		exit(1);
	}
	if (threads > queueConfiguration_.availableThreads) {
		std::cerr << "ERROR: Requested threads exceed available threads." << std::endl;
		Lock::unlock();
		exit(1);
	}
	if (useGpu && (queueConfiguration_.availableGpus == 0)) {
		std::cerr << "ERROR: This machine has no GPU available." << std::endl;
		Lock::unlock();
		exit(1);
	}
	if (timeLimit == 0) {
		std::cerr << "ERROR: Time limit must be at least 1(h)." << std::endl;
		Lock::unlock();
		exit(1);
	}
	if (maxMemory == 0) {
		std::cerr << "ERROR: Requested memory must be at least 1mb." << std::endl;
		Lock::unlock();
		exit(1);
	}
	if (threads == 0) {
		std::cerr << "ERROR: Requested threads must be at least 1." << std::endl;
		Lock::unlock();
		exit(1);
	}

	Job j;
	runningId_ = (runningId_ >= 999999999 ? 1 : runningId_ + 1);
	j.id = runningId_;
	j.name = name;
	char* user = getenv("USER");
	j.user = std::string(user);
	time(&(j.time));
	j.status = status;
	j.useGpu = useGpu;
	j.gpuId = -1;
	j.threads = threads;
	j.maxMemory = maxMemory;
	j.timeLimit = timeLimit;
	j.script = script;
	j.directory = directory;
	j.priorityClass = 0;

	// check if the user exists or if he should be created
	if (!userPriorities_.userExists(j.user)) {
		if (queueConfiguration_.addUnkownUsers) {
			addUser(j.user);
		}
		else {
			std::cerr << "ERROR: User " << j.user << " is not registered for the queue." << std::endl;
			Lock::unlock();
			exit(1);
		}
	}

	jobs_.push_back(j);

	return j.id;
}

void QueueStatistics::removeJob(u32 id) {
	// if job is running: free resources
	if ((job(id).status == 'r') || (job(id).status == 'p')) {
		queueConfiguration_.occupiedMemory -= job(id).maxMemory;
		queueConfiguration_.occupiedThreads -= job(id).threads;
		if (job(id).gpuId >= 0) {
			queueConfiguration_.isGpuOccupied.at((u32)(job(id).gpuId)) = false;
		}
	}

	for (u32 i = 0; i < jobs_.size(); i++) {
		// remove this job's id from the dependsOn list of all held jobs
		if (jobs_.at(i).status == 'h')
			jobs_.at(i).removeDependenceOn(id);
		// remove this job's id from the parallelJobs list of all jobs
		jobs_.at(i).removeFromParallelJobs(id);
	}

	// remove the script for this job
	if (job(id).parallelJobs.size() == 0)
		remove(job(id).script.substr(0, job(id).script.find_first_of(' ')).c_str());

	// erase job
	jobs_.erase(jobs_.begin() + getJobIndexById(id));
}

void QueueStatistics::prepareJobForRun(u32 id) {
	// increase priority class of the highest rated job that has been "overtaken" by this job due to too few available resources
	std::vector<u32> sortedIdx;
	userPriorities_.sortByPriorities(jobs_, sortedIdx);
	for (u32 i = 0; i < sortedIdx.size(); i++) {
		if ((jobs_.at(sortedIdx.at(i)).status == 'w') &&
				(userPriorities_.getPriority(jobs_.at(sortedIdx.at(i))) > userPriorities_.getPriority(job(id)))) {
			jobs_.at(sortedIdx.at(i)).priorityClass++;
			break;
		}
	}

	queueConfiguration_.occupiedMemory += job(id).maxMemory;
	queueConfiguration_.occupiedThreads += job(id).threads;
	if (job(id).useGpu) {
		for (u32 i = 0; i < queueConfiguration_.availableGpus; i++) {
			if (!queueConfiguration_.isGpuOccupied.at(i)) {
				queueConfiguration_.isGpuOccupied.at(i) = true;
				job(id).gpuId = (s32)i;
				break;
			}
		}
	}

	job(id).status = 'p';
	time(&(job(id).time));

	invokePenalty(job(id));
}

void QueueStatistics::reset() {
	queueConfiguration_.occupiedMemory = 0;
	queueConfiguration_.occupiedThreads = 0;
	for (u32 i = 0; i < queueConfiguration_.availableGpus; i++) {
		queueConfiguration_.isGpuOccupied.at(i) = false;
	}
	jobs_.clear();
	userPriorities_.reset();
}

void QueueStatistics::printJob(Job& j, bool verbose) {
	struct tm* timeinfo;
	char buffer[80];
	timeinfo = localtime(&(j.time));
	strftime(buffer, 80, "%d-%m-%Y %H:%M:%S", timeinfo);
	std::string date(buffer);
	u32 jobNameLength = (j.name.length() > 16 ? 16 : j.name.length());
	u32 userNameLength = (j.user.length() > 12 ? 12: j.user.length());
	std::cout << "|" << std::setfill('0') << std::setw(9) << j.id << " "
			<< std::setfill(' ') << std::setw(16) << j.name.substr(0, jobNameLength) << "  "
			<< date << "       "
			<< j.status << " "
			<< std::setfill(' ') << std::setw(12) << j.user.substr(0, userNameLength) << "   "
			<< std::fixed << userPriorities_.getPriority(j);
	if (verbose) {
		std::cout << "  " << std::setfill(' ') << std::setw(14) << j.priorityClass
				<< " " << std::setfill(' ') << std::setw(8) << j.threads << " "
				<< std::setfill(' ') << std::setw(6) << j.maxMemory << "mb "
				<< std::setfill(' ') << std::setw(10) << j.timeLimit << "h "
				<< std::setfill(' ') << std::setw(5) << (j.useGpu ? "true" : "false");
	}
	std::cout << "|" << std::endl;
}

void QueueStatistics::print(bool verbose) {
	std::cout.precision(5);
	// print header
	std::cout << "+------------------------------------------------------------------------------";
	if (verbose)
		std::cout << "----------------------------------------------------";
	std::cout << "+" << std::endl;
	std::cout << "|id                 jobname    submit/start time  status         user  priority";
	if (verbose)
		std::cout << "  priority-class  threads   memory  time limit   gpu";
	std::cout << "|" << std::endl;
	std::cout << "+------------------------------------------------------------------------------";
	if (verbose)
		std::cout << "----------------------------------------------------";
	std::cout << "+" << std::endl;
	// print all pending and running jobs
	for (u32 i = 0; i < jobs_.size(); i++) {
		if ((jobs_.at(i).status == 'r') || (jobs_.at(i).status == 'p')) {
			printJob(jobs_.at(i), verbose);
		}
	}
	// print all waiting jobs
	for (u32 i = 0; i < jobs_.size(); i++) {
		if (jobs_.at(i).status == 'w') {
			printJob(jobs_.at(i), verbose);
		}
	}
	// print all held jobs (jobs that wait for other jobs to be finished)
	for (u32 i = 0; i < jobs_.size(); i++) {
		if (jobs_.at(i).status == 'h') {
			printJob(jobs_.at(i), verbose);
		}
	}
	// print footer
	std::cout << "+------------------------------------------------------------------------------";
	if (verbose)
		std::cout << "----------------------------------------------------";
	std::cout << "+" << std::endl;
}

void QueueStatistics::info() {
	std::cout << "Queue info (occupied/available)" << std::endl;
	std::cout << std::setfill(' ') << std::setw(10) << "memory: " << queueConfiguration_.occupiedMemory
			<< "/" << queueConfiguration_.availableMemory << std::endl;
	std::cout << std::setfill(' ') << std::setw(10) << "threads: " << queueConfiguration_.occupiedThreads
			<< "/" << queueConfiguration_.availableThreads << std::endl;
	u32 occupiedGpus = 0;
	for (u32 i = 0; i < queueConfiguration_.availableGpus; i++)
		occupiedGpus += (queueConfiguration_.isGpuOccupied.at(i) ? 1 : 0);
	std::cout << std::setfill(' ') << std::setw(10) << "gpus: " << occupiedGpus
			<< "/" << queueConfiguration_.availableGpus << std::endl;
}

void QueueStatistics::addUser(const std::string& user, f32 priorityFactor) {
	userPriorities_.addUser(user, priorityFactor);
}

void QueueStatistics::delUser(const std::string& user) {
	bool hasRunningJobs = false;
	for (u32 i = 0; i < jobs_.size(); i++) {
		if (jobs_.at(i).user.compare(user) == 0)
			hasRunningJobs = true;
	}
	if (hasRunningJobs) {
		std::cerr << "ERROR: User " << user << " has running jobs." << std::endl;
		Lock::unlock();
		exit(1);
	}
	else {
		userPriorities_.delUser(user);
	}
}
