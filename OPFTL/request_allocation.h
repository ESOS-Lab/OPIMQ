//////////////////////////////////////////////////////////////////////////////////
// request_allocation.h for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
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
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Request Allocator
// File Name: request_allocation.h
//
// Version: v1.0.0
//
// Description:
//   - define parameters, data structure and functions of request allocator
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#ifndef REQUEST_ALLOCATION_H_
#define REQUEST_ALLOCATION_H_

#include "ftl/ftl_config.h"
#include "request_format.h"
#include "request_queue.h"

#define	AVAILABLE_OUNTSTANDING_REQ_COUNT			((USER_DIES) * 128)  //regardless of request type

#define REQ_SLOT_TAG_NONE		0xffff
#define REQ_SLOT_TAG_FAIL		0xffff

typedef struct _REQ_POOL
{
	SSD_REQ_FORMAT reqPool[AVAILABLE_OUNTSTANDING_REQ_COUNT];
} REQ_POOL, *P_REQ_POOL;

extern P_REQ_POOL reqPoolPtr;

extern unsigned int notCompletedNandReqCnt;
extern unsigned int blockedReqCnt;


void RequestAllocation_InitReqPool();
SSD_REQ_FORMAT* RequestAllocation_GetReqEntry(unsigned int reqSlotTag);
unsigned int RequestAllocation_GetReqPoolCount(unsigned int reqType, unsigned int channelNo, unsigned int wayNo);

void RequestAllocation_MoveToFreeReqQ(unsigned int reqSlotTag);
unsigned int RequestAllocation_GetFreeReqEntry();

void RequestAllocation_MoveToSliceReqQ(unsigned int reqSlotTag);
unsigned int RequestAllocation_GetReqEntryFromSliceReqQ();

void RequestAllocation_MoveToBlockedByBufDepReqQ(unsigned int reqSlotTag);
void RequestAllocation_SelectiveGetFromBlockedByBufDepReqQ(unsigned int reqSlotTag);

void RequestAllocation_MoveToBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo);
unsigned int RequestAllocation_PeekReqEntryFromBlockedByRowAddrDepReqQ(unsigned int channelNo, unsigned int wayNo);
void RequestAllocation_SelectiveGetFromBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo);

void RequestAllocation_MoveToNvmeDmaReqQ(unsigned int reqSlotTag);
unsigned int RequestAllocation_PeekReqEntryFromNvmeDmaReqQ();
void RequestAllocation_SelectiveGetFromNvmeDmaReqQ(unsigned int reqSlotTag);

void RequestAllocation_MoveToNandReqQ(unsigned int reqSlotTag, unsigned chNo, unsigned wayNo);
unsigned int RequestAllocation_PeekReqEntryFromNandReqQ(unsigned int channelNo, unsigned int wayNo);
void RequestAllocation_GetEntryFromNandReqQ(unsigned int chNo, unsigned int wayNo, unsigned int reqCode);

#endif /* REQUEST_ALLOCATION_H_ */
