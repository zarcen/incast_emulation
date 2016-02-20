#!/bin/bash

# cexec.sh - (Cluster) EXECute a command on all nodes
# Usage: ./cexec.sh "sudo ~/essential_install.sh"

if [ -z $1 ]; then
    echo "cexec.sh - (Cluster) EXECute a command on all nodes"
    echo "Usage: ./cexec.sh \"sudo ~/essential_install.sh\""
    exit 0
fi

cmd=$1
serverlist='node1 node2 node3 node4 node5 node6 node7 node8'
declare -A bg_pids
for node in $serverlist; do
    echo "executing '$1' on $node"
    ssh $node $cmd &
    bg_pids[$node]=$!
done

#declare -p bg_pids
for node in ${!bg_pids[@]}; do
    while ps | grep ${bg_pids["$node"]} > /dev/null
    do
        #echo $node is still in the ps output. Must still be running.
        sleep 1
    done
    wait ${bg_pids["$node"]}
    ps_status=$?
    if [[ $ps_status = 0 ]]; 
    then 
        echo "$node done successfully" 
    else
        echo Error: The exit status of $node was $ps_status 
    fi
done
