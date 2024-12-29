/*
 * lld_hdma.c
 *
 *  Created on: 2021. 8. 15.
 *      Author: Park
 */
#include "../../debug.h"
#include "xil_printf.h"
#include "../../host/nvme/io_access.h"
#include "lld_hdma.h"

HOST_DMA_STATUS g_hostDmaStatus;
HOST_DMA_ASSIST_STATUS g_hostDmaAssistStatus;

void lld_hdma_set_direct_tx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len)
{
	ASSERT((len <= 0x1000) && ((pcieAddrL & 0x3) == 0)); //modified

	HOST_DMA_CMD_FIFO_REG hostDmaReg;

	memset((void*)&hostDmaReg, 0x00, sizeof(HOST_DMA_CMD_FIFO_REG));

	hostDmaReg.devAddr = devAddr;
	hostDmaReg.pcieAddrL = pcieAddrL;
	hostDmaReg.pcieAddrH = pcieAddrH;

	hostDmaReg.dword[3] = 0;
	hostDmaReg.dmaType = HOST_DMA_DIRECT_TYPE;
	hostDmaReg.dmaDirection = HOST_DMA_TX_DIRECTION;
	hostDmaReg.dmaLen = len;

	IO_WRITE32(HOST_DMA_CMD_FIFO_REG_ADDR, hostDmaReg.dword[0]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 4), hostDmaReg.dword[1]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 8), hostDmaReg.dword[2]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 12), hostDmaReg.dword[3]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 16), hostDmaReg.dword[4]);//slot_modified

	g_hostDmaStatus.fifoTail.directDmaTx++;
	g_hostDmaStatus.directDmaTxCnt++;
}

void lld_hdma_set_direct_rx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len)
{
	ASSERT((len <= 0x1000) && ((pcieAddrL & 0x3) == 0)); //modified

	HOST_DMA_CMD_FIFO_REG hostDmaReg;

	memset((void*)&hostDmaReg, 0x00, sizeof(HOST_DMA_CMD_FIFO_REG));

	hostDmaReg.devAddr = devAddr;
	hostDmaReg.pcieAddrH = pcieAddrH;
	hostDmaReg.pcieAddrL = pcieAddrL;

	hostDmaReg.dword[3] = 0;
	hostDmaReg.dmaType = HOST_DMA_DIRECT_TYPE;
	hostDmaReg.dmaDirection = HOST_DMA_RX_DIRECTION;
	hostDmaReg.dmaLen = len;

	IO_WRITE32(HOST_DMA_CMD_FIFO_REG_ADDR, hostDmaReg.dword[0]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 4), hostDmaReg.dword[1]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 8), hostDmaReg.dword[2]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 12), hostDmaReg.dword[3]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 16), hostDmaReg.dword[4]);//slot_modified
	g_hostDmaStatus.fifoTail.directDmaRx++;
	g_hostDmaStatus.directDmaRxCnt++;

}

void lld_hdma_set_auto_tx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion)
{
	HOST_DMA_CMD_FIFO_REG hostDmaReg;
	unsigned char tempTail;

	ASSERT(cmd4KBOffset < 256);

	g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	while((g_hostDmaStatus.fifoTail.autoDmaTx + 1) % 256 == g_hostDmaStatus.fifoHead.autoDmaTx)
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);

	hostDmaReg.devAddr = devAddr;

	hostDmaReg.dword[3] = 0;
	hostDmaReg.dmaType = HOST_DMA_AUTO_TYPE;
	hostDmaReg.dmaDirection = HOST_DMA_TX_DIRECTION;
	hostDmaReg.cmd4KBOffset = cmd4KBOffset;
	hostDmaReg.cmdSlotTag = cmdSlotTag;
	hostDmaReg.autoCompletion = autoCompletion;

	IO_WRITE32(HOST_DMA_CMD_FIFO_REG_ADDR, hostDmaReg.dword[0]);
	//IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 4), hostDmaReg.dword[1]);
	//IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 8), hostDmaReg.dword[2]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 12), hostDmaReg.dword[3]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 16), hostDmaReg.dword[4]);//slot_modified

	tempTail = g_hostDmaStatus.fifoTail.autoDmaTx++;
	if(tempTail > g_hostDmaStatus.fifoTail.autoDmaTx)
		g_hostDmaAssistStatus.autoDmaTxOverFlowCnt++;

	g_hostDmaStatus.autoDmaTxCnt++;
}

void lld_hdma_set_auto_rx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion)
{
	HOST_DMA_CMD_FIFO_REG hostDmaReg;
	unsigned char tempTail;

	ASSERT(cmd4KBOffset < 256);

	g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	while((g_hostDmaStatus.fifoTail.autoDmaRx + 1) % 256 == g_hostDmaStatus.fifoHead.autoDmaRx)
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);

	hostDmaReg.devAddr = devAddr;

	hostDmaReg.dword[3] = 0;
	hostDmaReg.dmaType = HOST_DMA_AUTO_TYPE;
	hostDmaReg.dmaDirection = HOST_DMA_RX_DIRECTION;
	hostDmaReg.cmd4KBOffset = cmd4KBOffset;
	hostDmaReg.cmdSlotTag = cmdSlotTag;
	hostDmaReg.autoCompletion = autoCompletion;

	IO_WRITE32(HOST_DMA_CMD_FIFO_REG_ADDR, hostDmaReg.dword[0]);
	//IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 4), hostDmaReg.dword[1]);
	//IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 8), hostDmaReg.dword[2]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 12), hostDmaReg.dword[3]);
	IO_WRITE32((HOST_DMA_CMD_FIFO_REG_ADDR + 16), hostDmaReg.dword[4]);//slot_modified

	tempTail = g_hostDmaStatus.fifoTail.autoDmaRx++;
	if(tempTail > g_hostDmaStatus.fifoTail.autoDmaRx)
		g_hostDmaAssistStatus.autoDmaRxOverFlowCnt++;

	g_hostDmaStatus.autoDmaRxCnt++;
}

void lld_hdma_check_direct_tx_dma_done()
{
	while(g_hostDmaStatus.fifoHead.directDmaTx != g_hostDmaStatus.fifoTail.directDmaTx)
	{
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	}
}

void lld_hdma_check_direct_rx_dma_done()
{
	while(g_hostDmaStatus.fifoHead.directDmaRx != g_hostDmaStatus.fifoTail.directDmaRx)
	{
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	}
}

void lld_hdma_check_auto_tx_dma_done()
{
	while(g_hostDmaStatus.fifoHead.autoDmaTx != g_hostDmaStatus.fifoTail.autoDmaTx)
	{
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	}
}

void lld_hdma_check_auto_rx_dma_done()
{
	while(g_hostDmaStatus.fifoHead.autoDmaRx != g_hostDmaStatus.fifoTail.autoDmaRx)
	{
		g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);
	}
}

unsigned int lld_hdma_check_auto_tx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex)
{
	//xil_printf("check_auto_tx_dma_partial_done \r\n");

	g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);

	if(g_hostDmaStatus.fifoHead.autoDmaTx == g_hostDmaStatus.fifoTail.autoDmaTx)
		return 1;

	if(g_hostDmaStatus.fifoHead.autoDmaTx < tailIndex)
	{
		if(g_hostDmaStatus.fifoTail.autoDmaTx < tailIndex)
		{
			if(g_hostDmaStatus.fifoTail.autoDmaTx > g_hostDmaStatus.fifoHead.autoDmaTx)
				return 1;
			else
				if(g_hostDmaAssistStatus.autoDmaTxOverFlowCnt != (tailAssistIndex + 1))
					return 1;
		}
		else
			if(g_hostDmaAssistStatus.autoDmaTxOverFlowCnt != tailAssistIndex)
				return 1;

	}
	else if(g_hostDmaStatus.fifoHead.autoDmaTx == tailIndex)
		return 1;
	else
	{
		if(g_hostDmaStatus.fifoTail.autoDmaTx < tailIndex)
			return 1;
		else
		{
			if(g_hostDmaStatus.fifoTail.autoDmaTx > g_hostDmaStatus.fifoHead.autoDmaTx)
				return 1;
			else
				if(g_hostDmaAssistStatus.autoDmaTxOverFlowCnt != tailAssistIndex)
					return 1;
		}
	}

	return 0;
}

unsigned int lld_hdma_check_auto_rx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex)
{
	//xil_printf("check_auto_rx_dma_partial_done \r\n");

	g_hostDmaStatus.fifoHead.dword = IO_READ32(HOST_DMA_FIFO_CNT_REG_ADDR);

	if(g_hostDmaStatus.fifoHead.autoDmaRx == g_hostDmaStatus.fifoTail.autoDmaRx)
		return 1;

	if(g_hostDmaStatus.fifoHead.autoDmaRx < tailIndex)
	{
		if(g_hostDmaStatus.fifoTail.autoDmaRx < tailIndex)
		{
			if(g_hostDmaStatus.fifoTail.autoDmaRx > g_hostDmaStatus.fifoHead.autoDmaRx)
				return 1;
			else
				if(g_hostDmaAssistStatus.autoDmaRxOverFlowCnt != (tailAssistIndex + 1))
					return 1;
		}
		else
			if(g_hostDmaAssistStatus.autoDmaRxOverFlowCnt != tailAssistIndex)
				return 1;

	}
	else if(g_hostDmaStatus.fifoHead.autoDmaRx == tailIndex)
		return 1;
	else
	{
		if(g_hostDmaStatus.fifoTail.autoDmaRx < tailIndex)
			return 1;
		else
		{
			if(g_hostDmaStatus.fifoTail.autoDmaRx > g_hostDmaStatus.fifoHead.autoDmaRx)
				return 1;
			else
				if(g_hostDmaAssistStatus.autoDmaRxOverFlowCnt != tailAssistIndex)
					return 1;
		}
	}

	return 0;
}

unsigned int lld_hdma_get_dma_tail_fifo_cnt(void)
{
	return g_hostDmaStatus.fifoTail.dword;
}

unsigned int lld_hdma_get_auto_dma_tx_overflow_cnt(void)
{
	return g_hostDmaAssistStatus.autoDmaTxOverFlowCnt;
}

unsigned int lld_hdma_get_auto_dma_rx_overflow_cnt(void)
{
	return g_hostDmaAssistStatus.autoDmaRxOverFlowCnt;
}
