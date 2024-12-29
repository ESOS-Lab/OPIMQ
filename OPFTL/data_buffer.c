//////////////////////////////////////////////////////////////////////////////////
// data_buffer.c for Cosmos+ OpenSSD
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
// Module Name: Data Buffer Manager
// File Name: data_buffer.c
//
// Version: v1.0.0
//
// Description:
//   - manage data buffer used to transfer data between host system and NAND device
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include <assert.h>
#include "memory_map.h"

//bm_context_t g_bm_context;

P_DATA_BUF_MAP dataBufMapPtr;
DATA_BUF_LRU_LIST dataBufLruList;
P_DATA_BUF_HASH_TABLE sp_dataBufHashTable;
P_TEMPORARY_DATA_BUF_MAP tempDataBufMapPtr;

void BufferManagement_Init()
{
#if 0
	g_bm_context.free_count = AVAILABLE_DATA_BUFFER_ENTRY_COUNT;

	for (unsigned int bitmap_offset = 0; bitmap_offset < (AVAILABLE_DATA_BUFFER_ENTRY_COUNT/32); bitmap_offset++)
	{
		g_bm_context.bitmap[bitmap_offset] = 0xFFFFFFFF;
	}
#endif

	dataBufMapPtr = (P_DATA_BUF_MAP) DATA_BUFFER_MAP_ADDR;
	sp_dataBufHashTable = (P_DATA_BUF_HASH_TABLE)DATA_BUFFFER_HASH_TABLE_ADDR;
	tempDataBufMapPtr = (P_TEMPORARY_DATA_BUF_MAP)TEMPORARY_DATA_BUFFER_MAP_ADDR;

	DATA_BUF_ENTRY* p_dataBufEntry = NULL;

	for (unsigned int bufIdx = 0; bufIdx < AVAILABLE_DATA_BUFFER_ENTRY_COUNT; bufIdx++)
	{
		p_dataBufEntry = BufferManagement_GetDataBufEntry(bufIdx);

		p_dataBufEntry->logicalSliceAddr = LSA_NONE;
		p_dataBufEntry->prevEntry = bufIdx-1;
		p_dataBufEntry->nextEntry = bufIdx+1;
		p_dataBufEntry->dirty = DATA_BUF_CLEAN;
		p_dataBufEntry->blockingReqTail =  REQ_SLOT_TAG_NONE;

		sp_dataBufHashTable->dataBufHash[bufIdx].headEntry = DATA_BUF_IDX_INVALID;
		sp_dataBufHashTable->dataBufHash[bufIdx].tailEntry = DATA_BUF_IDX_INVALID;

		p_dataBufEntry->hashPrevEntry = DATA_BUF_IDX_INVALID;
		p_dataBufEntry->hashNextEntry = DATA_BUF_IDX_INVALID;
	}

	p_dataBufEntry = BufferManagement_GetDataBufEntry(0);

	p_dataBufEntry->prevEntry = DATA_BUF_IDX_INVALID;

	p_dataBufEntry = BufferManagement_GetDataBufEntry((AVAILABLE_DATA_BUFFER_ENTRY_COUNT - 1));

	p_dataBufEntry->nextEntry = DATA_BUF_IDX_INVALID;

	dataBufLruList.headEntry = 0 ;
	dataBufLruList.tailEntry = AVAILABLE_DATA_BUFFER_ENTRY_COUNT - 1;

	for(unsigned int bufIdx = 0; bufIdx < AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT; bufIdx++)
	{
		tempDataBufMapPtr->tempDataBuf[bufIdx].blockingReqTail =  REQ_SLOT_TAG_NONE;
	}
}

DATA_BUF_ENTRY* BufferManagement_GetDataBufEntry(unsigned int dataBufIdx)
{
	return &dataBufMapPtr->dataBuf[dataBufIdx];
}

unsigned int BufferManagement_CheckBufHit(unsigned int reqSlotTag)
{
	unsigned int bufIdx, logicalSliceAddr;
	DATA_BUF_ENTRY* p_dataBufEntry = NULL;

	logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
	bufIdx = sp_dataBufHashTable->dataBufHash[FindDataBufHashTableEntry(logicalSliceAddr)].headEntry;
#if (IFLOOP_DEBUG == 1)
	unsigned int cnt = 0;
#endif

	while(bufIdx != DATA_BUF_IDX_INVALID)
	{
//#if (PRINT_DEBUG == 1)
//		xil_printf("%u\n", bufIdx );
//#endif

#if (IFLOOP_DEBUG == 1)
		cnt += 1;
		if (cnt > 1000)
			xil_printf("%u\n", bufIdx );

#endif
		
		p_dataBufEntry = BufferManagement_GetDataBufEntry(bufIdx);

		if(logicalSliceAddr == p_dataBufEntry->logicalSliceAddr)
		{
			if((DATA_BUF_IDX_INVALID != p_dataBufEntry->nextEntry)
				&& (DATA_BUF_IDX_INVALID != p_dataBufEntry->prevEntry))
			{
				dataBufMapPtr->dataBuf[p_dataBufEntry->prevEntry].nextEntry = p_dataBufEntry->nextEntry;
				dataBufMapPtr->dataBuf[p_dataBufEntry->nextEntry].prevEntry = p_dataBufEntry->prevEntry;
			}
			else if((DATA_BUF_IDX_INVALID == p_dataBufEntry->nextEntry)
				&& (DATA_BUF_IDX_INVALID != p_dataBufEntry->prevEntry))
			{
				dataBufMapPtr->dataBuf[p_dataBufEntry->prevEntry].nextEntry = DATA_BUF_IDX_INVALID;
				dataBufLruList.tailEntry = p_dataBufEntry->prevEntry;
			}
			else if((DATA_BUF_IDX_INVALID != p_dataBufEntry->nextEntry)
				&& (DATA_BUF_IDX_INVALID == p_dataBufEntry->prevEntry))
			{
				dataBufMapPtr->dataBuf[p_dataBufEntry->nextEntry].prevEntry = DATA_BUF_IDX_INVALID;
				dataBufLruList.headEntry = p_dataBufEntry->nextEntry;
			}
			else
			{
				dataBufLruList.tailEntry = DATA_BUF_IDX_INVALID;
				dataBufLruList.headEntry = DATA_BUF_IDX_INVALID;
			}

			if(dataBufLruList.headEntry != DATA_BUF_IDX_INVALID)
			{
				p_dataBufEntry->prevEntry = DATA_BUF_IDX_INVALID;
				p_dataBufEntry->nextEntry = dataBufLruList.headEntry;
				dataBufMapPtr->dataBuf[dataBufLruList.headEntry].prevEntry = bufIdx;
				dataBufLruList.headEntry = bufIdx;
			}
			else
			{
				p_dataBufEntry->prevEntry = DATA_BUF_IDX_INVALID;
				p_dataBufEntry->nextEntry = DATA_BUF_IDX_INVALID;
				dataBufLruList.headEntry = bufIdx;
				dataBufLruList.tailEntry = bufIdx;
			}

//#if (PRINT_DEBUG == 1)
			//xil_printf("hit!\n");
//#endif

			return bufIdx;
		}
		else
		{
			bufIdx = p_dataBufEntry->hashNextEntry;
		}
	}
//#if (PRINT_DEBUG == 1)
	//xil_printf("hit fail\n");
//#endif
	return DATA_BUF_FAIL;
}

unsigned int BufferManagement_GetDataBufAddr(unsigned int reqSlotTag)
{
	if(reqPoolPtr->reqPool[reqSlotTag].reqType == REQ_TYPE_NAND)
	{
		if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_ENTRY)
		{
			return (DATA_BUFFER_BASE_ADDR + reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry * BYTES_PER_DATA_REGION_OF_SLICE);
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_TEMP_ENTRY)
		{
			return (TEMPORARY_DATA_BUFFER_BASE_ADDR + reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry * BYTES_PER_DATA_REGION_OF_SLICE);
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_ADDR)
		{
			return reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.addr;
		}

		return RESERVED_DATA_BUFFER_BASE_ADDR;
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqType == REQ_TYPE_NVME_DMA)
	{
		if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_ENTRY)
		{
			return (DATA_BUFFER_BASE_ADDR + reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry * BYTES_PER_DATA_REGION_OF_SLICE + reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nvmeBlockOffset * BYTES_PER_NVME_BLOCK);
		}
		else
		{
			assert(!"[WARNING] wrong reqOpt-dataBufFormat [WARNING]");
		}
	}
	else
	{
		assert(!"[WARNING] wrong reqType [WARNING]");
	}

}

unsigned int BufferManagement_GetSpareDataBufAddr(unsigned int reqSlotTag)
{
	SSD_REQ_FORMAT* p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

	if(REQ_TYPE_NAND == p_requestEntry->reqType)
	{
		switch(p_requestEntry->reqOpt.dataBufFormat)
		{
			case REQ_OPT_DATA_BUF_ENTRY:
				return (SPARE_DATA_BUFFER_BASE_ADDR + p_requestEntry->dataBufInfo.entry * BYTES_PER_SPARE_REGION_OF_SLICE);
			case REQ_OPT_DATA_BUF_TEMP_ENTRY:
				return (TEMPORARY_SPARE_DATA_BUFFER_BASE_ADDR + p_requestEntry->dataBufInfo.entry * BYTES_PER_SPARE_REGION_OF_SLICE);
			case REQ_OPT_DATA_BUF_ADDR:
				return (p_requestEntry->dataBufInfo.addr + BYTES_PER_DATA_REGION_OF_SLICE); // modify PAGE_SIZE to other
			default:
				return (RESERVED_DATA_BUFFER_BASE_ADDR + BYTES_PER_DATA_REGION_OF_SLICE);
		}
	}
	else if (REQ_TYPE_NVME_DMA == p_requestEntry->reqType)
	{
		if (REQ_OPT_DATA_BUF_ENTRY == p_requestEntry->reqOpt.dataBufFormat)
		{
			return (SPARE_DATA_BUFFER_BASE_ADDR + p_requestEntry->dataBufInfo.entry * BYTES_PER_SPARE_REGION_OF_SLICE);
		}
		else
		{
			assert(!"[WARNING] wrong reqOpt-dataBufFormat [WARNING]");
		}
	}
	else
	{
		assert(!"[WARNING] wrong reqType [WARNING]");
	}
}

void BufferManagement_ReleaseBuf(unsigned int buf_idx)
{
#if 0
	unsigned int offset = (buf_idx/32);

	buf_idx %= 32;

	g_bm_context.bitmap[offset] |= (1 << buf_idx);
#endif
}

unsigned int BufferManagement_AllocBuf_New()
{
#if 0
	if (0 == g_bm_context.free_count)
	{
		return DATA_BUF_IDX_INVALID;
	}

	unsigned int buf_idx = DATA_BUF_IDX_INVALID;

	for (unsigned int offset = 0; offset < (AVAILABLE_DATA_BUFFER_ENTRY_COUNT/32); offset++)
	{
		buf_idx = __builtin_ctz(g_bm_context.bitmap[offset]);

		if (32 != buf_idx)
		{
			g_bm_context.bitmap[offset] &= ~(1 << buf_idx);
			buf_idx += (offset*32);

			break;
		}
	}

	return buf_idx;
#endif
	return DATA_BUF_IDX_INVALID;
}

unsigned int BufferManagement_AllocBuf()
{
	unsigned int evictedBufEntryIdx = dataBufLruList.tailEntry;

	if(evictedBufEntryIdx == DATA_BUF_IDX_INVALID)
	{
		assert(!"[WARNING] There is no valid buffer entry [WARNING]");
	}

	DATA_BUF_ENTRY* p_evictedDataBufEntry = BufferManagement_GetDataBufEntry(evictedBufEntryIdx);

	if (DATA_BUF_IDX_INVALID != p_evictedDataBufEntry->prevEntry)
	{
		dataBufMapPtr->dataBuf[p_evictedDataBufEntry->prevEntry].nextEntry = DATA_BUF_IDX_INVALID;
		dataBufLruList.tailEntry = p_evictedDataBufEntry->prevEntry;

		p_evictedDataBufEntry->prevEntry = DATA_BUF_IDX_INVALID;
		p_evictedDataBufEntry->nextEntry = dataBufLruList.headEntry;

		dataBufMapPtr->dataBuf[dataBufLruList.headEntry].prevEntry = evictedBufEntryIdx;

		dataBufLruList.headEntry = evictedBufEntryIdx;

	}
	else
	{
		p_evictedDataBufEntry->prevEntry = DATA_BUF_IDX_INVALID;
		p_evictedDataBufEntry->nextEntry = DATA_BUF_IDX_INVALID;

		dataBufLruList.headEntry = evictedBufEntryIdx;
		dataBufLruList.tailEntry = evictedBufEntryIdx;
	}

	BufferManagement_SelectiveGetFromHashList(evictedBufEntryIdx, 1);

	return evictedBufEntryIdx;
}

unsigned int BufferManagement_AllocateTempBuf(unsigned int dieNo)
{
	return dieNo;
}

void BufferManagement_UpdateBufEntryInfoBlockingReq(unsigned int bufEntryIdx, unsigned int reqSlotTag)
{
	if(dataBufMapPtr->dataBuf[bufEntryIdx].blockingReqTail != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = dataBufMapPtr->dataBuf[bufEntryIdx].blockingReqTail;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq].nextBlockingReq  = reqSlotTag;
	}

	dataBufMapPtr->dataBuf[bufEntryIdx].blockingReqTail = reqSlotTag;
}

void BufferManagement_UpdateTempBufEntryInfoBlockingReq(unsigned int bufEntry, unsigned int reqSlotTag)
{

	if(tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq].nextBlockingReq  = reqSlotTag;
	}

	tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail = reqSlotTag;
}

void BufferManagement_AddBufToHashList(unsigned int bufEntryIdx)
{
	unsigned int hashEntry;

	hashEntry = FindDataBufHashTableEntry(dataBufMapPtr->dataBuf[bufEntryIdx].logicalSliceAddr);

	DATA_BUF_ENTRY* p_dataBufEntry = BufferManagement_GetDataBufEntry(bufEntryIdx);

	if(DATA_BUF_IDX_INVALID != sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry)
	{
		p_dataBufEntry->hashPrevEntry = sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry ;
		p_dataBufEntry->hashNextEntry = REQ_SLOT_TAG_NONE;

		dataBufMapPtr->dataBuf[sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry].hashNextEntry = bufEntryIdx;
		
#if(PRINT_DEBUG == 1)
		//xil_printf("[AddBufToHashList] lsa:0x%X, bufidx:0x%X, hashNextEntry:0x%X\r\n", dataBufMapPtr->dataBuf[sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry].logicalSliceAddr, sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry, bufEntryIdx);
		xil_printf("[A] hE: %d, tbi: %d, bi: %d\n", hashEntry, sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry, bufEntryIdx);
#endif

		sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry = bufEntryIdx;
	}
	else
	{
		p_dataBufEntry->hashPrevEntry = DATA_BUF_IDX_INVALID;
		p_dataBufEntry->hashNextEntry = DATA_BUF_IDX_INVALID;
	//	p_dataBufEntry->hashNextEntry = REQ_SLOT_TAG_NONE;

#if(PRINT_DEBUG == 1)
		xil_printf("[A] hE: %d, bi: %d \r\n", hashEntry, bufEntryIdx);
#endif

		sp_dataBufHashTable->dataBufHash[hashEntry].headEntry = bufEntryIdx;
		sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry = bufEntryIdx;
	}
}


void BufferManagement_SelectiveGetFromHashList(unsigned int bufEntryIdx, unsigned int is_allocbuf)
{
	DATA_BUF_ENTRY* p_dataBufEntry = BufferManagement_GetDataBufEntry(bufEntryIdx);

	if (LSA_NONE != p_dataBufEntry->logicalSliceAddr)
	{
		unsigned int prevBufEntry, nextBufEntry, hashEntry;

		prevBufEntry =  p_dataBufEntry->hashPrevEntry;
		nextBufEntry =  p_dataBufEntry->hashNextEntry;
		hashEntry = FindDataBufHashTableEntry(p_dataBufEntry->logicalSliceAddr);

		if((nextBufEntry != DATA_BUF_IDX_INVALID) && (prevBufEntry != DATA_BUF_IDX_INVALID))
		{
			dataBufMapPtr->dataBuf[prevBufEntry].hashNextEntry = nextBufEntry;
			dataBufMapPtr->dataBuf[nextBufEntry].hashPrevEntry = prevBufEntry;
#if(PRINT_DEBUG == 1)
			//xil_printf("[SelectiveGetFromHashList] lsa:0x%X, bufidx:0x%X, hashPrevEntry:0x%X, hashNextEntry:0x%X\r\n", dataBufMapPtr->dataBuf[prevBufEntry].logicalSliceAddr, bufEntryIdx, prevBufEntry, nextBufEntry);
			xil_printf("[G] hE: %d, bi: %d, hPE: %d, hNE: %d %d\n", hashEntry, bufEntryIdx, prevBufEntry, nextBufEntry, is_allocbuf);
#endif
		}
		else if((nextBufEntry == DATA_BUF_IDX_INVALID) && (prevBufEntry != DATA_BUF_IDX_INVALID))
		{
			dataBufMapPtr->dataBuf[prevBufEntry].hashNextEntry = DATA_BUF_IDX_INVALID;
			sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry = prevBufEntry;
#if(PRINT_DEBUG == 1)
			//xil_printf("[SelectiveGetFromHashList] lsa:0x%X, bufidx:0x%X, hashPrevEntry:0x%X, hashNextEntry:-1 \r\n", dataBufMapPtr->dataBuf[prevBufEntry].logicalSliceAddr, bufEntryIdx, prevBufEntry);
			xil_printf("[G] hE: %d, bi: %d, hPE: %d, hNE:-1 %d \n", hashEntry, bufEntryIdx, prevBufEntry, is_allocbuf);
#endif
		}
		else if((nextBufEntry != DATA_BUF_IDX_INVALID) && (prevBufEntry == DATA_BUF_IDX_INVALID))
		{
			dataBufMapPtr->dataBuf[nextBufEntry].hashPrevEntry = DATA_BUF_IDX_INVALID;
			sp_dataBufHashTable->dataBufHash[hashEntry].headEntry = nextBufEntry;
#if(PRINT_DEBUG == 1)
			//xil_printf("[SelectiveGetFromHashList] lsa:0x%X, bufidx:0x%X, hashPrevEntry:-1, hashNextEntry:0x%X\r\n", dataBufMapPtr->dataBuf[prevBufEntry].logicalSliceAddr, bufEntryIdx, nextBufEntry);
			xil_printf("[G] hE: %d, bi: %d, hPE: -1, hNE: %d %d \n", hashEntry, bufEntryIdx, nextBufEntry, is_allocbuf);
#endif
		}
		else
		{
			sp_dataBufHashTable->dataBufHash[hashEntry].headEntry = DATA_BUF_IDX_INVALID;
			sp_dataBufHashTable->dataBufHash[hashEntry].tailEntry = DATA_BUF_IDX_INVALID;
#if(PRINT_DEBUG == 1)
			xil_printf("[G] hE: %d, bi: %d, hPE: -1, hNE: -1 %d \n", hashEntry, bufEntryIdx, is_allocbuf);
#endif
		}
	}
}
