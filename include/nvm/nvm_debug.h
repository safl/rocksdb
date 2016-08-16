#ifndef _NVM_DEBUG_H_
#define _NVM_DEBUG_H_

#include <stdio.h>

#define NVM_DEBUG_ENABLED

#define NVM_ASSERT(c, x, ...) if(!(c)){printf("%s:%s - %d %s" x "\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno), ##__VA_ARGS__);fflush(stdout);exit(EXIT_FAILURE);}
#define NVM_ERROR(x, ...) printf("%s:%s - %d %s" x "\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno), ##__VA_ARGS__);fflush(stdout);
#define NVM_FATAL(x, ...) printf("%s:%s - %d %s" x "\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno), ##__VA_ARGS__);fflush(stdout);exit(EXIT_FAILURE)

#ifdef NVM_DEBUG_ENABLED

#define NVM_DEBUG(x, ...) printf("%s:%s - %d " x "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);fflush(stdout);


#else

#define NVM_DEBUG(x, ...)

#endif

#endif
