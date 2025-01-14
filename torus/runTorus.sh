RIPETIZIONI=2
N_TICKS=0
MACCHINE=(localhost localhost localhost localhost)
CONFIG_FILENAME=torus_generated.json
GROUPS_MAX=8 # must be power of 2 # NUMBER OF STAGES
BATCH_SIZE=1
BATCH_BYTE_SIZE=2M 

echoerr() { printf "%s\n" "$*" >&2; }

generaFileConfig() {
    #echoerr $1 $2 $3 $4 $5 $6

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


#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS

echoerr "Machine used: $mpi_machines_list"

## reindirizza std error to dev null to suppress error messages
exec 2>/dev/null

printf "Stages;Tasks;Time;Msg/s\n"
for((GROUPS_TOT=2; GROUPS_TOT<$GROUPS_MAX; GROUPS_TOT*=2)); do
    generaFileConfig $CONFIG_FILENAME $GROUPS_TOT MACCHINE[@] $BATCH_SIZE $BATCH_BYTE_SIZE
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
        if [ "$USE_SLURM" -eq 1 ]; then
            srun -N $GROUPS_TOT --exclusive --cpus-per-task=36 --export=ALL,UCX_ZCOPY_THRESH=2M $(pwd)/torus $GROUPS_TOT $N_TICKS  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
        else
            mpirun -H $mpi_machines_list  -np $GROUPS_TOT $(pwd)/torus $GROUPS_TOT $N_TICKS  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -1
        fi
    done
done