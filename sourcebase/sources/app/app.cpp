#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "ak.h"

#include "app.h"
#include "app_dbg.h"
#include "app_config.h"
#include "app_data.h"
 
CppTime::Timer systemTimer;


void task_init() {
	std::cout << "Hello world" << std::endl;
	mtce_contextInit();

	// USER CODE BEGIN 
}




