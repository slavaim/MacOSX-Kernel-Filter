/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

/*  This version implements the following printf features:
*
*   %p  hexadecimal pointer
*	%d	decimal conversion
*	%u	unsigned conversion
*   %lu unsigned long long ( 64 bit )
*	%x	hexadecimal conversion
*	%X	hexadecimal conversion with capital letters
*      %D      hexdump, ptr & separator string ("%6D", ptr, ":") -> XX:XX:XX:XX:XX:XX
*              if you use, "%*D" then there's a length, the data ptr and then the separator
*	%o	octal conversion
*	%c	character
*	%s	string
*	%m.n	field width, precision
*	%-m.n	left adjustment
*	%0m.n	zero-padding
*	%*.*	width and precision taken from arguments
*
*  This version does not implement %f, %e, or %g.
*/

#include "DldInternalLog.h"
#include <sys/fcntl.h>
#include "DldUndocumentedQuirks.h"

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldInternalLog, OSObject )

//--------------------------------------------------------------------

const char* DldInternalLog::mDigs = "0123456789abcdef";

//--------------------------------------------------------------------

DldInternalLog*
DldInternalLog::withFileName( const char* logFileName )
{
    assert( logFileName );
    
    if( !logFileName ){
        
        DBG_PRINT_ERROR_TO_ASL(("logFileName is NULL"));
        return NULL;
    }
    
    DldInternalLog* newLog = new  DldInternalLog();
    assert( newLog );
    if( !newLog ){
        
        DBG_PRINT_ERROR_TO_ASL(("allocation failed\n"));
        return NULL;
    }
    
    if( !newLog->init() ){
        
        DBG_PRINT_ERROR_TO_ASL(("init() failed\n"));
        newLog->release();
        return NULL;
    }
    
    if( !newLog->fStartInternalLogging( logFileName ) ){
        
        DBG_PRINT_ERROR_TO_ASL(("logging dtart for %s file failed\n", logFileName ));
        newLog->release();
        return NULL;
    }
    
    return newLog;
}

//--------------------------------------------------------------------

void DldInternalLog::free()
{
    this->fReleaseResources();
    super::free();
}

//--------------------------------------------------------------------

bool
DldInternalLog::fStartInternalLogging(
    __in const char*  logFileName
    )
{
    bool            init = false;
    int             error;
    
    //
    // allocate the buffer
    //
    this->mBuffer.buffer = (char*)IOMalloc( (vm_size_t)this->mBufferSize );
    assert( this->mBuffer.buffer );
    if( !this->mBuffer.buffer ){
        
        DBG_PRINT_ERROR_TO_ASL(("IOMalloc( (vm_size_t)this->mBufferSize ) failed\n"));
        goto __exit;
    }
    
    this->mLock = IOSimpleLockAlloc();
    assert( this->mLock );
    if( NULL == this->mLock ){
        
        DBG_PRINT_ERROR_TO_ASL(("IOSimpleLockAlloc() failed\n"));
        goto __exit;
    }
    
    //
    // set the context to NULL so the current thread context will be used for all operations,
    // this should not be a problem as the file access is done from the system thread,
    // saving the current thread vfs context is dangerous as the thread can be terminated
    // thus invalidating all pointers to the thread structure
    //
    this->mContext = NULL;
    /*
    this->mContext = vfs_context_create(NULL);
    assert( this->mContext );
    if( NULL == this->mContext ){
        
        DBG_PRINT_ERROR_TO_ASL(("vfs_context_create(NULL) failed\n"));
        goto __exit;
    }
     */
    
    //
    // create the file, truncate an existing file
    //
    error = vnode_open( logFileName,
                        (O_CREAT | O_TRUNC | FWRITE | O_NOFOLLOW),
                        (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), VNODE_LOOKUP_NOFOLLOW,
                        &this->mLogFileVnode,
                        this->mContext ? this->mContext : vfs_context_current() );
    
    assert( !error );
    if( error ){
        
        DBG_PRINT_ERROR_TO_ASL(( "vnode_open(%s) failed with the 0x%x error\n", logFileName, error ));
        goto __exit;
    }
    
    //
    // vn_open returns with both a use_count
    // and an io_count on the found vnode
    // drop the io_count, but keep the use_count
    //
    
    //
    // decrement the io count, the user count is still bumped,
    // this prevents from stalling on shutdown
    //
    vnode_put( this->mLogFileVnode );
    
    //
    // set the initial size
    //
    error = this->fIncreaseFileDataSize( 2*this->mBufferSize );
    if( error ){
        
        DBG_PRINT_ERROR_TO_ASL(("fIncreaseFileDataSize( %d ) failed\n", 2*(int)this->mBufferSize));
        goto __exit;
    }
    
    //
    // strat the writing thread
    //
    kern_return_t   result;       
    
	result = kernel_thread_start( ( thread_continue_t ) &DldInternalLog::DldInternalLogThread,
                                  this,
                                  &this->mInternalLogThread );
	assert( KERN_SUCCESS == result );
    if ( KERN_SUCCESS != result ) {
        
        DBG_PRINT_ERROR_TO_ASL(("kernel_thread_start() failed, error=0x%x\n", result));
    }
    
    init = true;
    
__exit:
    
    return init;
}

//--------------------------------------------------------------------

void
DldInternalLog::fStopFlushing()
{
    if( this->mFlushingStopped )
        return;
    
    if( this->mInternalLogThread ){
        
        int cookie;
        
        cookie = DldDisablePreemption();
        {
            //
            // the waiting is with a timeout so the preemtion disabling is not a requirement
            // but used to provide a more deterministic algorithm
            //
            assert_wait_timeout( (event_t)&this->mInternalLogThread,
                                 THREAD_UNINT,
                                 3000, /*3 secs*/
                                 1000*NSEC_PER_USEC );
            this->mStopThread = true;
            thread_wakeup( (event_t)&this->mBuffer.buffer );
        }
        DldEnablePreemption( cookie );
        
        thread_block( NULL );
        
        //
        // release the thread object
        //
        thread_deallocate( this->mInternalLogThread );
    }
    
    //
    // decrement the user count
    //
    if( this->mLogFileVnode ){
        
        int error;
        
        //
        // the io_count was dropped just after the vnode creation,
        // so we need to bump it as vnode_close() drops
        // it again, in case of shutdown the vnode has been drained
        // to this moment so vnode_getwithref() fails
        //
        error = vnode_getwithref( this->mLogFileVnode );
        if( !error ){
            
            vnode_close( this->mLogFileVnode, FWRITE, this->mContext ? this->mContext : vfs_context_current() );
        }
    }
    
    this->mFlushingStopped = true;
    
}

//--------------------------------------------------------------------

void
DldInternalLog::fReleaseResources()
{
    //
    // stop the thread and close the file
    //
    this->fStopFlushing();
    
    if( this->mContext )
        vfs_context_rele( this->mContext );
    
    if( this->mBuffer.buffer )
        IOFree( this->mBuffer.buffer, this->mBufferSize );
    
    if( this->mLock )
        IOSimpleLockFree( this->mLock );
    
}

//--------------------------------------------------------------------

void
DldInternalLog::prepareForPowerOff(){
    
    assert( preemption_enabled() );
    
    this->mPowerIsBeingOff = true;
    
    //
    // wait until the thread executed vn_rdwr()
    //
    assert_wait((event_t)&this->mPowerIsBeingOff, THREAD_UNINT );
    thread_block( NULL );
    
    //
    // stop the thread and close the file thus flushing the cache
    //
    this->fStopFlushing();
}

//--------------------------------------------------------------------

void
DldInternalLog::DldInternalLogThread( void* __this )
{
    assert( __this );
    
    DldInternalLog*  _this = (DldInternalLog*)__this;
    
    while( true ){
        
        int errorLockForIO;
        
        //
        // increase the io_count if the vnode is not being drained
        //
        errorLockForIO = vnode_getwithref( _this->mLogFileVnode );
        
        //
        // vn_rdwr returns EIO on shutdown
        //
        if( !errorLockForIO &&
            false == _this->mPowerIsBeingOff &&
            _this->mBuffer.start != _this->mBuffer.end ){
            
            caddr_t data;
            int     len;
            
            if( ( _this->mFileDataSize - _this->mValidDataOffset ) < _this->mBufferSize ){
                
                int error;
                
                //
                // grow the file
                //
                error = _this->fIncreaseFileDataSize( _this->mBufferSize );
                assert( !error );
                if( error ){
                    
                    DBG_PRINT_ERROR_TO_ASL(("fIncreaseFileDataSize( %d ) failed\n", (int)_this->mBufferSize));
                }
                
            }// end if( ( _this->mFileDataSize 
            
            while( _this->mBuffer.start+_this->mBufferSize < _this->mBuffer.end ){
                
                //
                // overflow, the data is lost
                //
                _this->mBuffer.start += _this->mBufferSize;
            }
            
            _this->fLock();
            {// start of the lock
                
                assert( _this->mBuffer.start <= _this->mBuffer.end );
                len = (_this->mBuffer.end - _this->mBuffer.start)%_this->mBufferSize;
                
            }// end of the lock
            _this->fUnlock();
            
            data = _this->mBuffer.buffer + (_this->mBuffer.start%_this->mBufferSize);
            
            int error;
            
            //
            // vn_rdwr returns EIO on shutdown
            //
            if( (_this->mBuffer.start%_this->mBufferSize) <= ((_this->mBuffer.start + len)%_this->mBufferSize ) ){
                
                //
                // a case when the buffer has not wrapped around
                //
                error = vn_rdwr( UIO_WRITE,
                                 _this->mLogFileVnode,
                                 data,
                                 len,
                                 _this->mValidDataOffset,
                                 UIO_SYSSPACE,
                                 IO_NOAUTH | IO_SYNC,
                                 _this->mContext ? vfs_context_ucred( _this->mContext ) : kauth_cred_get(),
                                 NULL,
                                 _this->mContext ? vfs_context_proc( _this->mContext ) : current_proc() );
                
                if( error ){
                    
                    DBG_PRINT_ERROR_TO_ASL(("vn_rdwr() failed with the %x error\n", (int)error));
                }
                
            } else {
                
                int lenToEnd;
                int lenFromStart;
                
                //
                // the buffer has wrapped around
                //
                lenToEnd = _this->mBufferSize - (_this->mBuffer.start%_this->mBufferSize);
                //
                // lenToEnd is equal to len when the data ends in the last byte of the buffer so
                // effectively the end is wrapped around and set to 0x0
                //
                assert( lenToEnd <= len );
                lenFromStart = len - lenToEnd;
                
                error = vn_rdwr( UIO_WRITE,
                                 _this->mLogFileVnode,
                                 data,
                                 lenToEnd,
                                 _this->mValidDataOffset,
                                 UIO_SYSSPACE,
                                 IO_NOAUTH | IO_SYNC,
                                 _this->mContext ? vfs_context_ucred( _this->mContext ) : kauth_cred_get(),
                                 NULL,
                                 _this->mContext ? vfs_context_proc( _this->mContext ) : current_proc() );
                
                if( error ){
                    
                    DBG_PRINT_ERROR_TO_ASL(("vn_rdwr() failed with the %x error\n", (int)error));
                }
                
                //
                // the lenFromStart is 0x0 when the buffer end has wrapped around to 0x0
                //
                if( !error && (0x0 != lenFromStart) ){
                    
                    error = vn_rdwr( UIO_WRITE,
                                     _this->mLogFileVnode,
                                     _this->mBuffer.buffer,
                                     lenFromStart,
                                     _this->mValidDataOffset,
                                     UIO_SYSSPACE,
                                     IO_NOAUTH | IO_SYNC,
                                     _this->mContext ? vfs_context_ucred( _this->mContext ) : kauth_cred_get(),
                                     NULL,
                                     _this->mContext ? vfs_context_proc( _this->mContext ) : current_proc() );
                    if( error ){
                        
                        DBG_PRINT_ERROR_TO_ASL(("vn_rdwr() failed with the %x error\n", (int)error));
                    }
                }// end if( !error )
            }
            
            if( error ){
                
                DBG_PRINT_ERROR_TO_ASL(("vn_rdwr() failed with the 0x%x error\n", error));
                
            } else {
                
                _this->mValidDataOffset += len;
                assert( _this->mValidDataOffset <= _this->mFileDataSize );
            }
            
            //
            // move the buffer's start in any case
            //
            _this->mBuffer.start += len;
            assert( _this->mBuffer.start <= _this->mBuffer.end );
            
        }// end if( false == _this->mPowerIsBeingOff && _this->mBuffer.start != _this->mBuffer.end )
        
        
        if( _this->mPowerIsBeingOff ){
            
            //
            // so this guaranties that there is no vn_rdrw being called,
            // constantly try to wake up the thread waiting in power managent handler
            // as it must be idempotent in its behaviour
            //
            thread_wakeup( &_this->mPowerIsBeingOff );
            
            //
            // emulate the full read to retain the circular buffer in consistency
            //
            _this->fLock();
            {// start of the lock
                _this->mBuffer.start = _this->mBuffer.end;
            }// end of the lock
            _this->fUnlock();
        }
        
        //
        // decrease the io_count
        //
        if( !errorLockForIO )
            vnode_put( _this->mLogFileVnode );
        
        if( false == _this->mStopThread ){
            
            assert_wait_timeout((event_t)&_this->mBuffer.buffer,
                                THREAD_UNINT,
                                1000, /* 1 secs*/
                                1000*NSEC_PER_USEC);
            thread_block( NULL );
            
        } else {
            
            thread_wakeup( &_this->mInternalLogThread );
            break;
        }
        
    }// end while( true )
    
    //
    // call_continuation() calls thread_terminate() for us,
    // but it is considered as a good behaviour to do this here
    //
    thread_terminate( current_thread() );
}

//--------------------------------------------------------------------

errno_t
DldInternalLog::fIncreaseFileDataSize( __in uint64_t add )
{
    int             error = 0x0;
    vnode_attr      va = {0x0};
    VATTR_INIT( &va );
    VATTR_WANTED( &va, va_data_size );
    
    va.va_data_size = this->mFileDataSize + add;
    va.va_vaflags =(IO_NOZEROFILL) & 0xffff;
    
    vfs_context_t   vfs_context = this->mContext;
    if( ! this->mContext )
        vfs_context = vfs_context_create( NULL );
    
    assert( vfs_context );
    if( vfs_context ){
        
        int     error;
        error = vnode_setattr( this->mLogFileVnode, &va, vfs_context );
        assert( !error );
        if( ! error ){
            
            this->mFileDataSize += add;
            
        } else {
            
            DBG_PRINT_ERROR_TO_ASL(("vnode_setattr() failed\n"));
        }
        
        if( vfs_context != this->mContext )
            vfs_context_rele( vfs_context );
        
    } // end if( vfs_context )
    
    return error;
}

//--------------------------------------------------------------------

void
DldInternalLog::fLock()
{
    assert( this->mLock );
#if defined( DBG )
    assert( current_thread() != mThreadHoldingLock );
#endif
    
    IOSimpleLockLock( this->mLock );
    
#if defined( DBG )
    assert( NULL == mThreadHoldingLock );
    mThreadHoldingLock = current_thread();;
#endif
    
}

//--------------------------------------------------------------------

void
DldInternalLog::fUnlock()
{
#if defined( DBG )
    assert( current_thread() == mThreadHoldingLock );
    mThreadHoldingLock = NULL;
#endif
    
    IOSimpleLockUnlock( this->mLock );
}

//--------------------------------------------------------------------

void
DldInternalLog::fLogPutcLocked( __in char c )
{
    //
    // the functon must be called with the lock being held
    //
#if defined( DBG )
    assert( current_thread() == mThreadHoldingLock );
#endif
    
    this->mBuffer.buffer[ (int)((this->mBuffer.end++)%this->mBufferSize) ] = c;
    
}

//--------------------------------------------------------------------

#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define Ctod(c) ((c) - '0')

#define MAXBUF (sizeof(long long int) * 8)	/* enough for binary */

int
DldInternalLog::printnum(
         unsigned long long int	u,	/* number to print */
         int		base
        )
{
	char	buf[MAXBUF];	/* build number here */
	char *	p = &buf[MAXBUF-1];
	int nprinted = 0;
    
	do {
	    *p-- = mDigs[u % base];
	    u /= base;
	} while (u != 0);
    
	while (++p != &buf[MAXBUF]) {
	    this->fLogPutcLocked(*p);
	    nprinted++;
	}
    
	return nprinted;
}

//--------------------------------------------------------------------

int
DldInternalLog::doprnt(
         const char     *fmt,
         va_list        argp,
         int			radix)		/* default radix - for '%r' */
{
	int		length;
	int		prec;
	boolean_t	ladjust;
	char		padc;
	long long		n;
	unsigned long long	u;
	int		plus_sign;
	int		sign_char;
	boolean_t	altfmt, truncate;
	int		base;
	char	c;
	int		capitals;
	int		long_long;
	int             nprinted = 0;
    
	while ((c = *fmt) != '\0') {
	    if (c != '%') {
            this->fLogPutcLocked(c);
            nprinted++;
            fmt++;
            continue;
	    }
        
	    fmt++;
        
	    long_long = 0;
	    length = 0;
	    prec = -1;
	    ladjust = FALSE;
	    padc = ' ';
	    plus_sign = 0;
	    sign_char = 0;
	    altfmt = FALSE;
        
	    while (TRUE) {
            c = *fmt;
            if (c == '#') {
                altfmt = TRUE;
            }
            else if (c == '-') {
                ladjust = TRUE;
            }
            else if (c == '+') {
                plus_sign = '+';
            }
            else if (c == ' ') {
                if (plus_sign == 0)
                    plus_sign = ' ';
            }
            else
                break;
            fmt++;
	    }
        
	    if (c == '0') {
            padc = '0';
            c = *++fmt;
	    }
        
	    if (isdigit(c)) {
            while(isdigit(c)) {
                length = 10 * length + Ctod(c);
                c = *++fmt;
            }
	    }
	    else if (c == '*') {
            length = va_arg(argp, int);
            c = *++fmt;
            if (length < 0) {
                ladjust = !ladjust;
                length = -length;
            }
	    }
        
	    if (c == '.') {
            c = *++fmt;
            if (isdigit(c)) {
                prec = 0;
                while(isdigit(c)) {
                    prec = 10 * prec + Ctod(c);
                    c = *++fmt;
                }
            }
            else if (c == '*') {
                prec = va_arg(argp, int);
                c = *++fmt;
            }
	    }
        
	    if (c == 'l') {
            c = *++fmt;	/* need it if sizeof(int) < sizeof(long) */
            if (sizeof(int)<sizeof(long))
                long_long = 1;
            if (c == 'l') {
                long_long = 1;
                c = *++fmt;
            }	
	    } else if (c == 'q' || c == 'L') {
	    	long_long = 1;
            c = *++fmt;
	    } 
        
	    truncate = FALSE;
	    capitals=0;		/* Assume lower case printing */
        
	    switch(c) {
            case 'b':
            case 'B':
            {
                register char *p;
                boolean_t	  any;
                register int  i;
                
                if (long_long) {
                    u = va_arg(argp, unsigned long long);
                } else {
                    u = va_arg(argp, unsigned int);
                }
                p = va_arg(argp, char *);
                base = *p++;
                nprinted += printnum(u, base);
                
                if (u == 0)
                    break;
                
                any = FALSE;
                while ((i = *p++) != '\0') {
                    if (*fmt == 'B')
                        i = 33 - i;
                    if (*p <= 32) {
                        /*
                         * Bit field
                         */
                        register int j;
                        if (any)
                            this->fLogPutcLocked(',');
                        else {
                            this->fLogPutcLocked('<');
                            any = TRUE;
                        }
                        nprinted++;
                        j = *p++;
                        if (*fmt == 'B')
                            j = 32 - j;
                        for (; (c = *p) > 32; p++) {
                            this->fLogPutcLocked(c);
                            nprinted++;
                        }
                        nprinted += printnum((unsigned)( (u>>(j-1)) & ((2<<(i-j))-1)), base );
                    }
                    else if (u & (1<<(i-1))) {
                        if (any)
                            this->fLogPutcLocked(',');
                        else {
                            this->fLogPutcLocked('<');
                            any = TRUE;
                        }
                        nprinted++;
                        for (; (c = *p) > 32; p++) {
                            this->fLogPutcLocked(c);
                            nprinted++;
                        }
                    }
                    else {
                        for (; *p > 32; p++)
                            continue;
                    }
                }
                if (any) {
                    this->fLogPutcLocked('>');
                    nprinted++;
                }
                break;
            }
                
            case 'c':
                c = va_arg(argp, int);
                this->fLogPutcLocked(c);
                nprinted++;
                break;
                
            case 's':
            {
                register const char *p;
                register const char *p2;
                
                if (prec == -1)
                    prec = 0x7fffffff;	/* MAXINT */
                
                p = va_arg(argp, char *);
                
                if (p == NULL)
                    p = "";
                
                if (length > 0 && !ladjust) {
                    n = 0;
                    p2 = p;
                    
                    for (; *p != '\0' && n < prec; p++)
                        n++;
                    
                    p = p2;
                    
                    while (n < length) {
                        this->fLogPutcLocked(' ');
                        n++;
                        nprinted++;
                    }
                }
                
                n = 0;
                
                while ((n < prec) && (!(length > 0 && n >= length))) {
                    if (*p == '\0') {
                        break;
                    }
                    this->fLogPutcLocked(*p++);
                    nprinted++;
                    n++;
                }
                
                if (n < length && ladjust) {
                    while (n < length) {
                        this->fLogPutcLocked(' ');
                        n++;
                        nprinted++;
                    }
                }
                
                break;
            }
                
            case 'o':
                truncate = _doprnt_truncates;
            case 'O':
                base = 8;
                goto print_unsigned;
                
            case 'D': {
                unsigned char *up;
                char *q, *p;
                
                up = (unsigned char *)va_arg(argp, unsigned char *);
                p = (char *)va_arg(argp, char *);
                if (length == -1)
                    length = 16;
                while(length--) {
                    this->fLogPutcLocked( this->mDigs[(*up >> 4)] );
                    this->fLogPutcLocked( this->mDigs[(*up & 0x0f)] );
                    nprinted += 2;
                    up++;
                    if (length) {
                        for (q=p;*q;q++) {
                            this->fLogPutcLocked( *q );
                            nprinted++;
                        }
                    }
                }
                break;
            }
                
            case 'd':
                truncate = _doprnt_truncates;
                base = 10;
                goto print_signed;
                
            case 'u':
                truncate = _doprnt_truncates;
            case 'U':
                base = 10;
                goto print_unsigned;
                
            case 'p':
                altfmt = TRUE;
                if (sizeof(int)<sizeof(void *)) {
                    long_long = 1;
                }
            case 'x':
                truncate = _doprnt_truncates;
                base = 16;
                goto print_unsigned;
                
            case 'X':
                base = 16;
                capitals=16;	/* Print in upper case */
                goto print_unsigned;
                
            case 'z':
                truncate = _doprnt_truncates;
                base = 16;
                goto print_signed;
                
            case 'Z':
                base = 16;
                capitals=16;	/* Print in upper case */
                goto print_signed;
                
            case 'r':
                truncate = _doprnt_truncates;
            case 'R':
                base = radix;
                goto print_signed;
                
            case 'n':
                truncate = _doprnt_truncates;
            case 'N':
                base = radix;
                goto print_unsigned;
                
            print_signed:
                if (long_long) {
                    n = va_arg(argp, long long);
                } else {
                    n = va_arg(argp, int);
                }
                if (n >= 0) {
                    u = n;
                    sign_char = plus_sign;
                }
                else {
                    u = -n;
                    sign_char = '-';
                }
                goto print_num;
                
            print_unsigned:
                if (long_long) {
                    u = va_arg(argp, unsigned long long);
                } else { 
                    u = va_arg(argp, unsigned int);
                }
                goto print_num;
                
            print_num:
            {
                char	buf[MAXBUF];	/* build number here */
                register char *	p = &buf[MAXBUF-1];
                static char digits[] = "0123456789abcdef0123456789ABCDEF";
                const char *prefix = NULL;
                
                if (truncate) u = (long long)((int)(u));
                
                if (u != 0 && altfmt) {
                    if (base == 8)
                        prefix = "0";
                    else if (base == 16)
                        prefix = "0x";
                }
                
                do {
                    /* Print in the correct case */
                    *p-- = digits[(u % base)+capitals];
                    u /= base;
                } while (u != 0);
                
                length -= (int)(&buf[MAXBUF-1] - p);
                if (sign_char)
                    length--;
                if (prefix)
                    length -= (int)strlen(prefix);
                
                if (padc == ' ' && !ladjust) {
                    /* blank padding goes before prefix */
                    while (--length >= 0) {
                        this->fLogPutcLocked( ' ' );
                        nprinted++;
                    }			    
                }
                if (sign_char) {
                    this->fLogPutcLocked( sign_char );
                    nprinted++;
                }
                if (prefix) {
                    while (*prefix) {
                        this->fLogPutcLocked( *prefix++ );
                        nprinted++;
                    }
                }
                if (padc == '0') {
                    /* zero padding goes after sign and prefix */
                    while (--length >= 0) {
                        this->fLogPutcLocked( '0' );
                        nprinted++;
                    }			    
                }
                while (++p != &buf[MAXBUF]) {
                    this->fLogPutcLocked( *p );
                    nprinted++;
                }
                
                if (ladjust) {
                    while (--length >= 0) {
                        this->fLogPutcLocked( ' ' );
                        nprinted++;
                    }
                }
                break;
            }
                
            case '\0':
                fmt--;
                break;
                
            default:
                this->fLogPutcLocked( c );
                nprinted++;
	    }
        fmt++;
	}
    
	return nprinted;
}

//--------------------------------------------------------------------

void
DldInternalLog::Log(const char *fmt, ...)
{
	va_list	listp;
    
	if (fmt) {
        this->fLock();
        {// start of the lock
            va_start(listp, fmt);
            this->doprnt( fmt, listp, 16 );
            va_end(listp);
        }// end of the lock
        this->fUnlock();
	}
}

//--------------------------------------------------------------------

