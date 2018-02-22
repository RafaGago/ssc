#ifndef __SSC_SIMULATION_LIBEXPORT_H__
#define __SSC_SIMULATION_LIBEXPORT_H__

#include <bl/base/platform.h>

#define SSC_EXPORT

#if defined (BL_GCC)
  #if BL_GCC >= BL_GCC_VER (4, 0, 0)
    #if !defined (SSC_PRIVATE_SYMS)
      #undef SSC_EXPORT
      #define SSC_EXPORT  __attribute__ ((visibility ("default")))
    #endif
  #endif

#elif defined (BL_MSC)
  #if (defined (SSC_SHAREDLIB_COMPILATION) &&\
    !defined (SSC_SHAREDLIB_USING_DEF))

    #undef SSC_EXPORT
    #define SSC_EXPORT __declspec (dllexport)

  #elif (defined (SSC_SHAREDLIB) && !defined (SSC_SHAREDLIB_USING_DEF))

    #undef SSC_EXPORT
    #define SSC_EXPORT __declspec (dllimport)

  #endif
#endif

#endif /* __BL_LIBEXPORT_H__ */

