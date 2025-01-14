RIPETIZIONI=2
N_TICKS=0
MACCHINE=(localhost localhost localhost localhost)
CONFIG_FILENAME=torus_generated.json
GROUPS_TOT=4 # minimum 2 # NUMBER OF STAGES
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

printf "Stages;Tasks;Time;Msg/s\n"

for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
    mpirun -H $mpi_machines_list  -np $GROUPS_TOT $(pwd)/torus $GROUPS_TOT $N_TICKS  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
done