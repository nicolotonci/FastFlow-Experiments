MESSAGES=100
RIPETIZIONI=2
MACCHINE=(localhost localhost localhost)
CONFIG_FILENAME=parametricPerf_generated.json
PRODUCER_TIME=5 #milliseconds
CONSUMER_TIME=5 #milliseconds
PRODUCER_GROUPS=1
CONSUMER_GROUPS=1
PRODUCER_PER_GROUP=1
CONSUMERS_PER_GROUP=1
BATCH_SIZE=1
BATCH_BYTE_SIZE=2M 

let total_groups=PRODUCER_GROUPS+CONSUMER_GROUPS

echoerr() { printf "%s\n" "$*" >&2; }

generaFileConfig() {
    echoerr $1 $2 $3 $4 $5 $6

    local fileName=$1
    local groupsSx=$2
    local groupsDx=$3
    local nome_array=$4
    local machines=("${!nome_array}")
    local batchSize
    local batchByteSize

    if [ -z ${5+x} ]; then batchSize=1; else batchSize=$5; fi
    if [ -z ${6+x} ]; then batchByteSize=1; else batchByteSize=$6; fi

    rm $fileName

    echo "{
    \"protocol\" : \"MPI\",
    \"groups\" : [" >> $fileName

    # printing the dx nodes in the configuration file
    for (( i=0; i<$groupsSx; i++))
    do
        local machine="${machines[$i]}"

        echo "        {
            \"endpoint\" : \"$machine:$((8000 + $i))\",
            \"name\" : \"S${i}\",
            \"batchSize\" : ${batchSize},
            \"batchByteSize\" : \"$batchByteSize\"
        }
        ," >> $fileName
    done

    # printing the dx nodes in the configuration file
    for (( i=0; i<$groupsDx; i++))
    do
        local machine="${machines[$(($i+$groupsSx))]}"
        echo "        {
            \"endpoint\" : \"$machine:$((8000 + $groupsSx + $i))\",
            \"name\" : \"D${i}\"
        }" >> $fileName
        if [[ $i -lt ${3}-1 ]]; then
            echo "     ," >> $fileName
        fi
    done

    echo "    ]
}" >> $fileName
}

generaFileConfig $CONFIG_FILENAME $PRODUCER_GROUPS $CONSUMER_GROUPS MACCHINE[@] $BATCH_SIZE $BATCH_BYTE_SIZE

#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS


echoerr "Machine used: $mpi_machines_list"



## reindirizza std error to dev null to suppress error messages
#exec 2>/dev/null

printf "Messages;MessageSize;ProducerGroups;ConsumerGroups;ProducerPerGroup;ConsumerPerGroup;Ondemand;Time\n"
for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    values=()
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N $total_groups --exclusive --cpus-per-task=36 --export=ALL,UCX_ZCOPY_THRESH=2M $(pwd)/parametricPerf $MESSAGES $messageSize $PRODUCER_TIME $CONSUMER_TIME $PRODUCER_GROUPS $CONSUMER_GROUPS $PRODUCER_PER_GROUP $CONSUMERS_PER_GROUP  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
        else
            mpirun -H $mpi_machines_list -np $total_groups --bind-to none -x UCX_ZCOPY_THRESH=2M $(pwd)/parametricPerf $MESSAGES $messageSize $PRODUCER_TIME $CONSUMER_TIME $PRODUCER_GROUPS $CONSUMER_GROUPS $PRODUCER_PER_GROUP $CONSUMERS_PER_GROUP  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
        fi
    done
done