//////////////////////////////////////////////////////////////////////////////////
// garbage_collection.c for Cosmos+ OpenSSD
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
// Module Name: Garbage Collector
// File Name: garbage_collection.c
//
// Version: v1.0.0
//
// Description:
//   - select a victim block
//   - collect valid pages to a free block
//   - erase a victim block to make a free block
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "xil_printf.h"
#include <assert.h>
#include "../memory_map.h"

P_GC_VICTIM_MAP sp_GCVimtimBlockMap = (P_GC_VICTIM_MAP) GC_VICTIM_MAP_ADDR;

unsigned int _GetVictimBlock(unsigned int dieNo)
{
	unsigned int evictedBlockNo;
	int invalidSliceCnt;

	for(invalidSliceCnt = SLICES_PER_BLOCK; invalidSliceCnt > 0 ; invalidSliceCnt--)
	{
		if (sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock != BLOCK_NONE)
		{
			evictedBlockNo = sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock;

			if(BLOCK_NONE != virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock)
			{
				virtualBlockMapPtr->block[dieNo][virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock].prevBlock = BLOCK_NONE;

				sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock;

			}
			else
			{
				sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
				sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
			}

			return evictedBlockNo;
		}
	}

	assert(!"[WARNING] There are no free blocks. Abort terminate this ssd. [WARNING]");
	return BLOCK_FAIL;
}

void GarbageCollection_AddVictimBlock(unsigned int dieNo, unsigned int blockNo, unsigned int invalidSliceCnt)
{
	if(sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock != BLOCK_NONE)
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock].nextBlock = blockNo;

		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = blockNo;
	}
	else
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;

		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = blockNo;
		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = blockNo;
	}
}

void GarbageCollection_SelectiveGetFromGcVictimList(unsigned int dieNo, unsigned int blockNo)
{
	unsigned int nextBlock, prevBlock, invalidSliceCnt;

	nextBlock = virtualBlockMapPtr->block[dieNo][blockNo].nextBlock;
	prevBlock = virtualBlockMapPtr->block[dieNo][blockNo].prevBlock;
	invalidSliceCnt = virtualBlockMapPtr->block[dieNo][blockNo].invalidSliceCnt;

	if((nextBlock != BLOCK_NONE) && (prevBlock != BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][prevBlock].nextBlock = nextBlock;
		virtualBlockMapPtr->block[dieNo][nextBlock].prevBlock = prevBlock;
	}
	else if((nextBlock == BLOCK_NONE) && (prevBlock != BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][prevBlock].nextBlock = BLOCK_NONE;
		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = prevBlock;
	}
	else if((nextBlock != BLOCK_NONE) && (prevBlock == BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][nextBlock].prevBlock = BLOCK_NONE;

		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = nextBlock;
	}
	else
	{
		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
		sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
	}
}

void GarbageCollection_Init()
{
	for(unsigned int dieNo=0 ; dieNo < USER_DIES; dieNo++)
	{
		for(unsigned int invalidSliceCnt = 0 ; invalidSliceCnt < (SLICES_PER_BLOCK+1); invalidSliceCnt++)
		{
			sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
			sp_GCVimtimBlockMap->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
		}
	}
}

void GarbageCollection_Process(unsigned int dieNo)
{
	unsigned int victimBlockNo, pageNo, virtualSliceAddr, logicalSliceAddr, dieNoForGcCopy, reqSlotTag;

	victimBlockNo = _GetVictimBlock(dieNo);
	dieNoForGcCopy = dieNo;

	if(SLICES_PER_BLOCK != virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt)
	{
		SSD_REQ_FORMAT* p_requestEntry = NULL;

		for (pageNo = 0 ; pageNo < USER_PAGES_PER_BLOCK; pageNo++)
		{
			virtualSliceAddr = Vorg2VsaTranslation(dieNo, victimBlockNo, pageNo);
			logicalSliceAddr = virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr;

			if(LSA_NONE != logicalSliceAddr)
			{
				if(virtualSliceAddr == logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr) //valid data
				{
					//read
					reqSlotTag = RequestAllocation_GetFreeReqEntry();
					p_requestEntry = RequestAllocation_GetReqEntry(reqSlotTag);

					p_requestEntry->reqType = REQ_TYPE_NAND;
					p_requestEntry->reqCode = REQ_CODE_READ;
					p_requestEntry->logicalSliceAddr = logicalSliceAddr;
					p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					p_requestEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					p_requestEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					p_requestEntry->dataBufInfo.entry = BufferManagement_AllocateTempBuf(dieNo);

					BufferManagement_UpdateTempBufEntryInfoBlockingReq(p_requestEntry->dataBufInfo.entry, reqSlotTag);

					p_requestEntry->nandInfo.virtualSliceAddr = virtualSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);

					//write
					reqSlotTag = RequestAllocation_GetFreeReqEntry();
					p_requestEntry = RequestAllocation_GetReqEntry(reqSlotTag);

					p_requestEntry->reqType = REQ_TYPE_NAND;
					p_requestEntry->reqCode = REQ_CODE_WRITE;
					p_requestEntry->logicalSliceAddr = logicalSliceAddr;
					p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					p_requestEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					p_requestEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					p_requestEntry->dataBufInfo.entry = BufferManagement_AllocateTempBuf(dieNo);
					BufferManagement_UpdateTempBufEntryInfoBlockingReq(p_requestEntry->dataBufInfo.entry, reqSlotTag);

					p_requestEntry->nandInfo.virtualSliceAddr = FindFreeVirtualSliceForGc(dieNoForGcCopy, victimBlockNo);

					logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = p_requestEntry->nandInfo.virtualSliceAddr;
					virtualSliceMapPtr->virtualSlice[p_requestEntry->nandInfo.virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);
				}
			}
		}
	}

	EraseBlock(dieNo, victimBlockNo);
}
