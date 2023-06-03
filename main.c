/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-04-29     tyustli      first version
 */

#include "MIMXRT1062.h"
#include <rtdevice.h>
#include "drv_gpio.h"
#include "core_cm7.h"

int main(void)
{

}

void reboot(void)
{
    NVIC_SystemReset();
}
MSH_CMD_EXPORT(reboot, reset system)
