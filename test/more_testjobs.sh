#block(name=[test], threads=2, memory=100, subtasks=1, gpus=1, hours=12)
date
echo $CUDA_VISIBLE_DEVICES
sleep 10
date
#block(name=[test], threads=2, memory=100, subtasks=1, gpus=2, hours=12)
date
echo $CUDA_VISIBLE_DEVICES
sleep 7
date
#block(name=[test], threads=2, memory=100, subtasks=1, gpus=4, hours=12)
date
echo $CUDA_VISIBLE_DEVICES
sleep 20
date
#block(name=[test], threads=2, memory=100, subtasks=1, gpus=3, hours=12)
date
echo $CUDA_VISIBLE_DEVICES
sleep 5
date