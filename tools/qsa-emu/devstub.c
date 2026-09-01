/* devstub.c - minimal QNX resmgr backing absent peripheral device nodes so that
 * DIOManagerApp/Cinemo/i2c-auth open()s succeed in the QEMU harness (no real HW).
 * Serves every path in argv[1..]; answers open/read/write/devctl with success.
 *   /dev/otg-cinemo  (iap2.device iap://ffs:///dev/otg-cinemo, opened by Cinemo/libNmeSDK)
 *   /dev/i2c8        (iap2.auth i2c:///dev/i2c8, MFi auth coprocessor)
 * read() returns EOF, write() consumes all, devctl() returns EOK with zeroed data.
 * Daemonizes (procmgr_daemon) so the IFS boot .script can continue past it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/procmgr.h>

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t      io_funcs;      /* EOF-read nodes (otg-cinemo, i2c8) */
static resmgr_io_funcs_t      rand_io_funcs; /* random-byte nodes (urandom, random) */

/* read() -> up to 2048 pseudo-random bytes (feeds libairplay RandomBytes/DeviceID).
 * Without this, RandomNumberUtils.c:236 loops forever on open("/dev/urandom"). */
static int rand_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    static uint8_t buf[2048];
    size_t n; size_t i;
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) return ENOSYS;
    n = msg->i.nbytes; if (n > sizeof buf) n = sizeof buf;
    for (i = 0; i < n; i++) buf[i] = (uint8_t)(rand() & 0xff);
    _IO_SET_READ_NBYTES(ctp, n);
    SETIOV(&ctp->iov[0], buf, n);
    return _RESMGR_NPARTS(1);
}

/* answer any devctl with success (no reply data) */
static int my_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_devctl_default(ctp, msg, ocb);
    if (status != _RESMGR_DEFAULT)
        return status;              /* std devctl handled */
    return EOK;                     /* unknown command: pretend success */
}

/* read() -> EOF (0 bytes) */
static int my_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;
    _IO_SET_READ_NBYTES(ctp, 0);
    return _RESMGR_NPARTS(0);
}

/* write() -> consume everything */
static int my_write(resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK) return status;
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return ENOSYS;
    _IO_SET_WRITE_NBYTES(ctp, msg->i.nbytes);
    return _RESMGR_NPARTS(0);
}

int main(int argc, char **argv) {
    dispatch_t *dpp;
    resmgr_attr_t rattr;
    resmgr_context_t *ctp;
    int i;

    if (argc < 2) { fprintf(stderr, "usage: devstub <path> [path...]\n"); return 1; }

    dpp = dispatch_create();
    if (!dpp) { perror("dispatch_create"); return 1; }

    memset(&rattr, 0, sizeof rattr);
    rattr.nparts_max   = 1;
    rattr.msg_max_size = 4096;

    srand((unsigned)(time(NULL) ^ getpid()));

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.devctl = my_devctl;
    io_funcs.read   = my_read;
    io_funcs.write  = my_write;
    rand_io_funcs = io_funcs;          /* same, but random-returning read */
    rand_io_funcs.read = rand_read;

    for (i = 1; i < argc; i++) {
        resmgr_io_funcs_t *iof = &io_funcs;
        if (strcmp(argv[i], "/dev/urandom") == 0 || strcmp(argv[i], "/dev/random") == 0)
            iof = &rand_io_funcs;
        iofunc_attr_t *a = malloc(sizeof *a);
        iofunc_attr_init(a, S_IFCHR | 0666, 0, 0);
        if (resmgr_attach(dpp, &rattr, argv[i], _FTYPE_ANY, 0,
                          &connect_funcs, iof, a) == -1) {
            fprintf(stderr, "devstub: attach %s failed: %s\n", argv[i], strerror(errno));
            return 1;
        }
        printf("devstub: serving %s\n", argv[i]);
    }

    /* background self so the boot script can proceed */
    procmgr_daemon(EXIT_SUCCESS, PROCMGR_DAEMON_NODEVNULL | PROCMGR_DAEMON_NOCHDIR);

    {
        dispatch_context_t *dctp = dispatch_context_alloc(dpp);
        while (1) {
            dctp = dispatch_block(dctp);
            if (!dctp) { if (errno == EFAULT) continue; break; }
            dispatch_handler(dctp);
        }
    }
    (void)ctp;
    return 0;
}
