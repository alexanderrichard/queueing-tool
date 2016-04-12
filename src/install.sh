#!/bin/bash

make

rm -rf objects/
mkdir -p /usr/queue
chmod 777 /usr/queue
cp ./q* /usr/queue/
cp ./_* /usr/queue/
cp ./scheduler /usr/queue
cp ./exec_local /usr/queue

echo "threads 10" > /usr/queue/queue.config
echo "memory 30000" >> /usr/queue/queue.config
echo "gpus 1" >> /usr/queue/queue.config
echo "abortOnTimeLimit true" >> /usr/queue/queue.config
echo "regeneration-factor 0.05" >> /usr/queue/queue.config
echo "max-priority-class 10" >> /usr/queue/queue.config

echo "0" > /usr/queue/.queue
echo "0" >> /usr/queue/.queue
echo "0" >> /usr/queue/.queue
echo "0" >> /usr/queue/.queue
echo "0" >> /usr/queue/.queue

echo "0" > /usr/queue/.priorities

/usr/queue/qreset
chmod 777 /usr/queue/.queue
chmod 777 /usr/queue/.priorities
