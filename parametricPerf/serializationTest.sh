MESSAGES=10000
RIPETIZIONI=2
MACCHINE=(localhost localhost)
CONFIG_FILENAME=serializationConfigExp.json

generaFileConfig() {
    echo $1 $2 $3 $4 $5

    local fileName=$1
    local groupsSx=$2
    local groupsDx=$3
    local nome_array=$4
    local machines=("${!nome_array}")
    local batchSize

    if [ -z ${5+x} ]; then batchSize=1; else batchSize=$5; fi

    rm $fileName

    echo "{
    \"groups\" : [" >> $fileName

    # printing the dx nodes in the configuration file
    for (( i=0; i<$groupsSx; i++))
    do
        local machine="${machines[$i]}"

        echo "        {
            \"endpoint\" : \"$machine:$((8000 + $i))\",
            \"name\" : \"S${i}\",
            \"batchSize\" : ${batchSize}
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

generaFileConfig $CONFIG_FILENAME 1 1 MACCHINE[@] 100

#generate the list of machines separated by comma
IFS=','       # Imposta IFS su ","
mpi_machines_list="${MACCHINE[*]}"
unset IFS

echo "Machine used: $mpi_machines_list"


## reindirizza std error to dev null to suppress error messages
exec 2>/dev/null

printf "%-12s %-10s %-10s %-16s\n" "MessageSize" "Time (ms)" "Std Dev" "Throughput (Mb/s)"
for ((messageSize=2; messageSize<=1048576; messageSize*=2)); do
    values=()
    for((ripetizione=0; ripetizione<$RIPETIZIONI; ripetizione+=1)); do
       values+=($(mpirun -H $mpi_machines_list  -np 2 $(pwd)/parametricPerf $MESSAGES $messageSize 0 0 1 1 1 1  --DFF_Config=$(pwd)/$CONFIG_FILENAME | tail -n 1 | awk -F'= ' '{print $2}'))
        #echo "mpirun -H $mpi_machines_list  -np 2 $(pwd)/test_parametricPerf $MESSAGES $messageSize 0 0 1 1 1 1  --DFF_Config=$(pwd)/$CONFIG_FILENAME"
    done

    # Calcola la media
    mean=$(echo "${values[@]}" | awk '{sum=0; for(i=1; i<=NF; i++) sum+=$i; print sum/NF}')

    # Calcola la somma dei quadrati delle differenze dalla media
    sum_squared_diff=$(echo "${values[@]}" | awk -v mean="$mean" '{sum=0; for(i=1; i<=NF; i++) sum+=($i-mean)^2; print sum}')

    # Calcola la deviazione standard
    std_dev=$(echo "scale=4; sqrt($sum_squared_diff / $RIPETIZIONI)" | bc)

    throughput=$(echo "scale=4; (($MESSAGES * $messageSize) / $mean) / 1000" | bc)

    printf "%-12s %-10s %-10s %-16s\n" "$messageSize"  "$mean" "$std_dev" "$throughput"
done