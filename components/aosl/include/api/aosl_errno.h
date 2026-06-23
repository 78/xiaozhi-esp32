/***************************************************************************
 * Module		:		AOSL errno definition header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_ERRNO_H__
#define __AOSL_ERRNO_H__

#include <api/aosl_defs.h>

#define AOSL_EBASE        1000

/* Error code definition */
#define AOSL_EPERM        (AOSL_EBASE + 1)  /* Operation not permitted */
#define AOSL_ENOENT       (AOSL_EBASE + 2)  /* No such file or directory */
#define AOSL_ESRCH        (AOSL_EBASE + 3)  /* No such process */
#define AOSL_EINTR        (AOSL_EBASE + 4)  /* Interrupted system call */
#define AOSL_EIO          (AOSL_EBASE + 5)  /* I/O error */
#define AOSL_ENXIO        (AOSL_EBASE + 6)  /* No such device or address */
#define AOSL_E2BIG        (AOSL_EBASE + 7)  /* Argument list too long */
#define AOSL_ENOEXEC      (AOSL_EBASE + 8)  /* Exec format error */
#define AOSL_EBADF        (AOSL_EBASE + 9)  /* Bad file number */
#define AOSL_ECHILD       (AOSL_EBASE + 10)  /* No child processes */
#define AOSL_EAGAIN       (AOSL_EBASE + 11)  /* Try again */
#define AOSL_ENOMEM       (AOSL_EBASE + 12)  /* Out of memory */
#define AOSL_EACCES       (AOSL_EBASE + 13)  /* Permission denied */
#define AOSL_EFAULT       (AOSL_EBASE + 14)  /* Bad address */
#define AOSL_ENOTBLK      (AOSL_EBASE + 15)  /* Block device required */
#define AOSL_EBUSY        (AOSL_EBASE + 16)  /* Device or resource busy */
#define AOSL_EEXIST       (AOSL_EBASE + 17)  /* File exists */
#define AOSL_EXDEV        (AOSL_EBASE + 18)  /* Cross-device link */
#define AOSL_ENODEV       (AOSL_EBASE + 19)  /* No such device */
#define AOSL_ENOTDIR      (AOSL_EBASE + 20)  /* Not a directory */
#define AOSL_EISDIR       (AOSL_EBASE + 21)  /* Is a directory */
#define AOSL_EINVAL      (AOSL_EBASE + 22)  /* Invalid argument */
#define AOSL_ENFILE      (AOSL_EBASE + 23)  /* File table overflow */
#define AOSL_EMFILE      (AOSL_EBASE + 24)  /* Too many open files */
#define AOSL_ENOTTY      (AOSL_EBASE + 25)  /* Not a typewriter */
#define AOSL_ETXTBSY     (AOSL_EBASE + 26)  /* Text file busy */
#define AOSL_EFBIG       (AOSL_EBASE + 27)  /* File too large */
#define AOSL_ENOSPC      (AOSL_EBASE + 28)  /* No space left on device */
#define AOSL_ESPIPE      (AOSL_EBASE + 29)  /* Illegal seek */
#define AOSL_EROFS       (AOSL_EBASE + 30)  /* Read-only file system */
#define AOSL_EMLINK      (AOSL_EBASE + 31)  /* Too many links */
#define AOSL_EPIPE       (AOSL_EBASE + 32)  /* Broken pipe */
#define AOSL_EDOM        (AOSL_EBASE + 33)  /* Math argument out of domain of func */
#define AOSL_ERANGE      (AOSL_EBASE + 34)  /* Math result not representable */

/* More error code ... */
#define AOSL_EDEADLK      (AOSL_EBASE + 35)  /* Resource deadlock would occur */
#define AOSL_ENAMETOOLONG (AOSL_EBASE + 36) /* File name too long */
#define AOSL_ENOLCK      (AOSL_EBASE + 37)  /* No record locks available */
#define AOSL_ENOSYS      (AOSL_EBASE + 38)  /* Function not implemented */
#define AOSL_ENOTEMPTY   (AOSL_EBASE + 39)  /* Directory not empty */
#define AOSL_ELOOP       (AOSL_EBASE + 40)  /* Too many symbolic links encountered */

/* Network related errors */
#define AOSL_EWOULDBLOCK (AOSL_EBASE + AOSL_EAGAIN)  /* Operation would block */
#define AOSL_ENOMSG      (AOSL_EBASE + 42)  /* No message of desired type */
#define AOSL_EIDRM       (AOSL_EBASE + 43)  /* Identifier removed */
#define AOSL_ECHRNG      (AOSL_EBASE + 44)  /* Channel number out of range */
#define AOSL_EL2NSYNC    (AOSL_EBASE + 45)  /* Level 2 not synchronized */
#define AOSL_EL3HLT      (AOSL_EBASE + 46)  /* Level 3 halted */
#define AOSL_EL3RST      (AOSL_EBASE + 47)  /* Level 3 reset */
#define AOSL_ELNRNG      (AOSL_EBASE + 48)  /* Link number out of range */
#define AOSL_EUNATCH     (AOSL_EBASE + 49)  /* Protocol driver not attached */

/* Socket errors */
#define AOSL_ENONET      (AOSL_EBASE + 64)  /* Machine is not on the network */
#define AOSL_ENOPKG      (AOSL_EBASE + 65)  /* Package not installed */
#define AOSL_EREMOTE     (AOSL_EBASE + 66)  /* Object is remote */
#define AOSL_ENOLINK     (AOSL_EBASE + 67)  /* Link has been severed */
#define AOSL_EADV        (AOSL_EBASE + 68)  /* Advertise error */
#define AOSL_ESRMNT      (AOSL_EBASE + 69)  /* Srmount error */
#define AOSL_ECOMM       (AOSL_EBASE + 70)  /* Communication error on send */
#define AOSL_EPROTO      (AOSL_EBASE + 71)  /* Protocol error */
#define AOSL_EMULTIHOP   (AOSL_EBASE + 72)  /* Multihop attempted */
#define AOSL_EDOTDOT     (AOSL_EBASE + 73)  /* RFS specific error */
#define AOSL_EBADMSG     (AOSL_EBASE + 74)  /* Not a data message */
#define AOSL_EOVERFLOW   (AOSL_EBASE + 75)  /* Value too large for defined data type */
#define AOSL_ENOTUNIQ    (AOSL_EBASE + 76)  /* Name not unique on network */
#define AOSL_EBADFD      (AOSL_EBASE + 77)  /* File descriptor in bad state */
#define AOSL_EREMCHG     (AOSL_EBASE + 78)  /* Remote address changed */
#define AOSL_ELIBACC     (AOSL_EBASE + 79)  /* Can not access a needed shared library */
#define AOSL_ELIBBAD     (AOSL_EBASE + 80)  /* Accessing a corrupted shared library */
#define AOSL_ELIBSCN     (AOSL_EBASE + 81)  /* .lib section in a.out corrupted */
#define AOSL_ELIBMAX     (AOSL_EBASE + 82)  /* Attempting to link in too many shared libraries */
#define AOSL_ELIBEXEC    (AOSL_EBASE + 83)  /* Cannot exec a shared library directly */
#define AOSL_EILSEQ      (AOSL_EBASE + 84)  /* Illegal byte sequence */
#define AOSL_ERESTART    (AOSL_EBASE + 85)  /* Interrupted system call should be restarted */
#define AOSL_ESTRPIPE    (AOSL_EBASE + 86)  /* Streams pipe error */
#define AOSL_EUSERS      (AOSL_EBASE + 87)  /* Too many users */
#define AOSL_ENOTSOCK    (AOSL_EBASE + 88)  /* Socket operation on non-socket */
#define AOSL_EDESTADDRREQ (AOSL_EBASE + 89) /* Destination address required */
#define AOSL_EMSGSIZE    (AOSL_EBASE + 90)  /* Message too long */
#define AOSL_EPROTOTYPE  (AOSL_EBASE + 91)  /* Protocol wrong type for socket */
#define AOSL_ENOPROTOOPT (AOSL_EBASE + 92)  /* Protocol not available */
#define AOSL_EPROTONOSUPPORT (AOSL_EBASE + 93) /* Protocol not supported */
#define AOSL_ESOCKTNOSUPPORT (AOSL_EBASE + 94) /* Socket type not supported */
#define AOSL_EOPNOTSUPP  (AOSL_EBASE + 95)  /* Operation not supported on transport endpoint */
#define AOSL_EPFNOSUPPORT (AOSL_EBASE + 96) /* Protocol family not supported */
#define AOSL_EAFNOSUPPORT (AOSL_EBASE + 97) /* Address family not supported by protocol */
#define AOSL_EADDRINUSE  (AOSL_EBASE + 98)  /* Address already in use */
#define AOSL_EADDRNOTAVAIL (AOSL_EBASE + 99) /* Cannot assign requested address */
#define AOSL_ENETDOWN    (AOSL_EBASE + 100) /* Network is down */
#define AOSL_ENETUNREACH (AOSL_EBASE + 101) /* Network is unreachable */
#define AOSL_ENETRESET   (AOSL_EBASE + 102) /* Network dropped connection because of reset */
#define AOSL_ECONNABORTED (AOSL_EBASE + 103) /* Software caused connection abort */
#define AOSL_ECONNRESET  (AOSL_EBASE + 104) /* Connection reset by peer */
#define AOSL_ENOBUFS     (AOSL_EBASE + 105) /* No buffer space available */
#define AOSL_EISCONN     (AOSL_EBASE + 106) /* Transport endpoint is already connected */
#define AOSL_ENOTCONN    (AOSL_EBASE + 107) /* Transport endpoint is not connected */
#define AOSL_ESHUTDOWN   (AOSL_EBASE + 108) /* Cannot send after transport endpoint shutdown */
#define AOSL_ETOOMANYREFS (AOSL_EBASE + 109) /* Too many references: cannot splice */
#define AOSL_ETIMEDOUT   (AOSL_EBASE + 110) /* Connection timed out */
#define AOSL_ECONNREFUSED (AOSL_EBASE + 111) /* Connection refused */
#define AOSL_EHOSTDOWN   (AOSL_EBASE + 112) /* Host is down */
#define AOSL_EHOSTUNREACH (AOSL_EBASE + 113) /* No route to host */
#define AOSL_EALREADY    (AOSL_EBASE + 114) /* Operation already in progress */
#define AOSL_EINPROGRESS (AOSL_EBASE + 115) /* Operation now in progress */
#define AOSL_ESTALE      (AOSL_EBASE + 116) /* Stale file handle */
#define AOSL_EUCLEAN     (AOSL_EBASE + 117) /* Structure needs cleaning */
#define AOSL_ENOTNAM     (AOSL_EBASE + 118) /* Not a XENIX named type file */
#define AOSL_ENAVAIL     (AOSL_EBASE + 119) /* No XENIX semaphores available */
#define AOSL_EISNAM      (AOSL_EBASE + 120) /* Is a named type file */
#define AOSL_EREMOTEIO   (AOSL_EBASE + 121) /* Remote I/O error */

/* HAL common error */
#define AOSL_EHAL        (AOSL_EBASE + 1000) /* HAL common error */

/**
 * @brief Get the pointer to the thread-local AOSL errno variable.
 * @return  pointer to the current thread's errno value
 **/
extern __aosl_api__ int *aosl_errno_ptr (void);

/**
 * @brief Get the human-readable error description string for an AOSL error code.
 * @param [in] errnum  the AOSL error number
 * @return             pointer to a static string describing the error
 **/
extern __aosl_api__ char *aosl_strerror (int errnum);

/** @brief The thread-local AOSL errno variable. */
#define aosl_errno (*aosl_errno_ptr ())

#endif /* __AOSL_ERRNO_H__ */