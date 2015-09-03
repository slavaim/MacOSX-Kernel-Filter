/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDINTERNALLOG_H
#define DLDINTERNALLOG_H

#include "DldCommon.h"
#include <sys/vnode.h>

//--------------------------------------------------------------------

class DldInternalLog: public OSObject
{    
    OSDeclareDefaultStructors( DldInternalLog )
    
public:
    
    static DldInternalLog* withFileName( const char* logFileName );
    
    //
    // all parameters must be from wired memory!
    //
    void Log(const char *fmt, ...);
    
    //
    // the following power function must be idempotent in its behaviour
    //
    void prepareForPowerOff();
    
private:
    
    thread_t                        mInternalLogThread;
    vnode_t                         mLogFileVnode;
    uint64_t                        mFileDataSize;
    off_t                           mValidDataOffset;
    vfs_context_t                   mContext; // OPTIONAL can be NULL, in that case the current thread context is being used
    static const mach_vm_size_t     mBufferSize = 256*PAGE_SIZE;
    static const char*              mDigs;
    bool     	                    _doprnt_truncates;
    bool                            mStopThread;
    bool                            mPowerIsBeingOff;
    bool                            mFlushingStopped;// this means the thread is stopped and the file is closed
    //
    // only a spin lock is suitable, the log function might be called
    // with preemption disabled
    //
    IOSimpleLock*                   mLock;
#if defined( DBG )
    thread_t                        mThreadHoldingLock;
#endif
    
    typedef struct _DldLogBufferDscr{
        
        //
        // the absolute indices are used, see http://en.wikipedia.org/wiki/Circular_buffer for references
        //
        mach_vm_offset_t    start;
        mach_vm_offset_t    end;
        char*               buffer;
        
    } DldLogBufferDscr;
    
    DldLogBufferDscr                 mBuffer;
    
private:
    
    bool
    fStartInternalLogging( __in const char*  logFileName );
    
    void
    fReleaseResources();
    
    errno_t
    fIncreaseFileDataSize( __in uint64_t add );
    
    void
    fLogPutcLocked( __in char c );
    
    int
    printnum( __in unsigned long long int	u,	/* number to print */
              __in int		base
             );
    
    static
    void
    DldInternalLogThread( void* __this );
    
    //
    // stops the thread and closes the file thus flushing the cache
    //
    void
    fStopFlushing();
    
    int
    doprnt(
             const char	*fmt,
             va_list    argp,
             int        radix);		/* default radix - for '%r' */
    
    void fLock();
    void fUnlock();
    
protected:
    
    virtual void free();
};

//--------------------------------------------------------------------

#endif//DLDINTERNALLOG_H