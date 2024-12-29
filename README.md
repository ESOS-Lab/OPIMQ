# OPIMQ: Order-Preserving IO Stack for Multi-Queue Block Devices

## Overview
OPIMQ is an order-preserving IO stack designed to enhance the performance of multi-queue block devices. It eliminates the inefficiencies of conventional order-preservation methods like transfer-and-flush while maximizing parallelism and maintaining storage order in multi-queue environments.

### Key Features:
- **Stream and Epoch IDs:** Groups I/O requests with order dependencies into streams and epochs.
- **Epoch Pinning:** Prevents order violations due to thread migrations in multi-queue devices.
- **Dual-Stream Write:** Ensures inter-stream order dependencies with minimal overhead.


---

## Design Goals
1. **Order Preservation Without Overhead:** Replace transfer-and-flush with lightweight mechanisms like cache barrier commands.
2. **Parallelism Maximization:** Support multiple streams to leverage multi-core and multi-queue architectures.
3. **Multi-Queue Device Compatibility:** Ensure both intra-stream and inter-stream order in devices with multiple submission and completion queues.

---

## Installation and Usage

### Requirements
- **Linux Kernel:** Version 5.18.18 or higher.
- **Platform:** Compatible with NVMe SSDs

### Build and Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/[your-repo]/opimq.git
2. Build the kernel:

### Mounting with OP-EXT4
1. Format the device with the OP-EXT4 filesystem:
   ```bash
   mkfs -t ext4 [your-nvme-device]
   ```
2. Mount the device:
   ```bash
   mount -t ext4 [your-nvme-device] [target directory]

