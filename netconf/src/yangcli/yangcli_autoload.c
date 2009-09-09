/*  FILE: yangcli_autoload.c

   NETCONF YANG-based CLI Tool

   autoload support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
13-aug-09    abb      begun; started from yangcli.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh2.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include "libtecla.h"

#ifndef _H_procdefs
#include "procdefs.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_mgr
#include "mgr.h"
#endif

#ifndef _H_mgr_ses
#include "mgr_ses.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_op
#include "op.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_rpc_err
#include "rpc_err.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val_util
#include "val_util.h"
#endif

#ifndef _H_var
#include "var.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_xml_val
#include "xml_val.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifndef _H_yangcli_autoload
#include "yangcli_autoload.h"
#endif

#ifndef _H_yangcli_cmd
#include "yangcli_cmd.h"
#endif

#ifndef _H_yangcli_util
#include "yangcli_util.h"
#endif


/********************************************************************
* FUNCTION send_get_schema_to_server
* 
* Send an <get-schema> operation to the specified server
* in MGR_IO_ST_AUTOLOAD state
*
* format will be hard-wired to yang
*
* INPUTS:
*   server_cb == server control block to use
*   scb == session control block to use
*   module == module to get
*   revision == revision to get
*
* OUTPUTS:
*    server_cb->state may be changed or other action taken
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_get_schema_to_server (server_cb_t *server_cb,
                              ses_cb_t *scb,
                              const xmlChar *module,
                              const xmlChar *revision)
{
    ncx_module_t          *mod;
    obj_template_t        *rpc, *input, *parmobj;
    val_value_t           *reqdata, *parmval;
    mgr_rpc_req_t         *req;
    mgr_scb_t             *mscb;
    status_t               res;
    xmlns_id_t             nsid;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    input = NULL;
    nsid = 0;
    mscb = (mgr_scb_t *)scb->mgrcb;
    mod = ncx_find_module(NCXMOD_IETF_NETCONF_STATE, NULL);
    if (mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* get the <get-schema> input template */
    rpc = ncx_find_rpc(mod,  NCX_EL_GET_SCHEMA);
    if (rpc) {
        nsid = obj_get_nsid(rpc);
	input = obj_find_child(rpc, NULL, YANG_K_INPUT);
    }

    if (!input) {
	res = SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    } else {
	/* construct a method + parameter tree */
	reqdata = xml_val_new_struct(obj_get_name(rpc), 
				     nsid);
	if (!reqdata) {
	    log_error("\nError allocating a new RPC request");
	    res = ERR_INTERNAL_MEM;
	}
    }

    /* add /get-schema/input/identifier */
    if (res == NO_ERR) {
        parmval = xml_val_new_cstring(NCX_EL_IDENTIFIER,
                                      nsid,
                                      module);
        if (parmval == NULL) {
            res = ERR_INTERNAL_MEM;
        } else {
            val_add_child(parmval, reqdata);
        }
    }

    /* add /get-schema/input/version */
    if (res == NO_ERR) {
        parmval = xml_val_new_cstring(NCX_EL_VERSION,
                                      nsid,
                                      (revision) ? revision : EMPTY_STRING);
        if (parmval == NULL) {
            res = ERR_INTERNAL_MEM;
        } else {
            val_add_child(parmval, reqdata);
        }
    }

    /* add /get-schema/input/version */
    if (res == NO_ERR) {
        parmobj = obj_find_child(input,
                                 NCXMOD_IETF_NETCONF_STATE,
                                 NCX_EL_FORMAT);
        if (parmobj == NULL) {
            res = SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
        } else {
            parmval = val_make_simval_obj(parmobj,
                                          (const xmlChar *)"ns:yang",
                                          &res);
            if (parmval != NULL) {
                val_add_child(parmval, reqdata);
            }
        }
    }

    /* check any errors so far */
    if (res != NO_ERR) {
	val_free_value(reqdata);
	return res;
    }

    /* allocate an RPC request and send it */
    req = mgr_rpc_new_request(scb);
    if (!req) {
        res = ERR_INTERNAL_MEM;
        log_error("\nError allocating a new RPC request");
    } else {
        req->data = reqdata;
        req->rpc = rpc;
        req->timeout = server_cb->timeout;
    }
	
    if (res == NO_ERR) {
	if (LOGDEBUG2) {
	    log_debug2("\nabout to send RPC request with reqdata:");
	    val_dump_value_ex(reqdata, 
                              NCX_DEF_INDENT,
                              server_cb->display_mode);
	}

	/* the request will be stored if this returns NO_ERR */
	res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
    }

    if (res != NO_ERR) {
	if (req) {
	    mgr_rpc_free_request(req);
	} else if (reqdata) {
	    val_free_value(reqdata);
	}
    } else {
	server_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_get_schema_to_server */


/********************************************************************
* FUNCTION save_schema_file
* 
* Save the <data> node in the <rpc-reply> as the
* specified YANG file in the session work directory
*
* INPUTS:
*    server_cb == server control block to use
*    module == module name
*    revision == revision date
*    targetfile == filespec of the output file
*    resultval == result to output to file
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    save_schema_file (server_cb_t *server_cb,
                      const xmlChar *module,
                      const xmlChar *revision,
                      const xmlChar *targetfile,
                      val_value_t *resultval)
{
    status_t     res;
    struct stat  statbuf;
    int          statresult;

    res = NO_ERR;

    if (LOGDEBUG2) {
	log_debug2("\n*** output <get-schema> result "
                   "\n   module '%s'"
                   "\n   revision '%'s"
                   "\n   target '%s'",
                   module,
                   (revision) ? revision : EMPTY_STRING,
		   targetfile);
    }

    /* see if file already exists */
    statresult = stat((const char *)targetfile, &statbuf);
    if (statresult == 0) {
	log_error("\nError: temporary file '%s' already exists",
		  targetfile);
	return ERR_NCX_DATA_EXISTS;
    }
    
    /* output in text format to the specified file */
    res = log_alt_open((const char *)targetfile);
    if (res != NO_ERR) {
        log_error("\nError: temporary file '%s' could "
                  "not be opened (%s)",
                  targetfile,
                  get_error_string(res));
    } else {
        val_dump_alt_value(resultval, 0);
        log_alt_close();

        /* copy the target filename into the search result */
        server_cb->cursearchresult->source = xml_strdup(targetfile);
        if (server_cb->cursearchresult->source == NULL) {
            log_error("\nError: malloc failed for temporary file '%s'",
                      targetfile);
            return ERR_INTERNAL_MEM;
        }
    }

    return res;

}  /* save_schema_file */


/********************************************************************
* FUNCTION reset_feature
* 
* Go through the feature list and see if the specified
* feature should be enabled or not
*
* INPUTS:
*    mod == module containing this feature
*    feature == feature found
*    cookie == cookie passed in (feature_list)
*
* RETURNS:
*    TRUE if processing should continue, FALSE if done
*********************************************************************/
static boolean
    reset_feature (const ncx_module_t *mod,
		   ncx_feature_t *feature,
		   void *cookie)
{
    const ncx_list_t *feature_list;

    (void)mod;
    feature_list = (const ncx_list_t *)cookie;

    feature->enabled = 
	(ncx_string_in_list(feature->name, feature_list)) ?
	TRUE : FALSE;

    return TRUE;

}  /* reset_feature */


/********************************************************************
 * FUNCTION copy_module_to_tempdir
 * 
 * Copy the source file to a different location and possibly
 * change the filename
 *
 * INPUTS:
 *   mscb == manager session control block to use
 *   module == module name to copy
 *   revision == revision date to copy
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *   malloced and filled in temp file control block
 *   NULL if some error (see *res)
 *********************************************************************/
static ncxmod_temp_filcb_t *
    get_new_temp_filcb (mgr_scb_t *mscb,
                        const xmlChar *module,
                        const xmlChar *revision,
                        status_t *res)
{
    xmlChar             *filebuffer, *p;
    ncxmod_temp_filcb_t *temp_filcb;
    uint32               len_needed;

    *res = NO_ERR;

    /* figure out how big the filename will be */
    len_needed = xml_strlen(module);
    if (revision) {
        len_needed += (xml_strlen(revision) + 1);
    }
    len_needed += 5;   /* .yang */

    filebuffer = m__getMem(len_needed+1);
    if (filebuffer == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* construct a file name for the target file */
    p = filebuffer;
    p += xml_strcpy(p, module);
    if (revision) {
        *p++ = '.';
        p += xml_strcpy(p, revision);
    }
    p += xml_strcpy(p, (const xmlChar *)".yang");

    /* get a temp file control block
     * it will be stored in the session control block
     * so it is not a live malloced pointer
     */
    temp_filcb = ncxmod_new_session_tempfile(mscb->temp_sescb,
                                             filebuffer,
                                             res);
    m__free(filebuffer);
    return temp_filcb;

}   /* get_new_temp_filcb */


/********************************************************************
 * FUNCTION copy_module_to_tempdir
 * 
 * Copy the YANG source file to the session work directory
 *
 * INPUTS:
 *   mscb == manager session control block to use
 *   module == module name to copy
 *   revision == revision date to copy
 *   source == complete pathspec of source YANG file
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    copy_module_to_tempdir (mgr_scb_t *mscb,
                            const xmlChar *module,
                            const xmlChar *revision,
                            const xmlChar *source)
{
    xmlChar             *linebuffer;
    FILE                *srcfile, *destfile;
    ncxmod_temp_filcb_t *temp_filcb;
    boolean              done;
    status_t             res;

    res = NO_ERR;
    linebuffer = NULL;
    srcfile = NULL;
    destfile = NULL;

    temp_filcb = get_new_temp_filcb(mscb,
                                    module,
                                    revision,
                                    &res);
    if (temp_filcb == NULL) {
        return res;
    }

    /* get a buffer for transferring lines */
    linebuffer = m__getMem(NCX_MAX_LINELEN+1);;
    if (linebuffer == NULL) {
        ncxmod_free_session_tempfile(temp_filcb);
        return ERR_INTERNAL_MEM;
    }

    /* open the destination file for writing */
    destfile = fopen((const char *)temp_filcb->source, "w");
    if (destfile == NULL) {
        res = errno_to_status();
        ncxmod_free_session_tempfile(temp_filcb);
        return res;
    }

    /* open the YANG source file for reading */
    srcfile = fopen((const char *)source, "r");
    if (srcfile == NULL) {
        res = errno_to_status();
        fclose(destfile);
        ncxmod_free_session_tempfile(temp_filcb);
        return res;
    }

    done = FALSE;
    while (!done) {
        if (!fgets((char *)linebuffer, NCX_MAX_LINELEN, srcfile)) {
            /* read line failed, not an error */
            done = TRUE;
            continue;
        }

        if (fputs((const char *)linebuffer, destfile) == EOF) {
            log_error("\nError: copy to temp file failed");
            /*** keeping partial file around!!! ***/
            done = TRUE;
        }
    }

    fclose(srcfile);
    fclose(destfile);
    m__free(linebuffer);

    return NO_ERR;

}   /* copy_module_to_tempdir */


/********************************************************************
* FUNCTION autoload_module
* 
* auto-load the specified module
*
* INPUTS:
*   modname == module name
*   revision == module revision
*   devlist  == deviation list
*   retmod == address of return module
*
* OUTPUTS:
*   *retmod == loaded module (if NO_ERR)
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    autoload_module (const xmlChar *modname,
                     const xmlChar *revision,
                     ncx_list_t *devlist,
                     ncx_module_t **retmod)
{
    dlq_hdr_t               savedevQ;
    ncx_lmem_t             *listmember;
    status_t                res;

    if (LOGDEBUG2) {
        log_debug2("\nStarting autoload for module '%s', "
                   "revision '%s'",
                   modname,
                   (revision) ? revision : EMPTY_STRING);
    }

    res = NO_ERR;
    dlq_createSQue(&savedevQ);

    /* first load any deviations */
    for (listmember = ncx_first_lmem(devlist);
         listmember != NULL && res == NO_ERR;
         listmember = (ncx_lmem_t *)dlq_nextEntry(listmember)) {

        res = ncxmod_load_deviation(listmember->val.str,
                                    &savedevQ);
        if (res != NO_ERR) {
            log_error("\nError: Deviation module %s not loaded (%s)!!",
                      listmember->val.str, 
                      get_error_string(res));
        }
    }

    /* parse the requested module now 
     * retmod is a live pointer
     */
    if (res == NO_ERR) {
        res = ncxmod_parse_module(modname, 
                                 revision, 
                                 &savedevQ,
                                 retmod);
        if (res != NO_ERR) {
            log_error("\nError: Auto-load for module '%s' failed (%s)",
                      modname, 
                      get_error_string(res));
        }
    }

    ncx_clean_save_deviationsQ(&savedevQ);

    return res;

}  /* autoload_module */


/********************************************************************
* FUNCTION autoload_setup_tempdir
* 
* copy all the advertised YANG modules into the
* session temp files directory
*
* DOES NOT COPY FILES UNLESS THE 'source' FIELD IS SET
* IN THE SEARCH RESULT (ONLY LOCAL FILES COPIED)
*
* INPUTS:
*   server_cb == server session control block to use
*   scb == current session in progress
*
* OUTPUTS:
*   $HOME/.yangtools/tmp/<progdir>/<sesdir>/ filled with
*   the the specified YANG files that are already available
*   on this system.
*   
*   These search records will be removed from the 
*   server_cb->searchresultQ and modptr records 
*   added to the server_cb->modptrQ
*
* RETURNS:
*    status
*********************************************************************/
status_t
    autoload_setup_tempdir (server_cb_t *server_cb,
                            ses_cb_t *scb)
{
    mgr_scb_t               *mscb;
    ncxmod_search_result_t  *searchresult;
    status_t                 res, retres;

#ifdef DEBUG
    if (!server_cb || !scb) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    res = NO_ERR;
    retres = NO_ERR;
    mscb = (mgr_scb_t *)scb->mgrcb;

    /* try to copy as many files as possible, even if some errors */
    for (searchresult = (ncxmod_search_result_t *)
             dlq_firstEntry(&server_cb->searchresultQ);
         searchresult != NULL;
         searchresult = (ncxmod_search_result_t *)
             dlq_nextEntry(searchresult)) {

        /* skip bad entries and not-found entries */
        if (searchresult->module == NULL ||
            searchresult->source == NULL ||
            searchresult->res != NO_ERR) {
            continue;
        }

        /* check if the URI was not the same, even though
         * the module was found, so the source was set
         */
        if (!searchresult->capmatch) {
            m__free(searchresult->source);
            searchresult->source = NULL;
            continue;
        }

        /* copy this local module to the work dir so
         * it will be found in an import, even if the
         * revision-date is missing from the import
         */
        res = copy_module_to_tempdir(mscb,
                                     searchresult->module,
                                     searchresult->revision,
                                     searchresult->source);
        if (res != NO_ERR) {
            searchresult->res = res;
            retres = res;
        }
    }

    return retres;

}  /* autoload_setup_tempdir */


/********************************************************************
* FUNCTION autoload_start_get_modules
* 
* Start the MGR_SES_IO_CONN_AUTOLOAD state
*
* Go through all the search result records and 
* make sure all files are present.  Try to use the
* <get-schema> operation to fill in any missing modules
*
* INPUTS:
*   server_cb == server session control block to use
*   scb == session control block to use
*
* OUTPUTS:
*   $HOME/.yangtools/tmp/<progdir>/<sesdir>/ filled with
*   the the specified YANG files that are ertrieved from
*   the device with <get-schema>
*   
* RETURNS:
*    status
*********************************************************************/
status_t
    autoload_start_get_modules (server_cb_t *server_cb,
                                ses_cb_t *scb)
{
    ncxmod_search_result_t  *searchresult;
    status_t                 res;
    boolean                  done;

#ifdef DEBUG
    if (!server_cb || !scb) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    done = FALSE;
    res = NO_ERR;

    /* find first file that needs to be retrieved with get-schema */
    for (searchresult = (ncxmod_search_result_t *)
             dlq_firstEntry(&server_cb->searchresultQ);
         searchresult != NULL && !done;
         searchresult = (ncxmod_search_result_t *)
             dlq_nextEntry(searchresult)) {

        /* skip bad entries and found entries */
        if (searchresult->module == NULL ||
            searchresult->source != NULL ||
            searchresult->res != NO_ERR) {
            continue;
        }

        /* found an entry that needs to be retrieved */
        server_cb->command_mode = CMD_MODE_AUTOLOAD;
        server_cb->cursearchresult = searchresult;
        done = TRUE;

        res = send_get_schema_to_server(server_cb,
                                       scb,
                                       searchresult->module,
                                       searchresult->revision);
    }

    return res;

}  /* autoload_start_get_modules */


/********************************************************************
* FUNCTION autoload_handle_rpc_reply
* 
* Handle the current <get-schema> response
*
* INPUTS:
*   server_cb == server session control block to use
*   scb == session control block to use
*   reply == data node from the <rpc-reply> PDU
*   anyerrors == TRUE if <rpc-error> detected instead
*                of <data>
*             == FALSE if no <rpc-error> elements detected
*
* OUTPUTS:
*   $HOME/.yangtools/tmp/<progdir>/<sesdir>/ filled with
*   the the specified YANG files that was retrieved from
*   the device with <get-schema>
*
*   Next request is started, or autoload process is completed
*   and the command_mode is changed back to CMD_MODE_NORMAL
*
* RETURNS:
*    status
*********************************************************************/
status_t
    autoload_handle_rpc_reply (server_cb_t *server_cb,
                               ses_cb_t *scb,
                               val_value_t *reply,
                               boolean anyerrors)
{
    mgr_scb_t               *mscb;
    ncxmod_search_result_t  *searchresult;
    ncxmod_temp_filcb_t     *temp_filcb;
    const xmlChar           *module, *revision;
    val_value_t             *dataval;
    status_t                 res;
    boolean                  done;

#ifdef DEBUG
    if (!server_cb || !scb || !reply) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    done = FALSE;
    res = NO_ERR;
    mscb = (mgr_scb_t *)scb->mgrcb;
    searchresult = server_cb->cursearchresult;
    module = searchresult->module;
    revision = searchresult->revision;

    if (anyerrors) {
        /* going to skip this module any try to
         * compile without it
         */
        log_error("\nError: <get-schema> for module '%s', "
                  "revision '%s' failed",
                  module,
                  (revision) ? revision : EMPTY_STRING);
        if (!LOGDEBUG2) {
            /* the error was never printed */
            if (LOGINFO) {
                val_dump_value(reply, NCX_DEF_INDENT);
            }
        }
    } else {
        /* get the data node out of the reply;
         * it contains the requested YANG module
         * in raw text form
         */
        dataval = val_find_child(reply, NULL, NCX_EL_DATA);
        if (dataval == NULL) {
            res = SET_ERROR(ERR_NCX_DATA_MISSING);
        } else {
            /* get a file handle in the temp session
             * directory
             */
            temp_filcb = get_new_temp_filcb(mscb,
                                            module,
                                            revision,
                                            &res);
            if (temp_filcb != NULL) {
                /* copy the value node to the work directory
                 * as a YANG file
                 */
                res = save_schema_file(server_cb,
                                       module,
                                       revision,
                                       temp_filcb->source,
                                       dataval);
            }
        }

        if (res != NO_ERR) {
            log_error("\nError: save <get-schema> content "
                      " for module '%s' revision '%s' failed (%s)",
                      module,
                      (revision) ? revision : EMPTY_STRING,
                      get_error_string(res));
            server_cb->cursearchresult->res = res;
        }
    }

    /* find next file that needs to be retrieved with get-schema */
    for (searchresult = (ncxmod_search_result_t *)
             dlq_nextEntry(server_cb->cursearchresult);
         searchresult != NULL && !done;
         searchresult = (ncxmod_search_result_t *)
             dlq_nextEntry(searchresult)) {

        /* skip bad entries and found entries */
        if (searchresult->module == NULL ||
            searchresult->source != NULL ||
            searchresult->res != NO_ERR) {
            continue;
        }

        /* found an entry that needs to be retrieved */
        server_cb->command_mode = CMD_MODE_AUTOLOAD;
        server_cb->cursearchresult = searchresult;
        done = TRUE;

        res = send_get_schema_to_server(server_cb,
                                       scb,
                                       searchresult->module,
                                       searchresult->revision);
    }

    if (!done) {
        /* no search results left to get */
        return autoload_compile_modules(server_cb, scb);
    }

    return res;

}  /* autoload_handle_rpc_reply */


/********************************************************************
* FUNCTION autoload_compile_modules
* 
* Go through all the search result records and parse
* the modules that the device advertised.
* DOES NOT LOAD THESE MODULES INTO THE MODULE DIRECTORY
* THE RETURNED ncx_module_t STRUCT IS JUST FOR ONE SESSION USE
*
* Apply the deviations and features specified in
* the search result cap back-ptr, to the module 
*
* INPUTS:
*   server_cb == server session control block to use
*   scb == session control block to use
*
* OUTPUTS:
*   $HOME/.yangtools/tmp/<progdir>/<sesdir>/ filled with
*   the the specified YANG files that are already available
*   on this system.
*   
*   These search records will be removed from the 
*   server_cb->searchresultQ and modptr records 
*   added to the server_cb->modptrQ
*
* RETURNS:
*    status
*********************************************************************/
status_t
    autoload_compile_modules (server_cb_t *server_cb,
                              ses_cb_t *scb)
{
    mgr_scb_t              *mscb;
    ncxmod_search_result_t *searchresult;
    modptr_t               *modptr;
    ncx_module_t           *mod;
    status_t                res;

#ifdef DEBUG
    if (!server_cb || !scb) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    /* should not happen, but it is possible that the
     * server did not send any YANG module capabilities
     */
    if (dlq_empty(&server_cb->searchresultQ)) {
        return NO_ERR;
    }

    res = NO_ERR;
    mscb = (mgr_scb_t *)scb->mgrcb;

    /* set the alternate path to point at the
     * session work directory; this will cause
     * the server revision date of each module to be
     * used instead of a random version that the
     * yangcli registry contains
     */
    ncxmod_set_altpath(mscb->temp_sescb->source);

    /* set the altername module Q so the imports
     * do not get re-compiled over and over
     */
    ncx_set_cur_modQ(&mscb->temp_modQ);

    /* go through all the search results and load
     * the modules in order; all imports should already
     * be preloaded into the session work directory
     */
    while (!dlq_empty(&server_cb->searchresultQ)) {

        searchresult = (ncxmod_search_result_t *)
            dlq_deque(&server_cb->searchresultQ);

        if (searchresult->res != NO_ERR ||
            searchresult->source == NULL) {
            ncxmod_free_search_result(searchresult);
            continue;
        }

        mod = NULL;
        res = autoload_module(searchresult->module,
                              searchresult->revision,
                              &searchresult->cap->cap_deviation_list,
                              &mod);

        if (res == NO_ERR && mod != NULL) {
            /* mod can be NULL if the module was already in the temp_modQ */
            modptr = new_modptr(mod, 
                                &searchresult->cap->cap_feature_list,
                                &searchresult->cap->cap_deviation_list);
            if (modptr == NULL) {
                log_error("\nMalloc failure");
            } else {
                dlq_enque(modptr, &server_cb->modptrQ);
            }
        }

        ncxmod_free_search_result(searchresult);
    }

    /* undo the temporary MODPATH setting */
    ncxmod_clear_altpath();

    /* undo the temporary module Q */
    ncx_reset_modQ();

    /* set the session module Q so the val.c
     * functions will find the server modules
     * instead of the pre-loaded versions
     */
    ncx_set_session_modQ(&mscb->temp_modQ);

    /* need to wait until all the modules are loaded to
     * go through the modptr list and enable/disable the features
     * to match what the server has reported
     */
    for (modptr = (modptr_t *)
	     dlq_firstEntry(&server_cb->modptrQ);
	 modptr != NULL;
	 modptr = (modptr_t *)dlq_nextEntry(modptr)) {

	if (modptr->feature_list) {
	    ncx_for_all_features(modptr->mod,
				 reset_feature,
				 modptr->feature_list,
				 FALSE);
	}
    }

    server_cb->command_mode = CMD_MODE_NORMAL;
    server_cb->cursearchresult = NULL;

    return res;

}  /* autoload_compile_modules */


/* END yangcli_autoload.c */
