/*
 * flash_control.c
 *
 *  Created on: 2021. 8. 24.
 *      Author: Park
 */
#include "nsc_driver.h"
#include <assert.h>
#include "xil_printf.h"
#include "../memory_map.h"
#include "../flash/t4nsc_ucode.h"

T4REGS chCtlReg[USER_CHANNELS];

unsigned int NSCS[] = {
	NSC_0_BASEADDR,
	NSC_1_BASEADDR,
	NSC_2_BASEADDR,
	NSC_3_BASEADDR,
	NSC_4_BASEADDR,
	NSC_5_BASEADDR,
	NSC_6_BASEADDR,
	NSC_7_BASEADDR,
};

unsigned int NSC_UCODES[] = {
	NSC_0_UCODEADDR,
	NSC_1_UCODEADDR,
	NSC_2_UCODEADDR,
	NSC_3_UCODEADDR,
	NSC_4_UCODEADDR,
	NSC_5_UCODEADDR,
	NSC_6_UCODEADDR,
	NSC_7_UCODEADDR
};


static void _nfc_install_ucode(unsigned int* bram0)
{
	int i;
	for (i = 0; i < T4NSCu_Common_CodeWordLength; i++)
	{
		bram0[i] = T4NSCuCode_Common[i];
	}
	for (i = 0; i < T4NSCu_PlainOps_CodeWordLength; i++)
	{
		bram0[T4NSCu_Common_CodeWordLength + i] = T4NSCuCode_PlainOps[i];
	}
}

static void _init_channel_control_register()
{
	if(USER_CHANNELS < 1)
	{
		assert(!"[WARNING] Configuration Error: Channel [WARNING]");
	}

	for (unsigned int chNo = 0; chNo < USER_CHANNELS; chNo++)
	{
		_nfc_install_ucode(NSC_UCODES[chNo]);
		V2FInitializeHandle(&chCtlReg[chNo], NSCS[chNo]);
	}
}

static void _set_req_entry_for_nand_reset(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	SSD_REQ_FORMAT* p_requestEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	p_requestEntry->reqType = REQ_TYPE_NAND;
	p_requestEntry->reqCode = REQ_CODE_RESET;
	p_requestEntry->reqOpt.nandAddr = REQ_OPT_NAND_ADDR_PHY_ORG;
	p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
	p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
	p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;
	p_requestEntry->nandInfo.physicalCh = chNo;
	p_requestEntry->nandInfo.physicalWay = wayNo;
	p_requestEntry->nandInfo.physicalBlock = 0;	//dummy
	p_requestEntry->nandInfo.physicalPage = 0;	//dummy
	p_requestEntry->prevBlockingReq = REQ_SLOT_TAG_NONE;
}

static void _set_req_entry_for_set_feature(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	SSD_REQ_FORMAT* p_requestEntry = RequestAllocation_GetReqEntry(reqSlotTag);

	p_requestEntry->reqType = REQ_TYPE_NAND;
	p_requestEntry->reqCode = REQ_CODE_SET_FEATURE;
	p_requestEntry->reqOpt.nandAddr =  REQ_OPT_NAND_ADDR_PHY_ORG;
	p_requestEntry->reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_NONE;
	p_requestEntry->reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_NONE;
	p_requestEntry->reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_TOTAL;
	p_requestEntry->nandInfo.physicalCh = chNo;
	p_requestEntry->nandInfo.physicalWay = wayNo;
	p_requestEntry->nandInfo.physicalBlock = 0;	//dummy
	p_requestEntry->nandInfo.physicalPage = 0;	//dummy
	p_requestEntry->prevBlockingReq = REQ_SLOT_TAG_NONE;
}

void flash_init_nand()
{
	unsigned int reqSlotTag;

	for (unsigned int chNo = 0; chNo < USER_CHANNELS; ++chNo)
	{
		for (unsigned int wayNo = 0; wayNo < USER_WAYS; ++wayNo)
		{
			reqSlotTag = RequestAllocation_GetFreeReqEntry();

			_set_req_entry_for_nand_reset(reqSlotTag, chNo, wayNo);

			SelectLowLevelReqQ(reqSlotTag);

			reqSlotTag = RequestAllocation_GetFreeReqEntry();

			_set_req_entry_for_set_feature(reqSlotTag, chNo, wayNo);

			SelectLowLevelReqQ(reqSlotTag);
		}
	}

	RequestScheduler_SyncAllLowLevelReqDone();

	xil_printf("[ NAND device reset complete. ]\r\n");
}

void flash_init()
{
	_init_channel_control_register();
	flash_init_nand();
}

void flash_task_run()
{
	if ((0 != notCompletedNandReqCnt)
		|| (0 != blockedReqCnt))
	{
		RequestScheduler_SchedulingNandReq();
	}
}
