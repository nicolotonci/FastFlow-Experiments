MESSAGES=1000
messageSize=64
RIPETIZIONI=1
MACCHINE=(localhost localhost localhost)
CONFIG_FILENAME=parametricPerfHLPP_generated.json
PRODUCER_TIME=0 #microseconds
CONSUMER_TIME=0 #microseconds
total_groups=2
CONSUMERS_PER_GROUP=2
BATCH_SIZE=1
BATCH_BYTE_SIZE=32k 


echoerr() { printf "%s\n" "$*" >&2; }

generaFileConfig() {
    echoerr $1 $2 $3 $4 $5

    local fileName=$1
    local groups=$2
    local nome_array=$3
    local machines=("${!nome_array}")
    local batchSize
    local batchByteSize

    if [ -z ${4+x} ]; then batchSize=1; else batchSize=$4; fi
    if [ -z ${5+x} ]; then batchByteSize=1; else batchByteSize=$5; fi

    rm $fileName

    echo "{
    \"protocol\" : \"MPI\",
    \"groups\" : [" >> $fileName

    # printing the dx nodes in the configuration file
    for (( i=0; i<$groups; i++))
    do
        local machine="${machines[$i]}"

        echo "        {
            \"endpoint\" : \"$machine:$((8000 + $i))\",
            \"name\" : \"G${i}\",
            \"batchSize\" : ${batchSize},
            \"batchByteSize\" : \"$batchByteSize\",
            \"messageOTF\" : 2
        }" >> $fileName
        if [[ $i -lt ${2}-1 ]]; then
            echo "     ," >> $fileName
        fi
    done

    echo "    ]
}" >> $fileName
}

generaFileConfig $CONFIG_FILENAME $total_groups MACCHINE[@] $BATCH_SIZE $BATCH_BYTE_SIZE

#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS


echoerr "Machine used: $mpi_machines_list"


## reindirizza std error to dev null to suppress error messages
#exec 2>/dev/null

printf "Messages;MessageSize;Groups;ConsumerPerGroup;Ondemand;Time\n"
for ((CONSUMER_TIME=100; CONSUMER_TIME<=6400; CONSUMER_TIME*=2)); do
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N $total_groups --exclusive --cpus-per-task=36 --export=ALL,UCX_ZCOPY_THRESH=2M $(pwd)/parametricPerfHLPP $MESSAGES $messageSize $PRODUCER_TIME $CONSUMER_TIME $total_groups  $PRODUCER_PER_GROUP $CONSUMERS_PER_GROUP 1  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
        else
            mpirun -H $mpi_machines_list -np $total_groups --bind-to none -x UCX_ZCOPY_THRESH=2M gdb -ex run -ex bt --args $(pwd)/parametricPerfHLPP $MESSAGES $messageSize $PRODUCER_TIME $CONSUMER_TIME $total_groups $CONSUMERS_PER_GROUP 1 --DFF_Config=$(pwd)/$CONFIG_FILENAME #| tail -1
        fi
    done
done