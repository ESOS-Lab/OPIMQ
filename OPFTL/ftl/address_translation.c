//////////////////////////////////////////////////////////////////////////////////
// address_translation.c for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
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
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Address Translator
// File Name: address translation.c
//
// Version: v1.0.0
//
// Description:
//   - translate address between address space of host system and address space of NAND device
//   - manage bad blocks in NAND device
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include "../memory_map.h"
#include "xil_printf.h"

P_LOGICAL_SLICE_MAP logicalSliceMapPtr = (P_LOGICAL_SLICE_MAP ) LOGICAL_SLICE_MAP_ADDR;
P_VIRTUAL_SLICE_MAP virtualSliceMapPtr = (P_VIRTUAL_SLICE_MAP) VIRTUAL_SLICE_MAP_ADDR;
P_VIRTUAL_BLOCK_MAP virtualBlockMapPtr = (P_VIRTUAL_BLOCK_MAP) VIRTUAL_BLOCK_MAP_ADDR;
P_VIRTUAL_DIE_MAP virtualDieMapPtr = (P_VIRTUAL_DIE_MAP) VIRTUAL_DIE_MAP_ADDR;
P_PHY_BLOCK_MAP phyBlockMapPtr = (P_PHY_BLOCK_MAP) PHY_BLOCK_MAP_ADDR;
P_BAD_BLOCK_TABLE_INFO_MAP bbtInfoMapPtr = (P_BAD_BLOCK_TABLE_INFO_MAP) BAD_BLOCK_TABLE_INFO_MAP_ADDR;

unsigned char sliceAllocationTargetDie;
unsigned int mbPerbadBlockSpace;


static void _EraseTotalBlockSpace()
{
	unsigned int reqSlotTag;

	SSD_REQ_FORMAT* p_reqEntry = NULL;

	xil_printf("Erase total block space...wait for a minute...\r\n");

	for(unsigned int blockNo = 0 ; blockNo < TOTAL_BLOCKS_PER_DIE; blockNo++)
	{
		for(unsigned int dieNo = 0; dieNo < USER_DIES; dieNo++)
		{
			reqSlotTag = RequestAllocation_GetFreeReqEntry();
			p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

			p_reqEntry->reqType = REQ_TYPE_NAND;
			p_reqEntry->reqCode = REQ_CODE_ERASE;
			p_reqEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
			p_reqEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
			p_reqEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
			p_reqEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

			p_reqEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
			p_reqEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
			p_reqEntry->nandInfo.physicalBlock = blockNo;
			p_reqEntry->nandInfo.physicalPage = 0;

			SelectLowLevelReqQ(reqSlotTag);
		}
	}

	RequestScheduler_SyncAllLowLevelReqDone();
	xil_printf("Done.\r\n");
}

static void _ReadBadBlockTable(unsigned int tempBbtBufAddr[], unsigned int tempBbtBufEntrySize)
{
	unsigned int tempPage, reqSlotTag;
	int loop, dataSize;

	SSD_REQ_FORMAT* p_reqEntry = NULL;

	loop = 0;
	dataSize = DATA_SIZE_OF_BAD_BLOCK_TABLE_PER_DIE;
	tempPage = PlsbPage2VpageTranslation(START_PAGE_NO_OF_BAD_BLOCK_TABLE_BLOCK); 	//bad block table is saved at lsb pages

	while (dataSize>0)
	{
		for (unsigned int dieNo = 0; dieNo < USER_DIES; dieNo++)
		{
			reqSlotTag = RequestAllocation_GetFreeReqEntry();
			p_reqEntry = RequestAllocation_GetReqEntry(reqSlotTag);

			p_reqEntry->reqType = REQ_TYPE_NAND;
			p_reqEntry->reqCode = REQ_CODE_READ;
			p_reqEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ADDR;
			p_reqEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
			p_reqEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
			p_reqEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
			p_reqEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
			p_reqEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

			p_reqEntry->dataBufInfo.addr = tempBbtBufAddr[dieNo] + loop * tempBbtBufEntrySize;

			p_reqEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
			p_reqEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
			p_reqEntry->nandInfo.physicalBlock = bbtInfoMapPtr->bbtInfo[dieNo].phyBlock;
			p_reqEntry->nandInfo.physicalPage = Vpage2PlsbPageTranslation(tempPage);

			SelectLowLevelReqQ(reqSlotTag);
		}

		tempPage++;
		loop++;
		dataSize -= BYTES_PER_DATA_REGION_OF_PAGE;
	}

	RequestScheduler_SyncAllLowLevelReqDone();
}

static void _FindBadBlock(unsigned char dieState[], unsigned int tempBbtBufAddr[], unsigned int tempBbtBufEntrySize, unsigned int tempReadBufAddr[], unsigned int tempReadBufEntrySize)
{
	unsigned int phyBlockNo, dieNo, reqSlotTag;
	unsigned char blockChecker[USER_DIES];
	unsigned char* markPointer0;
	unsigned char* markPointer1;
	unsigned char* bbtUpdater;

	SSD_REQ_FORMAT* p_requestEntry = NULL;

	//check bad block mark of each block
	for(phyBlockNo = 0; phyBlockNo < TOTAL_BLOCKS_PER_DIE; phyBlockNo++)
	{
		for(dieNo=0; dieNo < USER_DIES; dieNo++)
		{
			if(!dieState[dieNo])
			{
				blockChecker[dieNo] = BLOCK_STATE_NORMAL;

				reqSlotTag = RequestAllocation_GetFreeReqEntry();
				p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

				p_requestEntry->reqType = REQ_TYPE_NAND;
				p_requestEntry->reqCode = REQ_CODE_READ;
				p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ADDR;
				p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
				p_requestEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_OFF;
				p_requestEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
				p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
				p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

				p_requestEntry->dataBufInfo.addr = tempReadBufAddr[dieNo];

				p_requestEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
				p_requestEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
				p_requestEntry->nandInfo.physicalBlock = phyBlockNo;
				p_requestEntry->nandInfo.physicalPage = BAD_BLOCK_MARK_PAGE0;

				SelectLowLevelReqQ(reqSlotTag);
			}
		}

		RequestScheduler_SyncAllLowLevelReqDone();

		for(dieNo=0; dieNo < USER_DIES; dieNo++)
		{
			if(!dieState[dieNo])
			{
				markPointer0 = (unsigned char*)(tempReadBufAddr[dieNo] + BAD_BLOCK_MARK_BYTE0);
				markPointer1 = (unsigned char*)(tempReadBufAddr[dieNo] + BAD_BLOCK_MARK_BYTE1);

				if((*markPointer0 == CLEAN_DATA_IN_BYTE) && (*markPointer1 == CLEAN_DATA_IN_BYTE))
				{
					reqSlotTag = RequestAllocation_GetFreeReqEntry();
					p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

					p_requestEntry->reqType = REQ_TYPE_NAND;
					p_requestEntry->reqCode = REQ_CODE_READ;
					p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ADDR;
					p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
					p_requestEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_OFF;
					p_requestEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
					p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

					p_requestEntry->dataBufInfo.addr = tempReadBufAddr[dieNo];

					p_requestEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
					p_requestEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
					p_requestEntry->nandInfo.physicalBlock = phyBlockNo;
					p_requestEntry->nandInfo.physicalPage = BAD_BLOCK_MARK_PAGE1;

					SelectLowLevelReqQ(reqSlotTag);
				}
				else
				{
					xil_printf("	bad block is detected: Ch %d Way %d phyBlock %d \r\n",Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), phyBlockNo);

					blockChecker[dieNo] = BLOCK_STATE_BAD;
				}
			}
		}

		RequestScheduler_SyncAllLowLevelReqDone();

		for(dieNo=0; dieNo < USER_DIES; dieNo++)
		{
			if(!dieState[dieNo])
			{
				markPointer0 = (unsigned char*)(tempReadBufAddr[dieNo] + BAD_BLOCK_MARK_BYTE0);
				markPointer1 = (unsigned char*)(tempReadBufAddr[dieNo] + BAD_BLOCK_MARK_BYTE1);

				if(!((*markPointer0 == CLEAN_DATA_IN_BYTE) && (*markPointer1 == CLEAN_DATA_IN_BYTE)))
				{
					if(blockChecker[dieNo] == BLOCK_STATE_NORMAL)
					{
						xil_printf("	bad block is detected: Ch %d Way %d phyBlock %d \r\n",Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), phyBlockNo);

						blockChecker[dieNo] = BLOCK_STATE_BAD;

					}
				}

				bbtUpdater= (unsigned char*)(tempBbtBufAddr[dieNo] + phyBlockNo);
				*bbtUpdater = blockChecker[dieNo];
				phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].bad = blockChecker[dieNo];
			}
		}
	}
}

static void _SaveBadBlockTable(unsigned char dieState[], unsigned int tempBbtBufAddr[], unsigned int tempBbtBufEntrySize)
{
	unsigned int dieNo, reqSlotTag;
	int loop, dataSize, tempPage;

	loop = 0;
	dataSize = DATA_SIZE_OF_BAD_BLOCK_TABLE_PER_DIE;
	tempPage = PlsbPage2VpageTranslation(START_PAGE_NO_OF_BAD_BLOCK_TABLE_BLOCK);	//bad block table is saved at lsb pages

	SSD_REQ_FORMAT* p_requestEntry = NULL;

	while(dataSize>0)
	{
		for(dieNo = 0; dieNo < USER_DIES; dieNo++)
		{
			if((dieState[dieNo] == DIE_STATE_BAD_BLOCK_TABLE_NOT_EXIST) || (dieState[dieNo] == DIE_STATE_BAD_BLOCK_TABLE_UPDATE))
			{
				if(loop == 0)
				{
					reqSlotTag = RequestAllocation_GetFreeReqEntry();
					p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

					p_requestEntry->reqType = REQ_TYPE_NAND;
					p_requestEntry->reqCode = REQ_CODE_ERASE;
					p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
					p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
					p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
					p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

					p_requestEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
					p_requestEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
					p_requestEntry->nandInfo.physicalBlock = bbtInfoMapPtr->bbtInfo[dieNo].phyBlock;
					p_requestEntry->nandInfo.physicalPage = 0;	//dummy

					SelectLowLevelReqQ(reqSlotTag);
				}

				reqSlotTag = RequestAllocation_GetFreeReqEntry();
				p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

				p_requestEntry->reqType = REQ_TYPE_NAND;
				p_requestEntry->reqCode = REQ_CODE_WRITE;
				p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ADDR;
				p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
				p_requestEntry->reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
				p_requestEntry->reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
				p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
				p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;

				p_requestEntry->dataBufInfo.addr = tempBbtBufAddr[dieNo] + loop * tempBbtBufEntrySize;

				p_requestEntry->nandInfo.physicalCh = Vdie2PchTranslation(dieNo);
				p_requestEntry->nandInfo.physicalWay = Vdie2PwayTranslation(dieNo);
				p_requestEntry->nandInfo.physicalBlock = bbtInfoMapPtr->bbtInfo[dieNo].phyBlock;
				p_requestEntry->nandInfo.physicalPage =  Vpage2PlsbPageTranslation(tempPage);

				SelectLowLevelReqQ(reqSlotTag);
			}
		}

		loop++;
		dataSize++;
		dataSize -= BYTES_PER_DATA_REGION_OF_PAGE;
	}

	RequestScheduler_SyncAllLowLevelReqDone();


	for(dieNo=0; dieNo < USER_DIES; dieNo++)
	{
		if(dieState[dieNo] == DIE_STATE_BAD_BLOCK_TABLE_NOT_EXIST)
		{
			xil_printf("[ bad block table of Ch %d Way %d is saved. ]\r\n", dieNo%USER_CHANNELS, dieNo/USER_CHANNELS);
		}
	}
}

static void _RecoverBadBlockTable(unsigned int tempBufAddr)
{
	unsigned int dieNo, phyBlockNo, bbtMaker, tempBbtBufBaseAddr, tempBbtBufEntrySize, tempReadBufBaseAddr, tempReadBufEntrySize;
	unsigned int tempBbtBufAddr[USER_DIES];
	unsigned int tempReadBufAddr[USER_DIES];
	unsigned char dieState[USER_DIES];
	unsigned char* bbtTableChecker;

	//data buffer allocation
	tempBbtBufBaseAddr = tempBufAddr;
	tempBbtBufEntrySize = BYTES_PER_DATA_REGION_OF_PAGE + BYTES_PER_SPARE_REGION_OF_PAGE;
	tempReadBufBaseAddr = tempBbtBufBaseAddr + USER_DIES * USED_PAGES_FOR_BAD_BLOCK_TABLE_PER_DIE * tempBbtBufEntrySize;
	tempReadBufEntrySize = BYTES_PER_NAND_ROW;

	for(dieNo = 0; dieNo < USER_DIES; dieNo++)
	{
		tempBbtBufAddr[dieNo] = tempBbtBufBaseAddr + dieNo * USED_PAGES_FOR_BAD_BLOCK_TABLE_PER_DIE * tempBbtBufEntrySize;
		tempReadBufAddr[dieNo] = tempReadBufBaseAddr + dieNo * tempReadBufEntrySize;
	}

	//read bad block tables
	_ReadBadBlockTable(tempBbtBufAddr, tempBbtBufEntrySize);

	//check bad block tables
	bbtMaker = BAD_BLOCK_TABLE_MAKER_IDLE;
	for(dieNo=0; dieNo<USER_DIES; dieNo++)
	{
		bbtTableChecker = (unsigned char*)(tempBbtBufAddr[dieNo]);

		if((*bbtTableChecker == BLOCK_STATE_NORMAL)||(*bbtTableChecker == BLOCK_STATE_BAD))
		{
			xil_printf("[ bad block table of ch %d way %d exists.]\r\n",Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo));

			dieState[dieNo] = DIE_STATE_BAD_BLOCK_TABLE_EXIST;
			for(phyBlockNo=0; phyBlockNo<TOTAL_BLOCKS_PER_DIE; phyBlockNo++)
			{
				bbtTableChecker = (unsigned char*)(tempBbtBufAddr[dieNo] + phyBlockNo);

				phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].bad = *bbtTableChecker;
				if(phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].bad == BLOCK_STATE_BAD)
				{
					//xil_printf("	bad block: ch %d way %d phyBlock %d  \r\n", Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), phyBlockNo);
				}
			}

			xil_printf("[ bad blocks of ch %d way %d are checked. ]\r\n",Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo));
		}
		else
		{
			xil_printf("[ bad block table of ch %d way %d does not exist.]\r\n",Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo));
			dieState[dieNo] = DIE_STATE_BAD_BLOCK_TABLE_NOT_EXIST;
			bbtMaker = BAD_BLOCK_TABLE_MAKER_TRIGGER;
		}
	}

	//if bad block table does not exist in some dies, make new bad block table for each die having no bad block table
	if(bbtMaker == BAD_BLOCK_TABLE_MAKER_TRIGGER)
	{
		_FindBadBlock(dieState, tempBbtBufAddr, tempBbtBufEntrySize, tempReadBufAddr, tempReadBufEntrySize);
		_SaveBadBlockTable(dieState, tempBbtBufAddr, tempBbtBufEntrySize);
	}

	//grown bad update flag initialization
	for(dieNo=0; dieNo<USER_DIES; dieNo++)
	{
		bbtInfoMapPtr->bbtInfo[dieNo].grownBadUpdate = BBT_INFO_GROWN_BAD_UPDATE_NONE;
	}
}

static void _EraseUserBlockSpace()
{
	unsigned int blockNo, dieNo, reqSlotTag;

	xil_printf("Erase User block space...wait for a minute...\r\n");

	SSD_REQ_FORMAT* p_requestEntry = NULL;

	for(blockNo=0 ; blockNo<USER_BLOCKS_PER_DIE ; blockNo++)
	{
		for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
		{
			if(!virtualBlockMapPtr->block[dieNo][blockNo].bad)
			{
				reqSlotTag = RequestAllocation_GetFreeReqEntry();
				p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

				p_requestEntry->reqType = REQ_TYPE_NAND;
				p_requestEntry->reqCode = REQ_CODE_ERASE;
				p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
				p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
				p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
				p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;

				p_requestEntry->nandInfo.virtualSliceAddr = Vorg2VsaTranslation(dieNo, blockNo, 0);

				SelectLowLevelReqQ(reqSlotTag);
			}
		}
	}

	RequestScheduler_SyncAllLowLevelReqDone();
	xil_printf("Done.\r\n");
}

static void _RemapBadBlock()
{
	unsigned int blockNo, dieNo, remapFlag, maxBadBlockCount;
	unsigned int reservedBlockOfLun0[USER_DIES];
	unsigned int reservedBlockOfLun1[USER_DIES];
	unsigned int badBlockCount[USER_DIES];

	xil_printf("Bad block remapping start...\r\n");

	for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
	{
		reservedBlockOfLun0[dieNo] = USER_BLOCKS_PER_LUN;
		reservedBlockOfLun1[dieNo] = TOTAL_BLOCKS_PER_LUN + USER_BLOCKS_PER_LUN;
		badBlockCount[dieNo] = 0;
	}


	for(blockNo=0 ; blockNo<USER_BLOCKS_PER_LUN ; blockNo++)
	{
		for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
		{
			//lun0
			if(phyBlockMapPtr->phyBlock[dieNo][blockNo].bad)
			{
				if(reservedBlockOfLun0[dieNo] < TOTAL_BLOCKS_PER_LUN)
				{
					remapFlag = 1;
					while(phyBlockMapPtr->phyBlock[dieNo][reservedBlockOfLun0[dieNo]].bad)
					{
						reservedBlockOfLun0[dieNo]++;
						if(reservedBlockOfLun0[dieNo] >= TOTAL_BLOCKS_PER_LUN)
						{
							remapFlag = 0;
							break;
						}
					}

					if(remapFlag)
					{
						phyBlockMapPtr->phyBlock[dieNo][blockNo].remappedPhyBlock  = reservedBlockOfLun0[dieNo];
						reservedBlockOfLun0[dieNo]++;
					}
					else
					{
						xil_printf("No reserved block - Ch %d Way %d virtualBlock %d is bad block \r\n", Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), blockNo);
						badBlockCount[dieNo]++;
					}
				}
				else
				{
					xil_printf("No reserved block - Ch %d Way %d virtualBlock %d is bad block \r\n", Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), blockNo);
					badBlockCount[dieNo]++;
				}
			}

			if (LUNS_PER_DIE > 1)
			{
				//lun1
				if(phyBlockMapPtr->phyBlock[dieNo][blockNo+TOTAL_BLOCKS_PER_LUN].bad)
				{
					if(reservedBlockOfLun1[dieNo] < TOTAL_BLOCKS_PER_DIE)
					{
						remapFlag = 1;
						while(phyBlockMapPtr->phyBlock[dieNo][reservedBlockOfLun1[dieNo]].bad)
						{
							reservedBlockOfLun1[dieNo]++;
							if(reservedBlockOfLun1[dieNo] >= TOTAL_BLOCKS_PER_DIE)
							{
								remapFlag = 0;
								break;
							}
						}

						if(remapFlag)
						{
							phyBlockMapPtr->phyBlock[dieNo][blockNo+TOTAL_BLOCKS_PER_LUN].remappedPhyBlock  = reservedBlockOfLun1[dieNo];
							reservedBlockOfLun1[dieNo]++;
						}
						else
						{
							xil_printf("No reserved block - Ch %x Way %x virtualBlock %d is bad block \r\n",  Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), blockNo+USER_BLOCKS_PER_LUN);
							badBlockCount[dieNo]++;
						}
					}
					else
					{
						xil_printf("No reserved block - Ch %x Way %x virtualBlock %d is bad block \r\n",  Vdie2PchTranslation(dieNo), Vdie2PwayTranslation(dieNo), blockNo+USER_BLOCKS_PER_LUN);
						badBlockCount[dieNo]++;
					}
				}
			}
		}
	}

	xil_printf("Bad block remapping end\r\n");


	maxBadBlockCount = 0;
	for(dieNo=0; dieNo < USER_DIES; dieNo++)
	{
		if(maxBadBlockCount < badBlockCount[dieNo])
		{
			maxBadBlockCount = badBlockCount[dieNo];
		}
	}

	mbPerbadBlockSpace = maxBadBlockCount * USER_DIES * MB_PER_BLOCK;
}

static unsigned int _GetFromFbList(unsigned int dieNo, unsigned int getFreeBlockOption) //fb means free block
{
	unsigned int evictedBlockNo;

	evictedBlockNo = virtualDieMapPtr->die[dieNo].headFreeBlock;

	if(getFreeBlockOption == GET_FREE_BLOCK_NORMAL)
	{
		if(virtualDieMapPtr->die[dieNo].freeBlockCnt <= RESERVED_FREE_BLOCK_COUNT)
		{
			return BLOCK_FAIL;
		}
	}
	else if(getFreeBlockOption == GET_FREE_BLOCK_GC)
	{
		if(evictedBlockNo == BLOCK_NONE)
		{
			return BLOCK_FAIL;
		}
	}
	else
	{
		assert(!"[WARNING] Wrong getFreeBlockOption [WARNING]");
	}

	if(virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock != BLOCK_NONE)
	{
		virtualDieMapPtr->die[dieNo].headFreeBlock = virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock;
		virtualBlockMapPtr->block[dieNo][virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock].prevBlock = BLOCK_NONE;
	}
	else
	{
		virtualDieMapPtr->die[dieNo].headFreeBlock = BLOCK_NONE;
		virtualDieMapPtr->die[dieNo].tailFreeBlock = BLOCK_NONE;
	}

	virtualBlockMapPtr->block[dieNo][evictedBlockNo].free = 0;
	virtualDieMapPtr->die[dieNo].freeBlockCnt--;

	virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock = BLOCK_NONE;
	virtualBlockMapPtr->block[dieNo][evictedBlockNo].prevBlock = BLOCK_NONE;

	return evictedBlockNo;
}

static void _InitBlockMap()
{
	unsigned int dieNo, phyBlockNo, virtualBlockNo, remappedPhyBlock;

	for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
	{
		for(virtualBlockNo=0; virtualBlockNo<USER_BLOCKS_PER_DIE ; virtualBlockNo++)
		{
			phyBlockNo = Vblock2PblockOfTbsTranslation(virtualBlockNo);
			remappedPhyBlock = phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].remappedPhyBlock;
			virtualBlockMapPtr->block[dieNo][virtualBlockNo].bad = phyBlockMapPtr->phyBlock[dieNo][remappedPhyBlock].bad;

			virtualBlockMapPtr->block[dieNo][virtualBlockNo].free = 1;
			virtualBlockMapPtr->block[dieNo][virtualBlockNo].invalidSliceCnt = 0;
			virtualBlockMapPtr->block[dieNo][virtualBlockNo].currentPage = 0;
			virtualBlockMapPtr->block[dieNo][virtualBlockNo].eraseCnt = 0;

			if(virtualBlockMapPtr->block[dieNo][virtualBlockNo].bad)
			{
				virtualBlockMapPtr->block[dieNo][virtualBlockNo].prevBlock = BLOCK_NONE;
				virtualBlockMapPtr->block[dieNo][virtualBlockNo].nextBlock = BLOCK_NONE;
			}
			else
			{
				PutToFbList(dieNo, virtualBlockNo);
			}
		}
	}
}

static void _InitCurrentBlockOfDieMap()
{
	unsigned int dieNo;

	for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
	{
		virtualDieMapPtr->die[dieNo].currentBlock = _GetFromFbList(dieNo, GET_FREE_BLOCK_NORMAL);
		if(virtualDieMapPtr->die[dieNo].currentBlock == BLOCK_FAIL)
		{
			assert(!"[WARNING] There is no free block [WARNING]");
		}
	}
}

static void _InitSliceMap()
{
	int sliceAddr;
	for(sliceAddr=0; sliceAddr<SLICES_PER_SSD ; sliceAddr++)
	{
		logicalSliceMapPtr->logicalSlice[sliceAddr].virtualSliceAddr = VSA_NONE;
		virtualSliceMapPtr->virtualSlice[sliceAddr].logicalSliceAddr = LSA_NONE;
	}
}

static void _InitDieMap()
{
	unsigned int dieNo;

	for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
	{
		virtualDieMapPtr->die[dieNo].headFreeBlock = BLOCK_NONE;
		virtualDieMapPtr->die[dieNo].tailFreeBlock = BLOCK_NONE;
		virtualDieMapPtr->die[dieNo].freeBlockCnt = 0;
	}
}


static void _InitBlockDieMap()
{
	unsigned int dieNo;
	unsigned char eraseFlag = 1;

	xil_printf("Press 'X' to re-make the bad block table.\r\n");
	if (inbyte() == 'X')
	{
		_EraseTotalBlockSpace();
		eraseFlag = 0;
	}

	_InitDieMap();

	//make bad block table
	_RecoverBadBlockTable(RESERVED_DATA_BUFFER_BASE_ADDR);

	//to prevent accessing bbtBlock by host
	for(dieNo=0 ; dieNo<USER_DIES ; dieNo++)
	{
		phyBlockMapPtr->phyBlock[dieNo][bbtInfoMapPtr->bbtInfo[dieNo].phyBlock].bad = 1;
	}

	_RemapBadBlock();

	_InitBlockMap();

	if(eraseFlag)
	{
		_EraseUserBlockSpace();
	}

	_InitCurrentBlockOfDieMap();
}

void InitAddressMap()
{
	//init phyblockMap
	for(unsigned int dieNo = 0 ; dieNo < USER_DIES; dieNo++)
	{
		for(unsigned int blockNo = 0; blockNo < TOTAL_BLOCKS_PER_DIE; blockNo++)
		{
			phyBlockMapPtr->phyBlock[dieNo][blockNo].remappedPhyBlock = blockNo;
		}

		bbtInfoMapPtr->bbtInfo[dieNo].phyBlock = 0;
		bbtInfoMapPtr->bbtInfo[dieNo].grownBadUpdate = BBT_INFO_GROWN_BAD_UPDATE_NONE;
	}

	sliceAllocationTargetDie = FindDieForFreeSliceAllocation();

	_InitSliceMap();
	_InitBlockDieMap();
}

unsigned int AddrTransRead(unsigned int logicalSliceAddr)
{
	unsigned int virtualSliceAddr;

	if(logicalSliceAddr < SLICES_PER_SSD)
	{
		virtualSliceAddr = logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr;

		if(virtualSliceAddr != VSA_NONE)
		{
			return virtualSliceAddr;
		}
		else
		{
			return VSA_FAIL;
		}
	}
	else
	{
		assert(!"[WARNING] Logical address is larger than maximum logical address served by SSD [WARNING]");
	}
}

unsigned int AddrTransWrite(unsigned int logicalSliceAddr)
{
	unsigned int virtualSliceAddr;

	if(logicalSliceAddr < SLICES_PER_SSD)
	{
		InvalidateOldVsa(logicalSliceAddr);

		virtualSliceAddr = FindFreeVirtualSlice();

		logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = virtualSliceAddr;
		virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;

		return virtualSliceAddr;
	}
	else
	{
		assert(!"[WARNING] Logical address is larger than maximum logical address served by SSD [WARNING]");
	}
}

// Jieun add
unsigned int AddrTransSuspen(unsigned int logicalSliceAddr)
{
	unsigned int virtualSliceAddr;

	if(logicalSliceAddr < SLICES_PER_SSD)
	{
//		InvalidateOldVsa(logicalSliceAddr);

		virtualSliceAddr = FindFreeVirtualSlice();

//		logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = virtualSliceAddr;
//		virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;

		return virtualSliceAddr;
	}
	else
	{
		assert(!"[WARNING] Logical address is larger than maximum logical address served by SSD [WARNING]");
	}
}

// Jieun add
void UpdateAddrWrite(unsigned int logicalSliceAddr, unsigned int virtualSliceAddr)
{

	if(logicalSliceAddr < SLICES_PER_SSD)
	{
		InvalidateOldVsa(logicalSliceAddr);

		logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = virtualSliceAddr;
		virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;
	}
	else
	{
		assert(!"[WARNING] Logical address is larger than maximum logical address served by SSD [WARNING]");
	}
}



unsigned int FindFreeVirtualSlice()
{
	unsigned int currentBlock, virtualSliceAddr, dieNo;

	dieNo = sliceAllocationTargetDie;
	currentBlock = virtualDieMapPtr->die[dieNo].currentBlock;

	if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage == USER_PAGES_PER_BLOCK)
	{
		currentBlock = _GetFromFbList(dieNo, GET_FREE_BLOCK_NORMAL);

		if(currentBlock != BLOCK_FAIL)
		{
			virtualDieMapPtr->die[dieNo].currentBlock = currentBlock;
		}
		else
		{
			GarbageCollection_Process(dieNo);

			currentBlock = virtualDieMapPtr->die[dieNo].currentBlock;

			if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage == USER_PAGES_PER_BLOCK)
			{
				currentBlock = _GetFromFbList(dieNo, GET_FREE_BLOCK_NORMAL);
				if(currentBlock != BLOCK_FAIL)
				{
					virtualDieMapPtr->die[dieNo].currentBlock = currentBlock;
				}
				else
				{
					assert(!"[WARNING] There is no available block [WARNING]");
				}
			}
			else if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage > USER_PAGES_PER_BLOCK)
			{
				assert(!"[WARNING] Current page management fail [WARNING]");
			}
		}
	}
	else if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage > USER_PAGES_PER_BLOCK)
	{
		assert(!"[WARNING] Current page management fail [WARNING]");
	}

	virtualSliceAddr = Vorg2VsaTranslation(dieNo, currentBlock, virtualBlockMapPtr->block[dieNo][currentBlock].currentPage);
	virtualBlockMapPtr->block[dieNo][currentBlock].currentPage++;
	sliceAllocationTargetDie = FindDieForFreeSliceAllocation();
	dieNo = sliceAllocationTargetDie;

	return virtualSliceAddr;
}

unsigned int FindFreeVirtualSliceForGc(unsigned int copyTargetDieNo, unsigned int victimBlockNo)
{
	unsigned int currentBlock, virtualSliceAddr, dieNo;

	dieNo = copyTargetDieNo;

	if(victimBlockNo == virtualDieMapPtr->die[dieNo].currentBlock)
	{
		virtualDieMapPtr->die[dieNo].currentBlock = _GetFromFbList(dieNo, GET_FREE_BLOCK_GC);

		if(virtualDieMapPtr->die[dieNo].currentBlock == BLOCK_FAIL)
		{
			assert(!"[WARNING] There is no available block [WARNING]");
		}
	}

	currentBlock = virtualDieMapPtr->die[dieNo].currentBlock;

	if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage == USER_PAGES_PER_BLOCK)
	{

		currentBlock = _GetFromFbList(dieNo, GET_FREE_BLOCK_GC);

		if(currentBlock != BLOCK_FAIL)
		{
			virtualDieMapPtr->die[dieNo].currentBlock = currentBlock;
		}
		else
		{
			assert(!"[WARNING] There is no available block [WARNING]");
		}
	}
	else if(virtualBlockMapPtr->block[dieNo][currentBlock].currentPage > USER_PAGES_PER_BLOCK)
	{
		assert(!"[WARNING] Current page management fail [WARNING]");
	}

	virtualSliceAddr = Vorg2VsaTranslation(dieNo, currentBlock, virtualBlockMapPtr->block[dieNo][currentBlock].currentPage);
	virtualBlockMapPtr->block[dieNo][currentBlock].currentPage++;

	return virtualSliceAddr;
}


unsigned int FindDieForFreeSliceAllocation()
{
	static unsigned char targetCh = 0;
	static unsigned char targetWay = 0;
	unsigned int targetDie;

	targetDie = Pcw2VdieTranslation(targetCh, targetWay);

	if(targetCh != (USER_CHANNELS - 1))
	{
		targetCh = targetCh + 1;
	}
	else
	{
		targetCh = 0;
		targetWay = (targetWay + 1) % USER_WAYS;
	}

	return targetDie;
}

void InvalidateOldVsa(unsigned int logicalSliceAddr)
{
	unsigned int virtualSliceAddr, dieNo, blockNo;

	virtualSliceAddr = logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr;

	if(virtualSliceAddr != VSA_NONE)
	{
		if(virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr != logicalSliceAddr)
		{
			return;
		}

		dieNo = Vsa2VdieTranslation(virtualSliceAddr);
		blockNo = Vsa2VblockTranslation(virtualSliceAddr);

		// unlink
		GarbageCollection_SelectiveGetFromGcVictimList(dieNo, blockNo);

		virtualBlockMapPtr->block[dieNo][blockNo].invalidSliceCnt++;
		logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = VSA_NONE;

		GarbageCollection_AddVictimBlock(dieNo, blockNo, virtualBlockMapPtr->block[dieNo][blockNo].invalidSliceCnt);
	}

}


void EraseBlock(unsigned int dieNo, unsigned int blockNo)
{
	unsigned int pageNo, virtualSliceAddr, reqSlotTag;

	SSD_REQ_FORMAT* p_requestEntry = NULL;

	reqSlotTag = RequestAllocation_GetFreeReqEntry();
	p_requestEntry =  RequestAllocation_GetReqEntry(reqSlotTag);

	p_requestEntry->reqType = REQ_TYPE_NAND;
	p_requestEntry->reqCode = REQ_CODE_ERASE;
	p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
	p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
	p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
	p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
	p_requestEntry->nandInfo.virtualSliceAddr = Vorg2VsaTranslation(dieNo, blockNo, 0);
	p_requestEntry->nandInfo.programmedPageCnt = virtualBlockMapPtr->block[dieNo][blockNo].currentPage;

	SelectLowLevelReqQ(reqSlotTag);

	// block map indicated blockNo initialization
	virtualBlockMapPtr->block[dieNo][blockNo].free = 1;
	virtualBlockMapPtr->block[dieNo][blockNo].eraseCnt++;
	virtualBlockMapPtr->block[dieNo][blockNo].invalidSliceCnt = 0;
	virtualBlockMapPtr->block[dieNo][blockNo].currentPage = 0;

	PutToFbList(dieNo, blockNo);

	for(pageNo=0; pageNo<USER_PAGES_PER_BLOCK; pageNo++)
	{
		virtualSliceAddr = Vorg2VsaTranslation(dieNo, blockNo, pageNo);
		virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr = LSA_NONE;
	}
}

void PutToFbList(unsigned int dieNo, unsigned int blockNo) //fb means free block
{
	if(virtualDieMapPtr->die[dieNo].tailFreeBlock != BLOCK_NONE)
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = virtualDieMapPtr->die[dieNo].tailFreeBlock;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][virtualDieMapPtr->die[dieNo].tailFreeBlock].nextBlock = blockNo;
		virtualDieMapPtr->die[dieNo].tailFreeBlock = blockNo;
	}
	else
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;
		virtualDieMapPtr->die[dieNo].headFreeBlock = blockNo;
		virtualDieMapPtr->die[dieNo].tailFreeBlock = blockNo;
	}

	virtualDieMapPtr->die[dieNo].freeBlockCnt++;
}

void UpdatePhyBlockMapForGrownBadBlock(unsigned int dieNo, unsigned int phyBlockNo)
{
	phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].bad = BLOCK_STATE_BAD;

	bbtInfoMapPtr->bbtInfo[dieNo].grownBadUpdate = BBT_INFO_GROWN_BAD_UPDATE_BOOKED;
}


void UpdateBadBlockTableForGrownBadBlock(unsigned int tempBufAddr)
{
	unsigned int dieNo, phyBlockNo, tempBbtBufBaseAddr, tempBbtBufEntrySize;
	unsigned int tempBbtBufAddr[USER_DIES];
	unsigned char dieState[USER_DIES];
	unsigned char* bbtUpdater;

	//data buffer allocation
	tempBbtBufBaseAddr = tempBufAddr;
	tempBbtBufEntrySize = BYTES_PER_DATA_REGION_OF_PAGE + BYTES_PER_SPARE_REGION_OF_PAGE;
	for(dieNo = 0; dieNo < USER_DIES; dieNo++)
		tempBbtBufAddr[dieNo] = tempBbtBufBaseAddr + dieNo * USED_PAGES_FOR_BAD_BLOCK_TABLE_PER_DIE * tempBbtBufEntrySize;

	//create new bad block table
	for(dieNo = 0; dieNo < USER_DIES; dieNo++)
	{
		if(bbtInfoMapPtr->bbtInfo[dieNo].grownBadUpdate == BBT_INFO_GROWN_BAD_UPDATE_BOOKED)
		{
			for(phyBlockNo = 0; phyBlockNo < TOTAL_BLOCKS_PER_DIE; phyBlockNo++)
			{
				bbtUpdater = (unsigned char*)(tempBbtBufAddr[dieNo] + phyBlockNo);

				if(phyBlockNo != bbtInfoMapPtr->bbtInfo[dieNo].phyBlock)
					*bbtUpdater = phyBlockMapPtr->phyBlock[dieNo][phyBlockNo].bad;
				else
					*bbtUpdater = BLOCK_STATE_NORMAL;
			}

			dieState[dieNo] = DIE_STATE_BAD_BLOCK_TABLE_UPDATE;
		}
		else
			dieState[dieNo] = DIE_STATE_BAD_BLOCK_TABLE_HOLD;
	}

	//update bad block tables in flash
	_SaveBadBlockTable(dieState, tempBbtBufAddr, tempBbtBufEntrySize);
}

