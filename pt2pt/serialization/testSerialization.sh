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
for ((messageSize=1; messageSize<=131072; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        mpirun -np 2 --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/auto $MESSAGES $messageSize --DFF_Config=$(pwd)/dff.json | tail -1
    done
done
for ((messageSize=1; messageSize<=131072; messageSize*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        mpirun -np 2 --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/auto $MESSAGES $messageSize --DFF_Config=$(pwd)/dff.json | tail -1
    done
done