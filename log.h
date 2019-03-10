/* log.h */

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include "glog/logging.h"
using namespace google;

#pragma comment(lib, "glog/libglog.lib")

#define		logInfo		LOG(INFO)
#define     logError    LOG(ERROR)
#define		logEvent	LogEvent
