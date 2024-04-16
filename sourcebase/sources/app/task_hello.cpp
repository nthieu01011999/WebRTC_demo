#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ak.h"
#include "timer.h"

#include "app.h"
#include "app_dbg.h"
#include "app_data.h"

#include "task_list.h"
#include "task_hello.h"

q_msg_t gw_task_hello_mailbox;

void *gw_task_hello_entry(void *) {
	wait_all_tasks_started();

    APP_DBG("[STARTED] gw_task_hello_entry\n");

	ak_msg_t *msg = AK_MSG_NULL;

	// load config here

	// start all module

	while (1) {
		/* get messge */
		msg = ak_msg_rev(GW_TASK_HELLO_ID);

		switch (msg->header->sig) {

		case GW_HELLO_PRINT: {
			APP_DBG_SIG("GW_HELLO_PRINTGW_HELLO_PRINTGW_HELLO_PRINTGW_HELLO_PRINT\n");
		} break;

		case GW_CLOUD_SIGNALING_MQTT_RES: {
			APP_DBG_SIG("GW_CLOUD_SIGNALING_MQTT_RES\n");
			try {
				json revJs = json::parse(string((char *)msg->header->payload, msg->header->len));
				json resJs = {
					{"Method",	   "ACT"				},
					{"MessageType", "Signaling"			},
					{"Serial",	  mtce_getSerialInfo()},
					{"Data",		 revJs["Data"]		  },
					{"Result",		revJs["Result"]	   },
					{"Timestamp",	 time(nullptr)		  }
				   };

				APP_DBG("signaling report: %s\n", resJs.dump().data());
				/* response to mqtt server */
				// if (mospp) {
				// 	mospp->publishSignalingResponse(resJs);
				// 	// mospp->publishResponse(resJs);	  // TODO clear when RELEASE
				// }
			}
			catch (const exception &error) {
				APP_DBG("%s\n", error.what());
			}
		} break;

		default:
			break;
		}

		/* free message */
		ak_msg_free(msg);
	}

	return (void *)0;
}