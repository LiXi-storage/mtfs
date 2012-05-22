/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static const char *
err2str(int error)
{
	static char errnum[8];

	switch (error) {
#ifdef EPERM
	case EPERM:
		return ("EPERM");
#endif
#ifdef ENOENT
	case ENOENT:
		return ("ENOENT");
#endif
#ifdef ESRCH
	case ESRCH:
		return ("ESRCH");
#endif
#ifdef EINTR
	case EINTR:
		return ("EINTR");
#endif
#ifdef EIO
	case EIO:
		return ("EIO");
#endif
#ifdef ENXIO
	case ENXIO:
		return ("ENXIO");
#endif
#ifdef E2BIG
	case E2BIG:
		return ("E2BIG");
#endif
#ifdef ENOEXEC
	case ENOEXEC:
		return ("ENOEXEC");
#endif
#ifdef EBADF
	case EBADF:
		return ("EBADF");
#endif
#ifdef ECHILD
	case ECHILD:
		return ("ECHILD");
#endif
#ifdef EDEADLK
	case EDEADLK:
		return ("EDEADLK");
#endif
#ifdef ENOMEM
	case ENOMEM:
		return ("ENOMEM");
#endif
#ifdef EACCES
	case EACCES:
		return ("EACCES");
#endif
#ifdef EFAULT
	case EFAULT:
		return ("EFAULT");
#endif
#ifdef ENOTBLK
	case ENOTBLK:
		return ("ENOTBLK");
#endif
#ifdef EBUSY
	case EBUSY:
		return ("EBUSY");
#endif
#ifdef EEXIST
	case EEXIST:
		return ("EEXIST");
#endif
#ifdef EXDEV
	case EXDEV:
		return ("EXDEV");
#endif
#ifdef ENODEV
	case ENODEV:
		return ("ENODEV");
#endif
#ifdef ENOTDIR
	case ENOTDIR:
		return ("ENOTDIR");
#endif
#ifdef EISDIR
	case EISDIR:
		return ("EISDIR");
#endif
#ifdef EINVAL
	case EINVAL:
		return ("EINVAL");
#endif
#ifdef ENFILE
	case ENFILE:
		return ("ENFILE");
#endif
#ifdef EMFILE
	case EMFILE:
		return ("EMFILE");
#endif
#ifdef ENOTTY
	case ENOTTY:
		return ("ENOTTY");
#endif
#ifdef ETXTBSY
	case ETXTBSY:
		return ("ETXTBSY");
#endif
#ifdef EFBIG
	case EFBIG:
		return ("EFBIG");
#endif
#ifdef ENOSPC
	case ENOSPC:
		return ("ENOSPC");
#endif
#ifdef ESPIPE
	case ESPIPE:
		return ("ESPIPE");
#endif
#ifdef EROFS
	case EROFS:
		return ("EROFS");
#endif
#ifdef EMLINK
	case EMLINK:
		return ("EMLINK");
#endif
#ifdef EPIPE
	case EPIPE:
		return ("EPIPE");
#endif
#ifdef EDOM
	case EDOM:
		return ("EDOM");
#endif
#ifdef ERANGE
	case ERANGE:
		return ("ERANGE");
#endif
#ifdef EAGAIN
	case EAGAIN:
		return ("EAGAIN");
#endif
#ifdef EINPROGRESS
	case EINPROGRESS:
		return ("EINPROGRESS");
#endif
#ifdef EALREADY
	case EALREADY:
		return ("EALREADY");
#endif
#ifdef ENOTSOCK
	case ENOTSOCK:
		return ("ENOTSOCK");
#endif
#ifdef EDESTADDRREQ
	case EDESTADDRREQ:
		return ("EDESTADDRREQ");
#endif
#ifdef EMSGSIZE
	case EMSGSIZE:
		return ("EMSGSIZE");
#endif
#ifdef EPROTOTYPE
	case EPROTOTYPE:
		return ("EPROTOTYPE");
#endif
#ifdef ENOPROTOOPT
	case ENOPROTOOPT:
		return ("ENOPROTOOPT");
#endif
#ifdef EPROTONOSUPPORT
	case EPROTONOSUPPORT:
		return ("EPROTONOSUPPORT");
#endif
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT:
		return ("ESOCKTNOSUPPORT");
#endif
#ifdef EOPNOTSUPP
	case EOPNOTSUPP:
		return ("EOPNOTSUPP");
#endif
#ifdef EPFNOSUPPORT
	case EPFNOSUPPORT:
		return ("EPFNOSUPPORT");
#endif
#ifdef EAFNOSUPPORT
	case EAFNOSUPPORT:
		return ("EAFNOSUPPORT");
#endif
#ifdef EADDRINUSE
	case EADDRINUSE:
		return ("EADDRINUSE");
#endif
#ifdef EADDRNOTAVAIL
	case EADDRNOTAVAIL:
		return ("EADDRNOTAVAIL");
#endif
#ifdef ENETDOWN
	case ENETDOWN:
		return ("ENETDOWN");
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:
		return ("ENETUNREACH");
#endif
#ifdef ENETRESET
	case ENETRESET:
		return ("ENETRESET");
#endif
#ifdef ECONNABORTED
	case ECONNABORTED:
		return ("ECONNABORTED");
#endif
#ifdef ECONNRESET
	case ECONNRESET:
		return ("ECONNRESET");
#endif
#ifdef ENOBUFS
	case ENOBUFS:
		return ("ENOBUFS");
#endif
#ifdef EISCONN
	case EISCONN:
		return ("EISCONN");
#endif
#ifdef ENOTCONN
	case ENOTCONN:
		return ("ENOTCONN");
#endif
#ifdef ESHUTDOWN
	case ESHUTDOWN:
		return ("ESHUTDOWN");
#endif
#ifdef ETOOMANYREFS
	case ETOOMANYREFS:
		return ("ETOOMANYREFS");
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:
		return ("ETIMEDOUT");
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:
		return ("ECONNREFUSED");
#endif
#ifdef ELOOP
	case ELOOP:
		return ("ELOOP");
#endif
#ifdef ENAMETOOLONG
	case ENAMETOOLONG:
		return ("ENAMETOOLONG");
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN:
		return ("EHOSTDOWN");
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:
		return ("EHOSTUNREACH");
#endif
#ifdef ENOTEMPTY
	case ENOTEMPTY:
		return ("ENOTEMPTY");
#endif
#ifdef EPROCLIM
	case EPROCLIM:
		return ("EPROCLIM");
#endif
#ifdef EUSERS
	case EUSERS:
		return ("EUSERS");
#endif
#ifdef EDQUOT
	case EDQUOT:
		return ("EDQUOT");
#endif
#ifdef ESTALE
	case ESTALE:
		return ("ESTALE");
#endif
#ifdef EREMOTE
	case EREMOTE:
		return ("EREMOTE");
#endif
#ifdef EBADRPC
	case EBADRPC:
		return ("EBADRPC");
#endif
#ifdef ERPCMISMATCH
	case ERPCMISMATCH:
		return ("ERPCMISMATCH");
#endif
#ifdef EPROGUNAVAIL
	case EPROGUNAVAIL:
		return ("EPROGUNAVAIL");
#endif
#ifdef EPROGMISMATCH
	case EPROGMISMATCH:
		return ("EPROGMISMATCH");
#endif
#ifdef EPROCUNAVAIL
	case EPROCUNAVAIL:
		return ("EPROCUNAVAIL");
#endif
#ifdef ENOLCK
	case ENOLCK:
		return ("ENOLCK");
#endif
#ifdef ENOSYS
	case ENOSYS:
		return ("ENOSYS");
#endif
#ifdef EFTYPE
	case EFTYPE:
		return ("EFTYPE");
#endif
#ifdef EAUTH
	case EAUTH:
		return ("EAUTH");
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH:
		return ("ENEEDAUTH");
#endif
#ifdef EIDRM
	case EIDRM:
		return ("EIDRM");
#endif
#ifdef ENOMSG
	case ENOMSG:
		return ("ENOMSG");
#endif
#ifdef EOVERFLOW
	case EOVERFLOW:
		return ("EOVERFLOW");
#endif
#ifdef ECANCELED
	case ECANCELED:
		return ("ECANCELED");
#endif
#ifdef EILSEQ
	case EILSEQ:
		return ("EILSEQ");
#endif
#ifdef ENOATTR
	case ENOATTR:
		return ("ENOATTR");
#endif
#ifdef EDOOFUS
	case EDOOFUS:
		return ("EDOOFUS");
#endif
#ifdef EBADMSG
	case EBADMSG:
		return ("EBADMSG");
#endif
#ifdef EMULTIHOP
	case EMULTIHOP:
		return ("EMULTIHOP");
#endif
#ifdef ENOLINK
	case ENOLINK:
		return ("ENOLINK");
#endif
#ifdef EPROTO
	case EPROTO:
		return ("EPROTO");
#endif
	default:
		snprintf(errnum, sizeof(errnum), "%d", error);
		return (errnum);
	}
}

int main(int argc, char *argv[])
{
	long ret;
	if (argc != 2) {
		return -1;
	}
	
	ret = strtol(argv[1], NULL, 10);
	
	if (ret > 128 && ret < 256) {
		ret -= 256;
	}

	if (ret < 0) {	
		printf("-%s\n", err2str((int)(-ret)));
	} else {
		printf("%s\n", err2str((int)(ret)));
	}
	return 0;
}
