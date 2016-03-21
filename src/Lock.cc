/*
 * Lock.cc
 *
 *  Created on: Jul 22, 2014
 *      Author: richard
 */

#include "Lock.hh"

sem_t* Lock::lock = 0;

void Lock::waitForLock() {
	mode_t old_umask = umask(0);
	lock = sem_open("qScheduler", O_CREAT, 0777, 1);
	umask(old_umask);

	// handle possible errors
	if (lock == SEM_FAILED) {
		// these errors lead to an exit
		switch ( errno ) {
		case EACCES:
			std::cerr << "ERROR: Error in semaphore access: Permission denied." << std::endl;
			break;
		case ENFILE:
			std::cerr << "ERROR: System limit on the open number of files is reached." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "ERROR: Insufficient memory." << std::endl;
			break;
		default:
			std::cerr << "ERROR: An unexpected error occurred." << std::endl;
		}
		exit(1);
	}

	// wait until semaphore is unlocked
	int ret = sem_wait(lock);
	if (ret == -1) {
		std::cerr << "ERROR: An unexpected error occurred." << std::endl;
		exit(1);
	}
}

void Lock::unlock() {
	// unlock semaphore (semaphore is automatically closed on process termination)
	if (sem_post(lock) == -1) {
		std::cerr << "ERROR: An unexpected error occurred." << std::endl;
		exit(1);
	}
}
