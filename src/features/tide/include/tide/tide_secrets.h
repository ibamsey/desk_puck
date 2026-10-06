#ifndef TIDE_TIDE_SECRETS_H
#define TIDE_TIDE_SECRETS_H

#if __has_include("tide/tide_config_private.h")
#include "tide/tide_config_private.h"
#endif

#ifndef TIDE_CCO_API_KEY
/** CCO Swagger dev key (channelcoast.org); replace via tide_config_private.h for production. */
#define TIDE_CCO_API_KEY "6cefd36d8e12a4dead4cf06d4dbd09c0"
#endif

#ifndef TIDE_CCO_REFERER
#define TIDE_CCO_REFERER "https://www.coastalmonitoring.org/"
#endif

#endif // TIDE_TIDE_SECRETS_H
