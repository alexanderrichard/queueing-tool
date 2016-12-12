#!/usr/bin/python2.7

import subprocess
import threading
import os
import signal
import datetime
import random
import argparse
import socket
from block_parser import Block_Parser


### JOB ###
class Job(object):

    def __init__(self, requests, job_id, subtask_id, depends_on, user):
        # requests information (name, requested resources, subtasks, script)
        self.requests = requests
        # job name
        self.name = self.requests['name']
        if self.requests['subtasks'] > 1:
            self.name = self.name + '.' + str(subtask_id+1)
        self.user = user
        # job identifier
        self.job_id = job_id
        self.subtask_id = subtask_id
        # dependencies on other jobs (do not start before all dependencies have finished)
        self.depends_on = depends_on
        # address (port changes on creation of the socket)
        self.address = ('localhost', 0)
        # job statistics
        self.time = datetime.datetime.now()
        # assigned gpu id's if gpu is requested
        self.gpu_ids = [-1] # initially, no gpu is assigned
### END JOB ###


### EXECUTABLE JOB ###
class Executable_Job(Job):

    def __init__(self, requests, job_id, subtask_id, depends_on, user, server_address):
        super( Executable_Job, self ).__init__(requests, job_id, subtask_id, depends_on, user)
        self.is_running = False
        # set up a socket and store address
        self.listener = socket.socket( socket.AF_INET,  socket.SOCK_STREAM )
        self.listener.bind( self.address )
        self.address = self.listener.getsockname()
        self.listener.listen(1)
        # the server address
        self.server_address = server_address
        self.qlog = None


    def write_log(self, msg):
        if self.qlog == None:
            print msg
        else:
            self.qlog.write(msg + '\n')
            self.qlog.flush()


    def initialize_qlog(self):
        # open file to write qlog information, stdout, and stderr to (None: write to stdout/stderr)
        try:
            if not os.path.exists('q.log'):
                os.makedirs('q.log')
            self.qlog = open('q.log/' + self.name + '.' + str(self.job_id).zfill(7), 'w')
        except:
            self.qlog = None
        self.write_log('---------------- start ----------------')
        self.write_log('%s' % (self.time.strftime('%a %b %d %Y %H:%M:%S')) )
        self.write_log('max threads: %d' % self.requests['threads'])
        self.write_log('max memory: %d MB' % self.requests['memory'])
        self.write_log('time limit: %d hours' % self.requests['hours'])
        if self.requests['gpus'] > 0:
            self.write_log('gpu id(s): '+ ( ','.join( [str(i) for i in self.gpu_ids] )) )
        else:
            self.write_log('use gpu: false')
        self.write_log('---------------------------------------')


    def timeout(self):
        # notify server
        msg = 'timeout:' + str(self.job_id)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(self.server_address)
        sock.sendall(msg)
        sock.close()


    def finalize(self):
        # remove temporal script file
        try:
            if os.path.isfile(self.tmp_script_file):
                os.remove(self.tmp_script_file)
        except:
            pass
        # finalize qlog
        now = datetime.datetime.now()
        elapsed = now - self.time
        self.write_log('----------------- end -----------------')
        self.write_log('%s' % (now.strftime('%a %b %d %Y %H:%M:%S')) )
        self.write_log('elapsed time: %s:%s:%s' % ( str(elapsed.days * 24 + (elapsed.seconds / 3600)).zfill(3),
                                                    str((elapsed.seconds / 60) % 60).zfill(2),
                                                    str(elapsed.seconds % 60).zfill(2) ))
        self.write_log('---------------------------------------')
        if not self.qlog == None:
            self.qlog.close()
            self.qlog = None


    def run(self, script_params = []):
        # wait for run notification by server and receive gpu_ids
        msg = self.wait_for_message(os.getpid())
        msg = msg.split(':')
        if (len(msg) > 1) and not (msg[1] == ''):
            self.gpu_ids = [ int(i) for i in msg[1].split(',') ]
        self.is_running = True
        self.time = datetime.datetime.now()
        self.initialize_qlog()
        # log params
        if len(script_params) > 0:
            self.write_log('passed parameters: ' + ' '.join(script_params))
        # create script file
        self.tmp_script_file = '.' + self.name + '.' + str(self.job_id)
        with open(self.tmp_script_file, 'w') as f:
            f.write('#!/bin/bash\n')
            f.write('export N_SUBTASKS=' + str(self.requests['subtasks']) + '\n')
            f.write('export SUBTASK_ID=' + str(self.subtask_id+1) + '\n')
            f.write('\n'.join(self.requests['script']))
            f.close()
        cmd = ''
        # memory limit
        if self.requests['gpus'] == 0: # ulimit does not work in combination with GPUs due to unified memory
            cmd = 'ulimit -v ' + str(self.requests['memory'] * 1024) + '; '
        # omp threads
        cmd = cmd + 'export OMP_NUM_THREADS=' + str(self.requests['threads']) + '; '
        # cuda visible devices
        cmd = cmd + 'export CUDA_VISIBLE_DEVICES=' + ','.join([str(i) for i in self.gpu_ids]) + '; '
        # script
        cmd = cmd + 'bash ' + self.tmp_script_file + ' ' + ' '.join(script_params)
        # send finalize message via tcp
        cmd = cmd + '; bash -c "echo finished:' + str(self.job_id) \
            + ' > /dev/tcp/' + self.server_address[0] + '/' + str(self.server_address[1]) + '"'
        # run job without waiting
        proc = subprocess.Popen(args=cmd, stdout=self.qlog, stderr=self.qlog, shell=True, preexec_fn=os.setsid)
        timer = threading.Timer(self.requests['hours'] * 3600, self.timeout, args=[proc.pid])
        timer.start()
        self.wait_for_message(proc.pid)
        timer.cancel()
        self.finalize()


    def wait_for_message(self, pid):
        while True:
            connection, from_address = self.listener.accept()
            received = connection.recv(1024)
            if (received == 'delete') or (received == 'timeout'):
                if self.is_running:
                    self.write_log('\nJOB KILLED: %s\n' % received)
                    os.killpg(os.getpgid(pid), signal.SIGKILL)
                    return
                else:
                    exit()
            elif (received == 'finished') or (received[0:3] == 'run'):
                connection.close()
                return received
            connection.close()


    def register(self):
        msg = 'request:' + str(self.job_id) \
                + ',' + self.address[0] \
                + ',' + str(self.address[1]) \
                + ',' + self.name \
                + ',' + str(self.requests['threads']) \
                + ',' + str(self.requests['memory']) \
                + ',' + str(self.requests['gpus']) \
                + ',' + str(self.requests['hours']) \
                + ',' + self.user \
                + ',' + '+'.join( [str(i) for i in self.depends_on] )
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10.0)
            sock.connect(self.server_address)
            sock.sendall(msg)
            # wait for answer
            reply = sock.recv(1024)
            sock.close()
        except:
            print 'register job: no answer from server'
            exit()
        # if declined print decline message and exit
        if not reply == 'accept':
            print 'JOB DECLINED.'
            print reply
            exit()


### END EXECUTABLE JOB ###


def main():

    # parse arguments
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument('script', type=str, nargs='+',
                            help='script (plus its parameters) to be submitted. Script parameters must not start with \'-\'. If they'
                                  ' do so, pass them with quotation marks and a leading space, e.g. " --foo".')
    arg_parser.add_argument('--server_ip', type=str, default='localhost', help='ip address of the server')
    arg_parser.add_argument('--server_port', type=int, default=1234, help='port of the server')
    arg_parser.add_argument('--block_index', type=int, default=0, help='index of the block to execute')
    arg_parser.add_argument('--job_id', type=int, default=0, help='unique id of this job')
    arg_parser.add_argument('--subtask_id', type=int, default=1, help='subtask id for the selected block')
    arg_parser.add_argument('--depends_on', type=str, default='', help='comma separated list of job ids that need to be finished before this job starts')
    arg_parser.add_argument('--user', type=str, default='dummy', help='user who submitted the job')
    args = arg_parser.parse_args()

    script = args.script[0]
    script_params = args.script[1:]
    server_address = (args.server_ip, args.server_port)
    if args.depends_on == '':
        depends_on = []
    else:
        depends_on = [ int(i) for i in args.depends_on.split(',') ]

    # parse blocks
    parser = Block_Parser()
    blocks = parser.parse(script)
    assert len(blocks) > args.block_index

    job = Executable_Job( blocks[args.block_index].values, args.job_id, args.subtask_id, depends_on, args.user, server_address)
    job.register()
    job.run(script_params)

if __name__ == '__main__':
    main()
