MESSAGES=100
RIPETIZIONI=2
GROUPS_WORKER=2 # minimum 2
GROUPS_TOT=$(($GROUPS_WORKER + 2))
THREADS_WORKER=1
BATCH_SIZE=1

printf "Config;Messages;Size;BatchSize;WorkerProcess;Threads;Time\n"
for ((messageSize=4; messageSize<=1048576; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        srun -N $GROUPS_WORKER -n $GROUPS_TOT --cpu-bind=none --export=ALL,UCX_ZCOPY_THRESH=2M $(pwd)/ff-farm $MESSAGES $messageSize $BATCH_SIZE $THREADS_WORKER --DFF_Config=$(pwd)/dff.json | tail -1 
    done
done

for ((messageSize=4; messageSize<=1048576; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        srun -N $GROUPS_WORKER -n $GROUPS_TOT --cpu-bind=none --export=ALL,OMP_PLACES={0:$THREADS_WORKER} $(pwd)/mpi-farm $MESSAGES $messageSize $BATCH_SIZE $THREADS_WORKER | tail -1 
    done
done