# OPIMQ: Order-Preserving IO Stack for Multi-Queue Block Devices
### Maintainer: Jieun Kim (pipiato@kaist.ac.kr)

OPIMQ is an order-preserving IO stack designed to enhance the performance of multi-queue block devices. It eliminates the inefficiencies of conventional order-preservation methods like transfer-and-flush while maximizing parallelism and maintaining storage order in multi-queue environments.

## Publication
This artifact is part of the research presented in the following publication:
- **OPIMQ: Order Preserving IO stack for Multi-Queue Block Device**  
  Jieun Kim, Joontaek Oh, Juwon Kim, Juwon Kim, Seung Won Yoo, and Youjip Won, *USENIX FAST*, 2025.

## Directory Overview

This repository contains the following directories, each serving a specific purpose related to OPIMQ and its experiments:
You can compile the kernel by running the `recompile.sh` script located in each kernel directory.  
Simply navigate to the desired kernel directory and execute the following command:

```bash
make clean
./recompile.sh
```
#### `5_18_18_barrierfs+opimq`
- Contains the kernel source and configurations for OPIMQ integrated with BarrierFS.
- Used for experiments requiring BarrierFS compatibility with OPIMQ.

#### `5_18_18_opext4_opimq`
- Contains the kernel source and configurations for OPIMQ integrated with OPEXT4.
- This is the main setup for evaluating OPIMQ's order-preserving capabilities with the OPEXT4 file system.

#### `5_18_18_original`
- Contains the unmodified vanilla Linux kernel (5.18.18).
- Serves as the baseline kernel for comparison in all experiments.

#### `OPFTL`
- Includes the source code and related files for OPFTL (Order-Preserving Flash Translation Layer).
- Primarily used for flash-level order preservation experiments.

#### `experiment_scripts`
- Contains scripts for running experiments, including quick tests, benchmarking, and performance evaluations.
- Refer to the `README` inside this directory for detailed instructions on executing specific tests.

## Configuring the Number of Submission Queues (SQ)

To specify the number of submission queues (SQ) for each kernel, you need to modify the `nvme_probe()` function in the `drivers/nvme/host/pci.c` file.  

1. Open the file:
   ```bash
   drivers/nvme/host/pci.c
   ```
2. Locate the `nvme_probe()` function.

3. Modify the `dev->nr_allocated_queues` value:
   - Set the value to **`(number of I/O queues + 1)`**.  
     This includes the admin queue, which is always required.

4. Save the changes and recompile the kernel using the `recompile.sh` script:
   ```bash
   ./recompile.sh

## Merits of OPIMQ

OPIMQ introduces a novel approach to ensuring storage order in multi-queue block devices, offering several advantages:

### 1. **Enhanced Performance**
- **Reduced Overheads:** Eliminates the need for costly transfer-and-flush operations by using cache barrier commands.
- **Improved Scalability:** Achieves up to 2.9× throughput improvement over the default Linux IO stack under workloads like Filebench `varmail`, `dbench`, and `sysbench`.
- **Full Parallelism Utilization:** Leverages multiple streams and epochs to maximize the internal parallelism of modern SSDs.

### 2. **Order Preservation**
- **Intra-Stream Guarantee:** Ensures that write requests within a single stream maintain their order, even when threads migrate across cores.
- **Inter-Stream Dependency:** Supports dual-stream writes to enforce ordering constraints between dependent streams effectively.

### 3. **Crash Consistency**
- **Robust Recovery:** Maintains order consistency across crashes without requiring changes to existing file systems or applications.
- **Metadata Backup:** Utilizes SSD-reserved flash memory to store essential metadata, enabling efficient recovery after system failures.

### 4. **Compatibility**
- **File System Integration:** Fully compatible with file systems like OP-EXT4, enabling seamless integration with existing storage stacks.
- **Support for Modern SSDs:** Works effectively with NVMe devices, utilizing multi-queue capabilities without compromising storage order.

### 5. **Application Suitability**
- **Fsync-Intensive Workloads:** Ideal for workloads requiring frequent synchronization, such as databases and journaling file systems.
- **Cloud Environments:** Scales efficiently in containerized environments, outperforming traditional IO stacks in scenarios with high concurrency.

OPIMQ redefines the balance between performance and storage order, making it a critical advancement for modern multi-queue storage systems.

## Installation and Usage

### Requirements
- **Linux Kernel:** Version 5.18.18
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
   ```

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


