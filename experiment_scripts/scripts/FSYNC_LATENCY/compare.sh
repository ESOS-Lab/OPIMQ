#!/bin/bash

# Define file paths
file_ext4="LOGS/EXT4/output"
file_opext4="LOGS/OPEXT4/output"

# Read EXT4 data and store in variables
total_ext4=$(grep "Total fsync latency" "$file_ext4" | awk -F: '{print $2}' | xargs)
tdma_thost_ext4=$(grep "TDMA + THOST" "$file_ext4" | awk -F: '{print $2}' | xargs)
tflush_ext4=$(grep "TFlush" "$file_ext4" | awk -F: '{print $2}' | xargs)
tfua_ext4=$(grep "TFUA" "$file_ext4" | awk -F: '{print $2}' | xargs)
if [ -z "$tfua_ext4" ]; then
    tfua_ext4=0
fi

# Read OPEXT4 data and store in variables
total_opext4=$(grep "Total fsync latency" "$file_opext4" | awk -F: '{print $2}' | xargs)
tdma_thost_opext4=$(grep "TDMA + THOST" "$file_opext4" | awk -F: '{print $2}' | xargs)
tflush_opext4=$(grep "TFlush" "$file_opext4" | awk -F: '{print $2}' | xargs)
#tfua_opext4=$(grep "TFUA" "$file_opext4" | awk -F: '{print $2}' | xargs)
tfua_opext4=0

# Print results
echo "Comparison of fsync latency"
echo "================================"
echo "EXT4 Result:"
echo "  Total fsync latency: $total_ext4 msec"
echo "  TDMA + THOST: $tdma_thost_ext4 msec"
echo "  TFlush: $tflush_ext4 msec"
echo "  TFUA: $tfua_ext4 msec"
echo

echo "OPEXT4 Result:"
echo "  Total fsync latency: $total_opext4 msec"
echo "  TDMA + THOST: $tdma_thost_opext4 msec"
echo "  TFlush: $tflush_opext4 msec"
echo "  TFUA: $tfua_opext4 msec"
echo

echo "Summary of Total fsync latency:"
echo "  EXT4: $total_ext4 msec"
echo "  OPEXT4: $total_opext4 msec"

# Calculate percentage reduction using awk
reduction=$(awk -v ext4="$total_ext4" -v opext4="$total_opext4" 'BEGIN { if (ext4 > 0) printf "%.2f", ((ext4 - opext4) / ext4) * 100; else print "0" }')
echo "Percentage reduction in total fsync latency from EXT4 to OPEXT4: $reduction%"

