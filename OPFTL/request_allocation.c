//////////////////////////////////////////////////////////////////////////////////
// request_allocation.c for Cosmos+ OpenSSD
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
// File Name: request_allocation.c
//
// Version: v1.0.0
//
// Description:
//   - allocate requests to each request queue
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "xil_printf.h"
#include <assert.h>
#include "debug.h"
#include "memory_map.h"
#include "hal/hal_host.h"
#include "host/nvme/nvme_main.h"

#include "request_format.h"

#if (SUPPORT_BARRIER_FTL == 1)
// SP: temp
#include "ftl/barrier_ftl.h"
#endif

P_REQ_POOL reqPoolPtr;
FREE_REQUEST_QUEUE freeReqQ;
SLICE_REQUEST_QUEUE sliceReqQ;
BLOCKED_BY_BUFFER_DEPENDENCY_REQUEST_QUEUE blockedByBufDepReqQ;
BLOCKED_BY_ROW_ADDR_DEPENDENCY_REQUEST_QUEUE blockedByRowAddrDepReqQ[USER_CHANNELS][USER_WAYS];
NVME_DMA_REQUEST_QUEUE nvmeDmaReqQ;
NAND_REQUEST_QUEUE nandReqQ[USER_CHANNELS][USER_WAYS];

unsigned int notCompletedNandReqCnt;
unsigned int blockedReqCnt;

void RequestAllocation_InitReqPool()
{
	int chNo, wayNo, reqSlotTag;

	reqPoolPtr = (P_REQ_POOL) REQ_POOL_ADDR; //revise address

	freeReqQ.headReq = 0;
	freeReqQ.tailReq = AVAILABLE_OUNTSTANDING_REQ_COUNT - 1;

	sliceReqQ.headReq = REQ_SLOT_TAG_NONE;
	sliceReqQ.tailReq = REQ_SLOT_TAG_NONE;
	sliceReqQ.reqCnt = 0;

	blockedByBufDepReqQ.headReq = REQ_SLOT_TAG_NONE;
	blockedByBufDepReqQ.tailReq = REQ_SLOT_TAG_NONE;
	blockedByBufDepReqQ.reqCnt = 0;

	nvmeDmaReqQ.headReq = REQ_SLOT_TAG_NONE;
	nvmeDmaReqQ.tailReq = REQ_SLOT_TAG_NONE;
	nvmeDmaReqQ.reqCnt = 0;

	for(chNo = 0; chNo<USER_CHANNELS; chNo++)
	{
		for(wayNo = 0; wayNo<USER_WAYS; wayNo++)
		{
			blockedByRowAddrDepReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
			blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
			blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt = 0;

			nandReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
			nandReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
			nandReqQ[chNo][wayNo].reqCnt = 0;
		}
	}

	for(reqSlotTag = 0; reqSlotTag < AVAILABLE_OUNTSTANDING_REQ_COUNT; reqSlotTag++)
	{
		reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_FREE;
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].prevReq = reqSlotTag - 1;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = reqSlotTag + 1;

#if (SUPPORT_BARRIER_FTL == 1)
		reqPoolPtr->reqPool[reqSlotTag].stream_id_1 = 0;
		reqPoolPtr->reqPool[reqSlotTag].stream_id_2 = 0;
		reqPoolPtr->reqPool[reqSlotTag].epoch_id_1 = 0;
		reqPoolPtr->reqPool[reqSlotTag].epoch_id_2 = 0;
		reqPoolPtr->reqPool[reqSlotTag].mappable_1 = 0;
		reqPoolPtr->reqPool[reqSlotTag].mappable_2 = 0;
		reqPoolPtr->reqPool[reqSlotTag].barrier_flag = 0;
#endif
	}

	reqPoolPtr->reqPool[0].prevReq = REQ_SLOT_TAG_NONE;
	reqPoolPtr->reqPool[AVAILABLE_OUNTSTANDING_REQ_COUNT - 1].nextReq = REQ_SLOT_TAG_NONE;
	freeReqQ.reqCnt = AVAILABLE_OUNTSTANDING_REQ_COUNT;

	notCompletedNandReqCnt = 0;
	blockedReqCnt = 0;
}

SSD_REQ_FORMAT* RequestAllocation_GetReqEntry(unsigned int reqSlotTag)
{
	return &reqPoolPtr->reqPool[reqSlotTag];
}

unsigned int RequestAllocation_GetReqPoolCount(unsigned int reqType, unsigned int channelNo, unsigned int wayNo)
{
	unsigned int count = 0;

	switch(reqType)
	{
		case REQ_QUEUE_TYPE_FREE:
			count = freeReqQ.reqCnt;
			break;
		case REQ_QUEUE_TYPE_SLICE:
			count = sliceReqQ.reqCnt;
			break;
		case REQ_QUEUE_TYPE_BLOCKED_BY_BUF_DEP:
			count = blockedByBufDepReqQ.reqCnt;
			break;
		case REQ_QUEUE_TYPE_BLOCKED_BY_ROW_ADDR_DEP:
			count = blockedByRowAddrDepReqQ[channelNo][wayNo].reqCnt;
			break;
		case REQ_QUEUE_TYPE_NVME_DMA:
			count = nvmeDmaReqQ.reqCnt;
			break;
		case REQ_QUEUE_TYPE_NAND:
			count = nandReqQ[channelNo][wayNo].reqCnt;
			break;
	}

	return count;
}


void RequestAllocation_MoveToFreeReqQ(unsigned int reqSlotTag)
{
	if(freeReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = freeReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[freeReqQ.tailReq].nextReq = reqSlotTag;
		freeReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		freeReqQ.headReq = reqSlotTag;
		freeReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_FREE;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.fua = FALSE;
	freeReqQ.reqCnt++;
}

unsigned int RequestAllocation_GetFreeReqEntry()
{
	unsigned int reqSlotTag;

	reqSlotTag = freeReqQ.headReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
	{
		RequestScheduler_SyncAvailFreeReq();
		reqSlotTag = freeReqQ.headReq;
	}

	if(reqPoolPtr->reqPool[reqSlotTag].nextReq != REQ_SLOT_TAG_NONE)
	{
		freeReqQ.headReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		freeReqQ.headReq = REQ_SLOT_TAG_NONE;
		freeReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	freeReqQ.reqCnt--;

	return reqSlotTag;
}

void RequestAllocation_MoveToSliceReqQ(unsigned int reqSlotTag)
{
	if(sliceReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = sliceReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[sliceReqQ.tailReq].nextReq = reqSlotTag;
		sliceReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		sliceReqQ.headReq = reqSlotTag;
		sliceReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_SLICE;
	sliceReqQ.reqCnt++;
}

unsigned int RequestAllocation_GetReqEntryFromSliceReqQ()
{
	unsigned int reqSlotTag;

	reqSlotTag = sliceReqQ.headReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		return REQ_SLOT_TAG_FAIL;

	if(reqPoolPtr->reqPool[reqSlotTag].nextReq != REQ_SLOT_TAG_NONE)
	{
		sliceReqQ.headReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		sliceReqQ.headReq = REQ_SLOT_TAG_NONE;
		sliceReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	sliceReqQ.reqCnt--;

	return reqSlotTag;
}

void RequestAllocation_MoveToBlockedByBufDepReqQ(unsigned int reqSlotTag)
{
	if(blockedByBufDepReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = blockedByBufDepReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[blockedByBufDepReqQ.tailReq].nextReq = reqSlotTag;
		blockedByBufDepReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.headReq = reqSlotTag;
		blockedByBufDepReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_BLOCKED_BY_BUF_DEP;
	blockedByBufDepReqQ.reqCnt++;
	blockedReqCnt++;
}

void RequestAllocation_SelectiveGetFromBlockedByBufDepReqQ(unsigned int reqSlotTag)
{
	unsigned int prevReq, nextReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		assert(!"[WARNING] Wrong reqSlotTag [WARNING]");

	prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;
	nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.headReq = nextReq;
	}
	else
	{
		blockedByBufDepReqQ.headReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	blockedByBufDepReqQ.reqCnt--;
	blockedReqCnt--;
}

void RequestAllocation_MoveToBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	if(blockedByRowAddrDepReqQ[chNo][wayNo].tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = blockedByRowAddrDepReqQ[chNo][wayNo].tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[blockedByRowAddrDepReqQ[chNo][wayNo].tailReq].nextReq = reqSlotTag;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = reqSlotTag;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_BLOCKED_BY_ROW_ADDR_DEP;
	blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt++;
	blockedReqCnt++;
}

unsigned int RequestAllocation_PeekReqEntryFromBlockedByRowAddrDepReqQ(unsigned int channelNo, unsigned int wayNo)
{
	return blockedByRowAddrDepReqQ[channelNo][wayNo].headReq;
}

void RequestAllocation_SelectiveGetFromBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	unsigned int prevReq, nextReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
	{
		assert(!"[WARNING] Wrong reqSlotTag [WARNING]");
	}

	prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;
	nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = nextReq;
	}
	else
	{
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NONE;
	blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt--;
	blockedReqCnt--;
}

void RequestAllocation_MoveToNvmeDmaReqQ(unsigned int reqSlotTag)
{
	if(nvmeDmaReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = nvmeDmaReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[nvmeDmaReqQ.tailReq].nextReq = reqSlotTag;
		nvmeDmaReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.headReq = reqSlotTag;
		nvmeDmaReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NVME_DMA;
	nvmeDmaReqQ.reqCnt++;
}

unsigned int RequestAllocation_PeekReqEntryFromNvmeDmaReqQ()
{
	return nvmeDmaReqQ.headReq;
}

void RequestAllocation_SelectiveGetFromNvmeDmaReqQ(unsigned int reqSlotTag)
{
	SSD_REQ_FORMAT* p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);
	unsigned int prevReq, nextReq;

	prevReq = p_reqEntry->prevReq;
	nextReq = p_reqEntry->nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.headReq = nextReq;
	}
	else
	{
		nvmeDmaReqQ.headReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	if (REQ_CODE_RxDMA == p_reqEntry->reqCode)
	{
		if (TRUE == p_reqEntry->nvmeDmaInfo.fua)
		{
			//SP: need to modify function call
			unsigned int dataBufEntryIdx = p_reqEntry->dataBufInfo.entry;

			unsigned int flushReqSlotTag = RequestAllocation_GetFreeReqEntry();
			SSD_REQ_FORMAT* p_flushReqEntry = RequestAllocation_GetReqEntry(flushReqSlotTag);

			unsigned int virtualSliceAddr =  AddrTransWrite(p_reqEntry->logicalSliceAddr);

			p_flushReqEntry->reqType = REQ_TYPE_NAND;
			p_flushReqEntry->reqCode = REQ_CODE_WRITE;
			p_flushReqEntry->nvmeCmdSlotTag = p_reqEntry->nvmeCmdSlotTag;
			p_flushReqEntry->logicalSliceAddr = p_reqEntry->logicalSliceAddr;
			p_flushReqEntry->nvmeDmaInfo.fua = p_reqEntry->nvmeDmaInfo.fua;
			p_flushReqEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
			p_flushReqEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
			p_flushReqEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
			p_flushReqEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
			p_flushReqEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
			p_flushReqEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
			p_flushReqEntry->dataBufInfo.entry = dataBufEntryIdx;
			BufferManagement_UpdateBufEntryInfoBlockingReq(dataBufEntryIdx, flushReqSlotTag);
			p_flushReqEntry->nandInfo.virtualSliceAddr = virtualSliceAddr;

			SelectLowLevelReqQ(flushReqSlotTag);

			NVME_COMMAND_ENTRY* p_cmdEntry = nvme_command_context_get_command_entry(p_reqEntry->nvmeCmdSlotTag);

			p_cmdEntry->NandWaitReqEntryCnt++;
		}
#if (SUPPORT_BARRIER_FTL == 1)
		/* Get the stream ids (stream id1, stream id2) and epoch ids (epoch id1, epoch id2) ,	
		 	and barrier flag of current slice */
		uint32_t sid1 = reqPoolPtr->reqPool[reqSlotTag].stream_id_1;
		uint32_t sid2 = reqPoolPtr->reqPool[reqSlotTag].stream_id_2;
		uint32_t eid1 = reqPoolPtr->reqPool[reqSlotTag].epoch_id_1;
		uint32_t eid2 = reqPoolPtr->reqPool[reqSlotTag].epoch_id_2;
		uint32_t barrier_flag = reqPoolPtr->reqPool[reqSlotTag].barrier_flag;

		if (sid1 > 0)
		{
			// Set the mappable state to the corresponding data buffer, which is allocated to the slice. 
			DATA_BUF_ENTRY* p_dataBufEntry = BufferManagement_GetDataBufEntry(p_reqEntry->dataBufInfo.entry);

			// Initialize buffer entry's mappable state.
			p_dataBufEntry->mappable_1 = FALSE;
			p_dataBufEntry->mappable_2 = (sid2 > 0)? FALSE: TRUE;

			
			/* If the previous epoch state if closed, durable, and mapped, update current slice's data buffer to "mappable". */
			uint32_t ret;
#if (BARRIER_IN_DMA == 1)
			ret = barrier_check_and_set_epoch_state(sid1, eid1, barrier_flag, 1);
#else
			ret = barrier_check_prev_epoch_state(sid1, eid1);
#endif
			if (ret == EPOCH_STATE_CLOSED_DURABLE_MAPPED || 
				is_mappable(sid1, eid1))
				//is_mappable(sid1, (eid1>0)? eid1-1: eid1))
			{

				p_dataBufEntry->mappable_1 = TRUE;
			}

			/* If current slice is from dual stream write, check the previous epoch state included in stream id 2.
			   Update the second mappable state of current slice to true, if previous epoch of stream 2 is closed, durable
			   and mappable. */
			if (sid2) {
#if (BARRIER_IN_DMA == 1)
				ret = barrier_check_and_set_epoch_state(sid2, eid2, barrier_flag, 1);
#else
				ret = barrier_check_prev_epoch_state(sid2, eid2);
#endif
				if (ret == EPOCH_STATE_CLOSED_DURABLE_MAPPED 
						|| is_mappable(sid2,eid2))
				{
					p_dataBufEntry->mappable_2 = TRUE;
				}
			}

			// Debug
			//uint32_t buf_dirty = 0;
			//if (p_dataBufEntry->dirty == DATA_BUF_DIRTY) {
			//	buf_dirty = 1;
			//}
			//xil_printf("\r\n [ BUF ] buf->sid1:%u buf->eid1:%u buf_index %u Dirty :%u\r\n",
			//		p_dataBufEntry->stream_id_1, p_dataBufEntry->epoch_id_1, p_reqEntry->dataBufInfo.entry, buf_dirty);
		}
#endif
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NONE;
	nvmeDmaReqQ.reqCnt--;

	RequestAllocation_MoveToFreeReqQ(reqSlotTag);
	ReleaseBlockedByBufDepReq(reqSlotTag);
}

void RequestAllocation_MoveToNandReqQ(unsigned int reqSlotTag, unsigned chNo, unsigned wayNo)
{
	if(nandReqQ[chNo][wayNo].tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = nandReqQ[chNo][wayNo].tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[nandReqQ[chNo][wayNo].tailReq].nextReq = reqSlotTag;
		nandReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		nandReqQ[chNo][wayNo].headReq = reqSlotTag;
		nandReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NAND;
	nandReqQ[chNo][wayNo].reqCnt++;
	notCompletedNandReqCnt++;
}

unsigned int RequestAllocation_PeekReqEntryFromNandReqQ(unsigned int channelNo, unsigned int wayNo)
{
	return nandReqQ[channelNo][wayNo].headReq;
}

void RequestAllocation_GetEntryFromNandReqQ(unsigned int chNo, unsigned int wayNo, unsigned int reqCode)
{
	ASSERT(0 < notCompletedNandReqCnt);

	unsigned int reqSlotTag = nandReqQ[chNo][wayNo].headReq;

	if (reqSlotTag == REQ_SLOT_TAG_NONE)
	{
		assert(!"[WARNING] there is no request in Nand-req-queue[WARNING]");
	}

	SSD_REQ_FORMAT* p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	if(REQ_SLOT_TAG_NONE != p_reqEntry->nextReq)
	{
		nandReqQ[chNo][wayNo].headReq = p_reqEntry->nextReq;

		reqPoolPtr->reqPool[p_reqEntry->nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		nandReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
		nandReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
	}

	if ((REQ_CODE_WRITE == reqCode)
		&& (TRUE == p_reqEntry->nvmeDmaInfo.fua))
	{
		NVME_COMMAND_ENTRY* p_cmdEntry = nvme_command_context_get_command_entry(p_reqEntry->nvmeCmdSlotTag);

		p_cmdEntry->NandCompleteReqEntryCnt++;

		if (p_cmdEntry->totalReqEntryCnt == p_cmdEntry->NandCompleteReqEntryCnt)
		{
			hal_host_completion_nvme_command(p_cmdEntry->cmdSlotTag, 0, SC_SUCCESSFUL_COMPLETION);

			nvme_command_context_decrease_write_outstanding_count();
			nvme_command_context_increase_complete_command_count();
		}
	}

#if (SUPPORT_BARRIER_FTL == 1)
	if ((reqCode == REQ_CODE_WRITE) && (reqPoolPtr->reqPool[reqSlotTag].reqType == REQ_TYPE_NAND ) &&
			(p_reqEntry->stream_id_1 > 0)) {
		//xil_printf("\r\n [ Complete NAND REQ ] sid %u eid %u\r\n",
		//		p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1);

		//barrier_increase_durable_page_count(p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1, 1);
		//if (p_reqEntry->mappable_1 == TRUE) {
		//	barrier_increase_updated_page_count(p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1, 1);
		//}
		/*
		if (p_reqEntry->stream_id_2 > 0) {
			barrier_increase_durable_page_count(p_reqEntry->stream_id_2, p_reqEntry->epoch_id_2, 1);
			if (p_reqEntry->mappable_2 == TRUE) {
				barrier_increase_updated_page_count(p_reqEntry->stream_id_2, p_reqEntry->epoch_id_2, 1);
			}
		}*/
		/* Initialize REQ entry */
		p_reqEntry->stream_id_1 = 0;
		p_reqEntry->stream_id_2 = 0;
		p_reqEntry->epoch_id_1 = 0;
		p_reqEntry->epoch_id_2 = 0;
		p_reqEntry->mappable_1 = 0;
		p_reqEntry->mappable_2 = 0;
		p_reqEntry->reqCode = REQ_CODE_FLUSH;

		//xil_printf("\r\n [ Initialize REQ Entry] sid %u eid %u\r\n",
		//				p_reqEntry->stream_id_1, p_reqEntry->epoch_id_1);
	}



	//barrier_update_state(p_reqEntry->stream_id_2, p_reqEntry->epoch_id_2);
#endif

	p_reqEntry->reqQueueType = REQ_QUEUE_TYPE_NONE;

	nandReqQ[chNo][wayNo].reqCnt--;
	notCompletedNandReqCnt--;

	RequestAllocation_MoveToFreeReqQ(reqSlotTag);
	ReleaseBlockedByBufDepReq(reqSlotTag);

}
