/*
 * main.cc
 *
 *  Created on: Jul 21, 2014
 *      Author: richard
 */

#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iomanip>
#include <time.h>
#include "Types.hh"
#include "Lock.hh"
#include "Utils.hh"
#include "QueueStatistics.hh"
#include "Parser.hh"

QueueStatistics queue;

enum Action {none, submit, exec, del, state, reset, info, jobinfo, adduser, deluser};

void wakeNextJobs() {
	u32 nextJobId = 0;
	while (queue.findExecutableJob(nextJobId)) {
		queue.prepareJobForRun(nextJobId);
		std::stringstream cmd;
		cmd << "sudo " << root << "/_wake.sh " << queue.job(nextJobId).pid;
		system(cmd.str().c_str());
	}
}

void submitBlocks(std::string& script, std::string& directory) {

	Parser parser(script);
	parser.initialize();
	parser.parse();
	std::vector<u32> oldBlockIds;
	std::vector<u32> newBlockIds;
	// queue all blocks
	for (u32 i = 0; i < parser.nBlocks(); i++) {
		newBlockIds.clear();
		// queue subtasks of the current block
		for (u32 subtask = 1; subtask <= parser.block(i).subtasks; subtask++) {
			// add job to queue
			std::stringstream script;
			script << parser.block(i).script << " " << parser.scriptParameters() << " " << subtask;
			std::stringstream name;
			name << parser.block(i).name;
			if (parser.block(i).subtasks > 1)
				name << "." << subtask;
			u32 id = queue.addJob(name.str(), parser.block(i).gpu, parser.block(i).threads, parser.block(i).memory, parser.block(i).hours,
					script.str(), directory, '-');
			queue.jobs().back().dependsOn = oldBlockIds;
			// print information about submitted job
			std::cout << "submit job " << name.str() << " with threads=" << parser.block(i).threads << ", memory=" << parser.block(i).memory
					<< "mb hours=" << parser.block(i).hours << ", gpu=" << (parser.block(i).gpu ? "true" : "false") << "..." << std::endl;
			// execute the job
			std::stringstream cmd;
			cmd << root << "/scheduler exec " << queue.jobs().back().id << " &";
			system(cmd.str().c_str());
			// keep track of job ids
			newBlockIds.push_back(id);
		}
		// establish parallelism constraints within the current block
		for (u32 j = 0; j < newBlockIds.size(); j++) {
			queue.job(newBlockIds.at(j)).parallelJobs = newBlockIds;
			queue.job(newBlockIds.at(j)).parallelJobs.erase(queue.job(newBlockIds.at(j)).parallelJobs.begin() + j);
		}
		// newBlockIds become the job ids from the preceding block
		oldBlockIds.swap(newBlockIds);
	}
}

void execute(u32 id) {

	// set correct pid and change the status from '-' to either wait or hold
	queue.job(id).pid = getpid();
	queue.job(id).status = (queue.job(id).dependsOn.size() == 0 ? 'w' : 'h');
	// can the job be scheduled immediately?
	bool immediateExecution = false;
	u32 tmpId = 0;
	if (queue.findExecutableJob(tmpId) && (tmpId == id)) {
		queue.prepareJobForRun(id);
		immediateExecution = true;
	}

	queue.write();
	Lock::unlock();

	// pause process until job can be scheduled
	if (!immediateExecution) {
		std::stringstream cmd;
		cmd << "kill -s STOP " << queue.job(id).pid;
		system(cmd.str().c_str());
		// wait until the job status is 'p' (pending) -> ensure that kill -s STOP <myPid> is caught and job is reawakened before moving on
		while (queue.job(id).status != 'p') {
			// busy waiting until next call to queue.read() (100ms)
			clock_t t = clock();
			while ( ((float)clock() - t) / CLOCKS_PER_SEC < 0.1) ;
			queue.read(); // reading itself is not critical (values may be corrupted but are later updated anyway)
		}
	}

	/* run job */
	Lock::waitForLock();
	queue.read();

	queue.job(id).status = 'r';
	std::stringstream strId;
	strId << std::setfill('0') << std::setw(10) << id;
	std::stringstream s;
	s << "cd " << queue.job(id).directory << " ; "
			<< root << "/_execute.sh " << queue.job(id).name << "." << strId.str() << " "
			<< queue.job(id).threads << " "
			<< queue.job(id).maxMemory << " "
			<< queue.job(id).timeLimit << " "
			<< queue.job(id).gpuId << " "
			<< (queue.abortOnTimeLimit() ? "true" : "false") << " "
			<< "\"" << queue.job(id).script << "\"";

	queue.write();
	Lock::unlock();

	time_t startTime, endTime;
	time(&startTime);
	system(s.str().c_str());
	time(&endTime);
	/* done */

	// if execution terminates, request a lock and remove the (finished) job
	Lock::waitForLock();
	queue.read();

	// check if time limit has been clearly exceeded and invoke extra penalty if this is the case
	if (difftime(endTime, startTime) / 3600 > queue.job(id).timeLimit + 1) {
		queue.job(id).timeLimit = (u32)(difftime(endTime, startTime) / 3600) * 2; // twice the penalty the job would have gotten in the first place
		queue.invokePenalty(queue.job(id), false);
	}

	queue.removeJob(id);

	// wake the next job
	wakeNextJobs();
}

void deleteJob(u32 id) {
	if (queue.exists(id)) {
		// kill running processes
		std::stringstream s;
		s << root << "/_qdel.sh " << queue.job(id).pid;
		system(s.str().c_str());
		// remove job from the queue
		queue.removeJob(id);
	}
}

void resetQueue() {
	// first kill all waiting and held jobs
	for (u32 i = 0; i < queue.jobs().size(); i++) {
		if ((queue.jobs().at(i).status == 'w') || (queue.jobs().at(i).status == 'h'))
			deleteJob(queue.jobs().at(i).id);
	}
	// then kill all other jobs
	for (u32 i = 0; i < queue.jobs().size(); i++) {
		// remove running flag
		queue.jobs().at(i).status = 'w';
		deleteJob(queue.jobs().at(i).id);
	}
	queue.reset();
}

void usage(Action action, const char* programName) {
	switch ( action ) {
	case submit:
		std::cout << "usage: " << programName << " submit <script+parameters> <directory>" << std::endl;
		break;
	case exec:
		std::cout << "usage: " << programName << " exec <job-id>" << std::endl;
		break;
	case del:
		std::cout << "usage: " << programName << " del <id1|jobname1> [id2|jobname2] [id3|jobname3] ..." << std::endl;
		break;
	case state:
		std::cout << "usage: " << programName << " stat [-v]" << std::endl;
		break;
	case adduser:
		std::cout << "usage: " << programName << " adduser <user-name> [priority-factor (default: 1.0)]" << std::endl;
		break;
	case deluser:
		std::cout << "usage: " << programName << " deluser <user-name>" << std::endl;
		break;
	default:
		std::cout << "usage: " << programName << " [submit|del|stat|reset|info|adduser]" << std::endl;
	}
}

void _submit(int argc, const char* argv[]) {
	if (argc != 4) {
		usage(submit, argv[0]);
	}
	else {
		std::string script = argv[2];
		std::string directory = argv[3];
		submitBlocks(script, directory);
	}
}

void _exec(int argc, const char* argv[]) {
	if (argc != 3) {
		usage(exec, argv[0]);
	}
	else {
		execute(atoi(argv[2]));
	}
}

void _del(int argc, const char* argv[]) {
	if (argc < 3) {
		usage(del, argv[0]);
	}
	else {
		bool running = false;
		std::vector<u32> ids;
		for (int i = 2; i < argc; i++) {
			// is the argument a number (=id)?
			if (Utils::isInteger(argv[i])) {
				ids.push_back((u32)atoi(argv[i]));
			}
			// is the argument a range of ids?
			else if (Utils::isRange(argv[i])) {
				std::string str(argv[i]);
				std::vector<std::string> v;
				Utils::tokenizeString(v, str, "-");
				for (u32 id = (u32)atoi(v[0].c_str()); id <= (u32)atoi(v[1].c_str()); id++)
					ids.push_back(id);
			}
			// is the argument a jobname?
			else {
				for (u32 j = 0; j < queue.jobs().size(); j++) {
					if (queue.jobs().at(j).name.compare(argv[i]) == 0)
						ids.push_back(queue.jobs().at(j).id);
				}
			}
		}

		// delete the jobs
		for (u32 i = 0; i < ids.size(); i++) {
			// check if the job may be deleted
			std::string user = getenv("USER");
			if (user.compare(queue.job(ids.at(i)).user) != 0) {
				std::cerr << "ERROR: Could not delete job of user " << queue.job(ids.at(i)).user << ". Permission denied." << std::endl;
			}
			else {
				if ((queue.job(ids.at(i)).status == 'r') || (queue.job(ids.at(i)).status == 'p'))
					running = true;
				deleteJob(ids.at(i));
			}
		}
		// wake next jobs
		if (running)
			wakeNextJobs();
	}
}

void _adduser(int argc, const char* argv[]) {
	if ((argc != 3) && (argc != 4)) {
		usage(adduser, argv[0]);
	}
	else if (argc == 3) {
		queue.addUser(argv[2]);
		std::cout << "Added user " << argv[2] << " with priority factor 1.0." << std::endl;
	}
	else {
		queue.addUser(argv[2], atof(argv[3]));
		std::cout << "Added user " << argv[2] << " with priority factor " << atof(argv[3]) << "." << std::endl;
	}
}

void _deluser(int argc, const char* argv[]) {
	if (argc != 3) {
		usage(deluser, argv[0]);
	}
	else if (argc == 3) {
		queue.delUser(argv[2]);
		std::cout << "Deleted user " << argv[2] << "." << std::endl;
	}
}

void _print(int argc, const char* argv[]) {
	if ((argc != 2) && (argc != 3)) {
		usage(state, argv[0]);
	}
	else if (argc == 3) {
		if (strcmp("-v", argv[2]) == 0)
			queue.print(true);
		else
			usage(state, argv[0]);
	}
	else {
		queue.print(false);
	}
}

void _jobinfo(int argc, const char* argv[]) {
	if (argc != 3) {
		usage(jobinfo, argv[0]);
	}
	else {
		std::cout << "*** Job-ID " << atoi(argv[2]) << " ***"<< std::endl;
		u32 id = atoi(argv[2]);
		std::cout << "  name:       " << queue.job(id).name << std::endl;
		std::cout << "  user:       " << queue.job(id).user << std::endl;
		std::cout << "  status:     " << queue.job(id).status << std::endl;
		std::cout << "  directory:  " << queue.job(id).directory << std::endl;
		std::cout << "  script:     " << queue.job(id).script << std::endl;
		std::cout << "  pid:        " << queue.job(id).pid << std::endl;
		std::cout << "  requests:   " << queue.job(id).threads << " threads, " << queue.job(id).maxMemory << "mb"
				  << (queue.job(id).useGpu ? ", gpu" : "") << std::endl;
		std::cout << "  time limit: " << queue.job(id).timeLimit << "h" << std::endl;
	}
}

void requestRootPrivileges() {
	FILE *pPipe;
	char buffer[8];
	pPipe = popen("sudo sleep 0", "r");
	fgets(buffer, 8, pPipe);
}

int main(int argc, const char* argv[]) {

	Action action = none;

	if (argc <= 1) {
		usage(action, argv[0]);
		exit(1);
	}

	if (strcmp("submit", argv[1]) == 0) {
		action = submit;
	}
	else if (strcmp("exec", argv[1]) == 0) {
		action = exec;
	}
	else if (strcmp("del", argv[1]) == 0) {
		action = del;
	}
	else if (strcmp("stat", argv[1]) == 0) {
		action = state;
	}
	else if (strcmp("reset", argv[1]) == 0) {
		requestRootPrivileges();
		action = reset;
	}
	else if (strcmp("info", argv[1]) == 0) {
		action = info;
	}
	else  if (strcmp("jobinfo", argv[1]) == 0) {
		action = jobinfo;
	}
	else if (strcmp("adduser", argv[1]) == 0) {
		requestRootPrivileges();
		action = adduser;
	}
	else if (strcmp("deluser", argv[1]) == 0) {
		requestRootPrivileges();
		action = deluser;
	}

	Lock::waitForLock();
	queue.readQueueConfig();

	switch ( action ) {
	case submit:
		queue.read();
		_submit(argc, argv);
		queue.write();
		break;
	case exec:
		queue.read();
		_exec(argc, argv);
		queue.write();
		break;
	case del:
		queue.read();
		_del(argc, argv);
		queue.write();
		break;
	case state:
		queue.read();
		_print(argc, argv);
		break;
	case reset:
		queue.read();
		resetQueue();
		queue.write();
		break;
	case info:
		queue.read();
		queue.info();
		break;
	case jobinfo:
		queue.read();
		_jobinfo(argc, argv);
		break;
	case adduser:
		queue.read();
		_adduser(argc, argv);
		queue.write();
		break;
	case deluser:
		queue.read();
		_deluser(argc, argv);
		queue.write();
		break;
	default:
		;
	}

	Lock::unlock();

	return 0;
}
