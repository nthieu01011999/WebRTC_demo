#ifndef __TASK_AV_H__
#define __TASK_AV_H__

#include "message.h"

// Declare the mailbox and the entry function for the AV task
extern q_msg_t gw_task_av_mailbox;
extern void *gw_task_av_entry(void *);

#endif // __TASK_AV_H__
