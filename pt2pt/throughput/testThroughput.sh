MESSAGES=100000
RIPETIZIONI=1
MACCHINE=(localhost localhost)

trap "echo Exited!; exit;" SIGINT SIGTERM

#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS

echoerr() { printf "%s\n" "$*" >&2; }

echoerr "Machine used: $mpi_machines_list"

## reindirizza std error to dev null to suppress error messages
exec 2>/dev/null

printf "App;Messages;MessageSize;Time\n"
for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N 2 --exclusive --cpus-per-task=36 --export=UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputFF $MESSAGES $messageSize --DFF_Config=$(pwd)/dff.json | tail -1
        else
            mpirun -H $mpi_machines_list  -np 2 --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputFF $MESSAGES $messageSize --DFF_Config=$(pwd)/dff.json | tail -1
        fi
    done
done

for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N 2 --exclusive --cpus-per-task=36 --export=UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputMTCL $MESSAGES $messageSize | tail -1
        else
            mpirun -H $mpi_machines_list  -np 2 --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputMTCL $MESSAGES $messageSize | tail -1
        fi
    done
done

for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N 2 --exclusive --cpus-per-task=36 --export=UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputMPI $MESSAGES $messageSize | tail -1
        else
            mpirun -H $mpi_machines_list  -np 2 --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/ThroughputMPI $MESSAGES $messageSize | tail -1
        fi
    done
done