/*
 * flash_control.h
 *
 *  Created on: 2021. 8. 24.
 *      Author: Park
 */

#ifndef _FLASH_CONTROL_H_
#define _FLASH_CONTROL_H_

void flash_init_channel_control_register();
void flash_init_nand();

void flash_init();

void flash_task_run();

#endif /* _FLASH_CONTROL_H_ */
