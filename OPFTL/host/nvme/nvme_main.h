//////////////////////////////////////////////////////////////////////////////////
// nvme_main.h for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe Main
// File Name: nvme_main.h
//
// Version: v1.0.0
//
// Description:
//   - declares nvme_main function
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#ifndef __NVME_MAIN_H_
#define __NVME_MAIN_H_

typedef struct _NVME_CONTEXT
{
	unsigned int status;
	unsigned int cacheEn;
	NVME_ADMIN_QUEUE_STATUS adminQueueInfo;
	unsigned short numOfIOSubmissionQueuesAllocated;//non zero-based value
	unsigned short numOfIOCompletionQueuesAllocated;//non zero-based value
	NVME_IO_SQ_STATUS ioSqInfo[MAX_NUM_OF_IO_SQ];
	NVME_IO_CQ_STATUS ioCqInfo[MAX_NUM_OF_IO_CQ];
} NVME_TASK_CONTEXT;


typedef struct _NVME_COMMAND_ENTRY
{
	unsigned short qID;
	unsigned short cmdSlotTag;
	unsigned int cmdSeqNum;
	unsigned int cmdDword[16];
	unsigned int totalReqEntryCnt;
	unsigned int hdmaWaitReqEntryCnt;
	unsigned int hdmaCompleteReqEntryCnt;
	unsigned int NandWaitReqEntryCnt;
	unsigned int NandCompleteReqEntryCnt;
}NVME_COMMAND_ENTRY;

typedef struct
{
	NVME_COMMAND_ENTRY command_list[256]; // need to define magic number

	// IO command only
	unsigned int fetch_io_command_count;
	unsigned int outstanding_read_command_count;
	unsigned int outstanding_write_command_count;
	unsigned int complete_io_command_count;
} NVME_COMMAND_CONTEXT_T;

extern NVME_COMMAND_CONTEXT_T g_nvme_command_context;

NVME_COMMAND_ENTRY* nvme_command_context_get_command_entry(unsigned int cmdSlotTag);
void nvme_command_context_increase_read_outstanding_count();
void nvme_command_context_increase_write_outstanding_count();
void nvme_command_context_decrease_read_outstanding_count();
void nvme_command_context_decrease_write_outstanding_count();
void nvme_command_context_increase_complete_command_count();
void dev_irq_handler();

void host_task_init(void);
void host_task_run(void);

#endif	//__NVME_MAIN_H_
