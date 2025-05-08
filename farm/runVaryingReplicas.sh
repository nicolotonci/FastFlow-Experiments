MESSAGES=100
messageSize=2048
RIPETIZIONI=2
THREADS_WORKER=1
BATCH_SIZE=1

printf "Config;Messages;Size;BatchSize;WorkerProcess;Threads;Time\n"
for ((GROUPS_WORKER=1; GROUPS_WORKER<=64; GROUPS_WORKER*=2)); do
    GROUPS_TOT=$(($GROUPS_WORKER + 2))
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        srun -N $GROUPS_WORKER -n $GROUPS_TOT --cpu-bind=none --export=ALL,UCX_ZCOPY_THRESH=2M $(pwd)/ff-farm $MESSAGES $messageSize $BATCH_SIZE $THREADS_WORKER --DFF_Config=$(pwd)/dff.json | tail -1 
    done
done

for ((GROUPS_WORKER=4; GROUPS_WORKER<=64; GROUPS_WORKER*=2)); do
    GROUPS_TOT=$(($GROUPS_WORKER + 2))
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        srun -N $GROUPS_WORKER -n $GROUPS_TOT --cpu-bind=none --export=ALL,OMP_PLACES={0:$THREADS_WORKER} $(pwd)/mpi-farm $MESSAGES $messageSize $BATCH_SIZE $THREADS_WORKER | tail -1 
    done
done