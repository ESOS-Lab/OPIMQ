/*
 * lld_pcie.h
 *
 *  Created on: 2021. 8. 15.
 *      Author: Park
 */

#ifndef _LLD_PCIE_H_
#define _LLD_PCIE_H_


void lld_pcie_async_reset(unsigned int rstCnt);
void lld_pcie_set_link_width(unsigned int linkNum);

#endif /* _LLD_PCIE_H_ */
