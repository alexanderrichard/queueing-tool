/*
 * Lock.hh
 *
 *  Created on: Jul 22, 2014
 *      Author: richard
 */

#ifndef LOCK_HH_
#define LOCK_HH_

#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

class Lock
{
private:
	static sem_t* lock;
public:
	static void waitForLock();
	static void unlock();
};

#endif /* LOCK_HH_ */
