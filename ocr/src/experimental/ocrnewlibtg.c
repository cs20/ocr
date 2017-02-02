#include "ocr-config.h"
#ifdef ENABLE_NEWLIB_SCAFFOLD_TG
#include <ocr.h>
#include <extensions/ocr-legacy.h>

#include <unistd.h>
#include <sys/stat.h>

#define _NOARGS void

//
// Note: This file needs to be compiled with the platform (Linux) includes
//       instead of newlib's since we need to use the platform syscalls.
//       However, this leads to mismatches in newlib vs. platform types,
//       structures, and flags which we need to translate back and forth.

#define ocr_assert(expr)  // for now no realization

void __assert_func(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// OCR SAL methods
//

u8 ocrUSalOpen( ocrGuid_t legacyContext, ocrGuid_t* handle,
                const char * file, s32 flags, s32 mode )
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( handle != NULL );
    ocr_assert( file != NULL );

#if 0
    int retval = open( file, flags, mode );

    if( retval >= 0 ) {
        ((ocrGuid_t *)handle)->guid = setGuidType( retval, GUID_FD );
        add_guid( *handle, 0, 0 );
    }


    return retval < 0;
#endif
    return 0;
}

u8 ocrUSalClose(ocrGuid_t legacyContext, ocrGuid_t handle)
{
    if( ocrGuidIsNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

//    return close( getGuidValue(handle) ) < 0;
    return 0;
}

u8 ocrUSalRead( ocrGuid_t legacyContext, ocrGuid_t handle,
                s32 *readCount, char * ptr, s32 len)
{
    if( ocrGuidIsNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

    PRINTF("Read %d bytes\n", len);

#if 0
    int nread = read( getGuidValue(handle), (void *) ptr, (size_t) len );

    if( nread >= 0 )
        *readCount = nread;

    return nread < 0;
#endif
    return 0;
}

u8 ocrUSalWrite( ocrGuid_t legacyContext, ocrGuid_t handle,
                 s32 *wroteCount, const char * ptr, s32 len)
{
    if( ocrGuidIsNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );

    PRINTF("Write %p %d bytes\n", ptr, len);

#if 0
    int nwritten = write( getGuidValue(handle), (const void *) ptr, (size_t) len );

    if( nwritten >= 0 )
        *wroteCount = nwritten;

    return nwritten < 0;
#endif
    return 0;
}

//
// debugging hook
//
void do_catch()
{
//    ocr_errno = 0;
}

u8 ocrUSalChown(ocrGuid_t legacyContext, const char* path, uid_t owner, gid_t group)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}

//
// mode_t mappings seem to match between newlib and Linux. So no translation
// between newlib and Linux needed.
//
u8 ocrUSalChmod(ocrGuid_t legacyContext, const char* path, mode_t mode)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrUSalChdir(ocrGuid_t legacyContext, const char* path)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrIsAtty (s32 file)
{
    do_catch(); return 0;
}
s64 ocrReadlink (ocrGuid_t legacyContext, const char *path, char *buf, size_t bufsize)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrUSalSymlink(ocrGuid_t legacyContext, const char* path1, const char* path2)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrUSalLink(ocrGuid_t legacyContext, const char* existing, const char* new)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrUSalUnlink(ocrGuid_t legacyContext, const char* name)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
u8 ocrUSalGetcwd(ocrGuid_t legacyContext, char* buf, u64 bufSize)
{
    return 0;
}
//
// This file is compiled with the Linux host includes and it's struct stat
// is a different size and order than newlib's. So we do a manual copy-over
// to avoid trashing our stack, etc.
//
u8 ocrUSalFStat(ocrGuid_t legacyContext, ocrGuid_t handle, struct stat* st)
{
    if( ocrGuidIsNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );
    ocr_assert( st != NULL );
    return 0;
}
u8 ocrUSalStat(ocrGuid_t legacyContext, const char* file, struct stat* st)
{
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    return 0;
}
s64 ocrUSalLseek(ocrGuid_t legacyContext, ocrGuid_t handle, s64 offset, s32 whence)
{
    if( ocrGuidIsNull(handle) )
        return -1;
    ocr_assert( isGuidType( legacyContext, GUID_CONTEXT ) );
    ocr_assert( isGuidType( handle, GUID_FD ) );
    return 0;
}

u8 ocrFork (_NOARGS)
{
    return -1;
}
u8 ocrExecve (char *name, char **argv, char **env)
{
    return -1;
}
u8 ocrGetPID (_NOARGS)
{
    return 0;
}
u8 ocrKill (s32 pid, s32 sig)
{
    return -1; //(u8) (kill( pid, sig ) == -1);
}
u8 ocrGetTimeofDay (struct timeval  *ptimeval, void *ptimezone)
{
    return 0;
}

#endif
