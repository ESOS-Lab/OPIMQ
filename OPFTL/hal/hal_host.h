/*
 * hal_host.h
 *
 *  Created on: 2021. 8. 17.
 *      Author: Park
 */

#ifndef _HAL_HOST_H_
#define _HAL_HOST_H_

#include "../host/nvme/nvme.h"
#include "../host/nvme/nvme_main.h"

#define NVME_COMMAND_AUTO_COMPLETION_OFF	0
#define NVME_COMMAND_AUTO_COMPLETION_ON		1

void hal_host_issue_hdma_req(unsigned int reqSlotTag);
void hal_host_handle_hdma_result();
unsigned int hal_host_set_nvme_ccen(void);
unsigned int hal_host_clear_nvme_ccen(void);
unsigned int hal_host_nvme_shutdown(void);
unsigned int hal_host_fetch_nvme_command(NVME_COMMAND_ENTRY* p_cmdContext);
void hal_host_completion_nvme_command(unsigned int cmd_slot_tag, unsigned int cmd_specific, unsigned short status_field);

#endif /* _HAL_HOST_H_ */
