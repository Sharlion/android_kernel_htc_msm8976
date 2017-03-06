
#ifndef _FUSB30X_FSCTYPES_H_
#define _FUSB30X_FSCTYPES_H_

#define FSC_HOSTCOMM_BUFFER_SIZE    64  

#if defined(FSC_PLATFORM_LINUX)

#if defined(__GNUC__)
#define __EXTENSION __extension__
#else
#define __EXTENSION
#endif

#if !defined(__PACKED)
    #define __PACKED
#endif

#include <linux/types.h>

#ifndef U8_MAX
#define U8_MAX  ((__u8)~0U)
#endif

#if !defined(BOOL) && !defined(FALSE) && !defined(TRUE)
typedef enum _BOOL { FALSE = 0, TRUE } FSC_BOOL;    
#endif 

#ifndef FSC_S8
typedef __s8                FSC_S8;                                            
#endif 

#ifndef FSC_S16
typedef __s16               FSC_S16;                                           
#endif 

#ifndef FSC_S32
typedef __s32               FSC_S32;                                           
#endif 

#ifndef FSC_S64
typedef __s64               FSC_S64;                                           
#endif 

#ifndef FSC_U8
typedef __u8                FSC_U8;                                            
#endif 

#ifndef FSC_U16
typedef __u16               FSC_U16;                                           
#endif 

#ifndef FSC_U32
typedef __u32               FSC_U32;                                           
#endif 

#undef __EXTENSION

#endif 
 
#endif 

