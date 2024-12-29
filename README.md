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



# OPFTL: Order-Preserving Flash Translation Layer

OPFTL is a Flash Translation Layer designed to work with the Order-Preserving IO Stack (OPIMQ) to ensure storage order in multi-queue block devices. It enhances storage performance while maintaining strict storage order guarantees.

---

## Features

- **Persistence Order Guarantee:** Ensures the storage order across multiple command queues by updating mapping tables in a controlled sequence.
- **Sibling-Aware Delayed Mapping:** Manages dual-stream writes by maintaining delayed mapping entries to ensure inter-stream dependencies.
- **Crash Consistency:** Provides robust crash recovery by leveraging backup metadata stored in SSD's reserved flash memory.

---

## Key Components

### Epoch States
1. **Active:** Epoch is created when the first command arrives.
2. **Closed:** Epoch boundary is set upon receiving a cache barrier command.
3. **Durable:** Data blocks of the epoch are durable in the storage device.
4. **Mapped:** Physical locations of all data blocks are updated in the mapping table.

### Delayed Mapping
- Handles writes that cannot be immediately updated in the mapping table.
- Uses sibling references to coordinate updates for dual-stream writes.

### Dual-Stream Write
- Allows a write request to belong to two streams (major and minor) simultaneously to ensure inter-stream dependencies.

---

## Advantages

- **High Performance:** Achieves near-optimal throughput by decoupling flush operations from persistence order enforcement.
- **Parallelism Utilization:** Supports multiple streams and epochs to leverage the hardware parallelism of modern SSDs.
- **Crash Recovery:** Maintains storage consistency without requiring modifications to existing file systems or applications.

---

## Integration with OPIMQ

OPFTL works in conjunction with OPIMQ to preserve IO order:
- Implements storage order by using stream and epoch IDs.
- Supports epoch pinning and dual-stream write mechanisms for intra-stream and inter-stream order guarantees.

---

## Performance Evaluation

- **Overhead:** OPFTL incurs minimal performance overhead (1.05%) compared to traditional page-mapping FTLs.
- **Scalability:** Supports multiple concurrent streams without significant impact on performance or memory footprint.
- **Crash Consistency:** Verified with 100% success in CrashMonkey tests.

---

## Use Cases

OPFTL is ideal for:
- **File Systems:** Ensuring data durability and order for journaling systems (e.g., OP-EXT4).
- **Database Workloads:** Providing ordered persistence for transactions with inter-thread dependencies.
- **Cloud Environments:** Enhancing scalability for containerized applications requiring frequent synchronization (e.g., `fsync()`).

---

## References

For more details, refer to the paper:  
*OPIMQ: Order Preserving IO Stack for Multi-Queue Block Device*


