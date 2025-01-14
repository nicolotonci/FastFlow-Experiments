MESSAGES=100
RIPETIZIONI=2
MACCHINE=(localhost localhost)
CONFIG_FILENAME=parametricPerfH_generated.json
PRODUCER_TIME=5 #milliseconds
CONSUMER_TIME=5 #milliseconds
GROUPS_TOT=2 # minimum 2
PRODUCER_PER_GROUP=1 #minimum 1
CONSUMERS_PER_GROUP=1 #minimum 1
BATCH_SIZE=1
BATCH_BYTE_SIZE=2M 

echoerr() { printf "%s\n" "$*" >&2; }

generaFileConfig() {
    echoerr $1 $2 $3 $4 $5 $6

    local fileName=$1
    local groupsTot=$2
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
    for (( i=0; i<$groupsTot; i++))
    do
        local machine="${machines[$i]}"

        echo "        {
            \"endpoint\" : \"$machine:$((8000 + $i))\",
            \"name\" : \"G${i}\",
            \"batchSize\" : ${batchSize},
            \"batchByteSize\" : \"$batchByteSize\"
        }
        " >> $fileName
        if [[ $i -lt ${groupsTot}-1 ]]; then
            echo "     ," >> $fileName
        fi
    done

    echo "    ]
}" >> $fileName
}

generaFileConfig $CONFIG_FILENAME $GROUPS_TOT MACCHINE[@] $BATCH_SIZE $BATCH_BYTE_SIZE

#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS


echoerr "Machine used: $mpi_machines_list"

## reindirizza std error to dev null to suppress error messages
exec 2>/dev/null

printf "Messages;MessageSize;ProducerGroups;ConsumerGroups;ProducerPerGroup;ConsumerPerGroup;Ondemand;Time\n"
for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    values=()
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
       mpirun -H $mpi_machines_list  -np 2 $(pwd)/parametricPerfH $MESSAGES $messageSize $PRODUCER_TIME $CONSUMER_TIME $GROUPS_TOT $PRODUCER_PER_GROUP $CONSUMERS_PER_GROUP  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
    done
done