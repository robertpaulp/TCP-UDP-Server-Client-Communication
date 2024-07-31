#!/bin/bash

# Find the PID of the process listening on port 12345
pid=$(netstat -npln | grep ':12345 ' | awk '{print $7}' | cut -d'/' -f1)

if [ -n "$pid" ]; then
    # Kill the process with the found PID
    echo "Killing process with PID: $pid"
    kill "$pid"
else
    echo "No process found listening on port 12345"
fi

