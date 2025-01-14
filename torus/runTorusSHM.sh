RIPETIZIONI=2
N_TICKS=0
GROUPS_MAX=8 # must be power of 2 # NUMBER OF STAGES

printf "Stages(SHM);Tasks;Time;Msg/s\n"
for((GROUPS_TOT=2; GROUPS_TOT<=$GROUPS_MAX; GROUPS_TOT*=2)); do
    for((ripetizione=0; ripetizione<=$RIPETIZIONI; ripetizione+=1)); do
            $(pwd)/torusSHM $GROUPS_TOT $N_TICKS | tail -1
    done
done