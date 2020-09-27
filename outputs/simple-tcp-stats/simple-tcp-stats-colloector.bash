#!/bin/bash

for ((DELAY=10; DELAY<=10000; DELAY+=1000))
do
  for ((BW=10; BW<=100; BW+=10))
  do
    ACCESS_BW=$((2*BW))
    BW_UNIT=Mbps
    DELAY_UNIT=us
    ./waf --cwd=./outputs/simple-tcp-stats/ --run "simple-tcp-stats --tracing=true --num_flows=50 --duration=10.0 --flow_monitor=true --link_delay=$DELAY$DELAY_UNIT --bandwidth=$BW$BW_UNIT --access_bandwidth=$ACCESS_BW$BW_UNIT"
  done
done
