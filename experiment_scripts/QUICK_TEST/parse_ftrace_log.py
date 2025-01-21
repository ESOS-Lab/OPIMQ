import sys
import re
import csv

# Check for command-line argument
if len(sys.argv) != 2:
    print("Usage: python parse_nvme_logs.py <input_file>")
    sys.exit(1)

input_file = sys.argv[1]
output_file = './LOGS/parsed_data'

# Regular expression to parse log data
pattern = re.compile(
    r'^\s*(\S+)\s+\[\d+\]\s+\S+\s+([\d.]+):\s+nvme_setup_cmd:'
    r'\s+cmd_type:(\S+)\s+sid1:\s*(\d+)\s+eid1:\s*(\d+)\s+'
    r'sid2:\s*(\d+)\s+eid2:\s*(\d+)\s+bflag:\s*(\d+)\s+qid:\s*(\d+)'
)

# List to store parsed results
parsed_data = []

# Read and parse the input file
with open(input_file, 'r') as f:
    for line in f:
        match = pattern.match(line)
        if match:
            process_name = match.group(1)
            process_parts = process_name.rsplit('-', 1)
            process_name, process_id = process_parts if len(process_parts) == 2 and process_parts[1].isdigit() else (process_name, 'N/A')
            
            # Modify PName based on inclusion of 'jbd2' or 'kworker'
            if 'jbd2' in process_name:
                process_name = 'jbd2'
            elif 'kworker' in process_name:
                process_name = 'kworker'

            parsed_data.append([
                match.group(2),  # Timestamp
                process_name,    # PName
                process_id,      # PID
                f"[{match.group(4)}, {match.group(5)}]",  # [sid1, eid1]
                f"[{match.group(6)}, {match.group(7)}]",  # [sid2, eid2]
                match.group(8),  # bflag
                match.group(9),  # qid
                match.group(3),  # optype (cmd_type)
            ])

# Write the parsed results to a TSV file
with open(output_file, 'w', newline='') as f:
    writer = csv.writer(f, delimiter='\t')
    writer.writerow(['Timestamp', 'PName', 'PID', '[sid1, eid1]', '[sid2, eid2]', 'bflag', 'qid', 'optype'])
    writer.writerows(parsed_data)

print(f"Parsing complete! Parsed data saved to {output_file}")
