/*
 * Copyright (c) 2009, Andy Bierman
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: netconf-subsystem.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
14-jan-07    abb      begun;
03-mar-11    abb      get rid of usleeps and replace with
                      design that checks for EAGAIN

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <stdarg.h>

#define _C_main 1

#include "procdefs.h"

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_agt
#include "agt.h"
#endif

#ifndef _H_agt_ncxserver
#include "agt_ncxserver.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_send_buff
#include "send_buff.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
/* #define SUBSYS_TRACE 1 */
#endif

#define BUFFLEN  2000

#define MAX_READ_TRIES 1000

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

static char *client_addr;
static struct sockaddr_un ncxname;
static int    ncxsock;
static char *user;
static char *port;

#ifdef SUBSYS_TRACE
static FILE *infile;
static FILE *outfile;
static FILE *errfile;
static int   indirty;
static int   outdirty;
static int   errdirty;
static int   errok;
#endif

static boolean ncxconnect;

static char msgbuff[BUFFLEN];


/********************************************************************
* FUNCTION init_subsys
*
* Initialize the subsystem, and get it ready to send and receive
* the first message of any kind
* 
* RETURNS:
*   status
*********************************************************************/
static status_t
    init_subsys (void)
{
    char *cp, *con;
    int   ret;

#ifdef SUBSYS_TRACE
    infile = NULL;
    outfile = NULL;
    errfile = NULL;

    indirty = 0;
    outdirty = 0;
    errdirty = 0;

    errok = 1;   /* set to 1 to write non-errors to errfile */

    /* open the logfiles that should be active */

    /* infile = fopen("/tmp/subsys-in.log", "w"); */
    /* outfile = fopen("/tmp/subsys-out.log", "w"); */
    errfile = fopen("/tmp/subsys-err.log", "w");
#endif

    client_addr = NULL;
    port = NULL;
    user = NULL;
    ncxsock = -1;
    ncxconnect = FALSE;

    /* get the client address */
    con = getenv("SSH_CONNECTION");
    if (!con) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nGet SSH_CONNECTION variable failed\n");
        }
#endif
        return ERR_INTERNAL_VAL;
    }

    /* get the client addr */
    client_addr = strdup(con);
    if (!client_addr) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nStrdup failed\n");
        }
#endif
        return ERR_INTERNAL_MEM;
    }
    cp = strchr(client_addr, ' ');
    if (!cp) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nMalformed SSH_CONNECTION variable\n");
        }
#endif
        return ERR_INTERNAL_VAL;
    } else {
        *cp = 0;
    }

    /* get the server connect port */
    cp = strrchr(con, ' ');
    if (cp && cp[1]) {
        port = strdup(++cp);
    }
    if (!port) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nMalformed SSH_CONNECTION variable\n");
        }
#endif
        return ERR_INTERNAL_VAL;
    }
        
    /* get the username */
    cp = getenv("USER");
    if (!cp) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nGet USER variable failed\n");
        }
#endif
        return ERR_INTERNAL_VAL;
    }
    user = strdup(cp);
    if (!user) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nStrdup failed\n");
        }
#endif
        return ERR_INTERNAL_MEM;
    }

    /* make a socket to connect to the NCX server */
    ncxsock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (ncxsock < 0) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nNCX Socket failed\n");
        }
#endif
        return ERR_NCX_CONNECT_FAILED;
    } 
    ncxname.sun_family = AF_LOCAL;
    strncpy(ncxname.sun_path, 
            NCXSERVER_SOCKNAME, 
            sizeof(ncxname.sun_path));

    /* try to connect to the NCX server */
    ret = connect(ncxsock,
                  (const struct sockaddr *)&ncxname,
                  SUN_LEN(&ncxname));
    if (ret != 0) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nNCX Socket connect failed\n");
        }
#endif
        return ERR_NCX_OPERATION_FAILED;
    } else {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, 
                    "\nNCX Socket connect OK on FD %d\n",
                    ncxsock);
        }
#endif
        ncxconnect = TRUE;
    }

#ifdef USE_NONBLOCKING_IO
    /* set non-blocking IO */
    if (fcntl(ncxsock, F_SETFD, O_NONBLOCK)) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, "\nfnctl failed");
        }
#endif
    }
#endif

#ifdef SUBSYS_TRACE
    if (infile) {
        fflush(infile);
    }
    if (outfile) {
        fflush(outfile);
    }
    if (errfile) {
        fflush(errfile);
    }
#endif

    /* connected to the ncxserver and setup the ENV vars ok */
    return NO_ERR;

} /* init_subsys */


/********************************************************************
* FUNCTION cleanup_subsys
*
* Cleanup the subsystem
* 
*********************************************************************/
static void
    cleanup_subsys (void)
{
    if (client_addr) {
        m__free(client_addr);
    }
    if (user) {
        m__free(user);
    }
    if (ncxconnect) {
        close(ncxsock);
    }

#ifdef SUBSYS_TRACE
    if (infile) {
        fclose(infile);
    }
    if (outfile) {
        fclose(outfile);
    }
    if (errfile) {
        fclose(errfile);
    }
#endif

} /* cleanup_subsys */


/********************************************************************
* FUNCTION send_ncxconnect
*
* Send the <ncx-connect> message to the ncxserver
* 
* RETURNS:
*   status
*********************************************************************/
static status_t
    send_ncxconnect (void)
{
    status_t  res;
    char connectmsg[] = 
    "%s\n<ncx-connect xmlns=\"%s\" version=\"1\" user=\"%s\" "
    "address=\"%s\" magic=\"%s\" transport=\"ssh\" port=\"%s\" />\n%s";

    memset(msgbuff, 0x0, BUFFLEN);
    sprintf(msgbuff, 
            connectmsg,
            (const char *)XML_START_MSG, 
            NCX_URN, 
            user, 
            client_addr, 
            NCX_SERVER_MAGIC, 
            port, 
            NC_SSH_END);

    res = send_buff(ncxsock, msgbuff, strlen(msgbuff));
    return res;

} /* send_ncxconnect */


/********************************************************************
* FUNCTION do_read
*
* Read from a FD
* 
* INPUTS:
*              
* RETURNS:
*   return byte count
*********************************************************************/
static ssize_t
    do_read (int readfd, 
             char *readbuff, 
             size_t readcnt, 
             status_t *retres)
{
    boolean   readdone;
    ssize_t   retcnt;

    readdone = FALSE;
    retcnt = 0;
    *retres = NO_ERR;

    while (!readdone && *retres == NO_ERR) {
        retcnt = read(readfd, readbuff, readcnt);
        if (retcnt < 0) {
            if (errno != EAGAIN) {
#ifdef SUBSYS_TRACE
                if (errfile) {
                    fprintf(errfile, 
                            "\nread failed on FD %d (%d)",
                            readfd,
                            (int)retcnt);
                    errdirty = 1;
                }
#endif
                *retres = ERR_NCX_READ_FAILED;
                continue;
            }
        } else if (retcnt == 0) {
#ifdef SUBSYS_TRACE
            if (errfile) {
                fprintf(errfile, "\nclosed connection");
                errdirty = 1;
            }
#endif
            *retres = ERR_NCX_EOF;
            readdone = TRUE;
            continue;
        } else {
            /* retcnt is the number of bytes read */
            readdone = TRUE;
        }
    }  /*end readdone loop */

    return retcnt;

}  /* do_read */


/********************************************************************
* FUNCTION io_loop
*
* Handle the IO for the program
* 
* INPUTS:
*              
* RETURNS:
*   status
*********************************************************************/
static status_t
    io_loop (void)
{
    status_t  res;
    boolean   done;
    fd_set    fds;
    int       ret;
    ssize_t   retcnt;

    res = NO_ERR;
    done = FALSE;
    FD_ZERO(&fds);

    while (!done) {

#ifdef SUBSYS_TRACE
        if (infile && indirty) {
            fflush(infile);
            indirty = 0;
        }
        if (outfile && outdirty) {
            fflush(outfile);
            outdirty = 0;
        }
        if (errfile && errdirty) {
            fflush(errfile);
            errdirty = 0;
        }
#endif

        FD_SET(STDIN_FILENO, &fds);
        FD_SET(ncxsock, &fds);

        ret = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if ( errno != EINTR ) {
#ifdef SUBSYS_TRACE
                if (errfile) {
                    fprintf(errfile, "\nnetconf select failed (%d)", ret);
                    errdirty = 1;
                }
#endif
                res = ERR_NCX_OPERATION_FAILED;
                done = TRUE;
            }
            continue;
        } else if (ret == 0) {
#ifdef SUBSYS_TRACE
            if (errfile) {
                fprintf(errfile, "\nnetconf select zero exit");
                errdirty = 1;
            }
#endif
            res = NO_ERR;
            done = TRUE;
            continue;
        } /* else some IO to process */

        /* check any input from client */
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            /* get buff from openssh */
            retcnt = do_read(STDIN_FILENO, 
                             msgbuff, 
                             (size_t)BUFFLEN,
                             &res);

            if (res == ERR_NCX_EOF) {
                res = NO_ERR;
                done = TRUE;
                continue;
            } else if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            } else if (res == NO_ERR && retcnt > 0) {
#ifdef SUBSYS_TRACE
                if (errfile && errok) {
                    /* not an error */
                    fprintf(errfile,
                            "\nSTDIN read (%d)", 
                            (int)retcnt);
                    errdirty = 1;
                }
                if (infile) {
                    int cnt;
                    for (cnt = 0; cnt<retcnt; cnt++) {
                        fprintf(infile, "%c", msgbuff[cnt]);
                    }
                    indirty = 1;
                }
#endif

                /* send this buffer to the ncxserver */
                res = send_buff(ncxsock, msgbuff, (size_t)retcnt);
                if (res != NO_ERR) {
#ifdef SUBSYS_TRACE
                    if (errfile) {
                        fprintf(errfile, 
                                "\nnetconf send buff (%d) to "
                                "ncxserver failed",
                                (int)retcnt);
                        errdirty = 1;
                    }
#endif
                    done = TRUE;
                    continue;
                }

#ifdef SUBSYS_TRACE
                if (errfile && errok) {
                    /* not an error */
                    fprintf(errfile, 
                            "\nnetconf write ncxserver (%d)", 
                            (int)retcnt);
                    errdirty = 1;
                }
#endif
            }
        }  /* if STDIN set */

        /* check any input from the ncxserver */
        if (FD_ISSET(ncxsock, &fds)) {
            res = NO_ERR;
            retcnt = do_read(ncxsock, 
                             msgbuff, 
                             (size_t)BUFFLEN,
                             &res);

            if (res == ERR_NCX_EOF) {
                res = NO_ERR;
                done = TRUE;
                continue;
            } else if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            } else if (res == NO_ERR && retcnt > 0) {
#ifdef SUBSYS_TRACE
                if (errfile && errok) {
                    /* not an error */
                    fprintf(errfile,
                            "\nNncxserver read  (%d)", 
                            (int)retcnt);
                    errdirty = 1;
                }
                if (outfile) {
                    int cnt;
                    for (cnt = 0; cnt<retcnt; cnt++) {
                        fprintf(outfile, "%c", msgbuff[cnt]);
                    }
                    outdirty = 1;
                }
#endif
                /* send this buffer to STDOUT */
                res = send_buff(STDOUT_FILENO, msgbuff, (size_t)retcnt);
                if (res != NO_ERR) {
#ifdef SUBSYS_TRACE
                    if (errfile) {
                        fprintf(errfile, 
                                "\nnetconf send buff to client failed");
                        errdirty = 1;
                    }
#endif
                    done = TRUE;
                    continue;
                }
#ifdef SUBSYS_TRACE
                if (errfile && errok) {
                    /* not an error */
                    fprintf(errfile,
                            "\nnetconf write client (%d)",
                            (int)retcnt);
                    errdirty = 1;
                }
#endif
            }
        }
    }

    return res;

} /* io_loop */


/********************************************************************
* FUNCTION main
*
* STDIN is input from the SSH client (sent to ncxserver)
* STDOUT is output to the SSH client (rcvd from ncxserver)
* 
* RETURNS:
*   0 if NO_ERR
*   1 if error connecting or logging into ncxserver
*********************************************************************/
int
/*     main (int argc, char **argv) */
    main (void)
{
    status_t  res;
    const char *msg;

    res = init_subsys();
    if (res != NO_ERR) {
        msg = "init failed";
    }

    if (res == NO_ERR) {
        res = send_ncxconnect();
        if (res != NO_ERR) {
            msg = "connect failed";
        }
    }

    if (res == NO_ERR) {
        res = io_loop();
        if (res != NO_ERR) {
            msg = "IO error";
        }
    }

    if (res != NO_ERR) {
#ifdef SUBSYS_TRACE
        if (errfile) {
            fprintf(errfile, 
                    "\nnetconf: %s (%s)", 
                    msg, 
                    get_error_string(res));
        }
#endif
    }

    cleanup_subsys();

    if (res != NO_ERR) {
        return 1;
    } else {
        return 0;
    }

} /* main */

