/*  FILE: agt_sys.c

   NETCONF System Data Model implementation: Agent Side Support

identifiers:
container /system
leaf /system/sysName
leaf /system/sysCurrentDateTime
leaf /system/sysBootDateTime
notification /sysStartup
leaf /sysStartup/startupSource
list /sysStartup/bootError
leaf /sysStartup/bootError/error-type
leaf /sysStartup/bootError/error-tag
leaf /sysStartup/bootError/error-severity
leaf /sysStartup/bootError/error-app-tag
leaf /sysStartup/bootError/error-path
leaf /sysStartup/bootError/error-message
leaf /sysStartup/bootError/error-info
notification /sysShutdown
leaf /sysShutdown/timeLeft
leaf /sysShutdown/mode
notification /sysConfigChange
leaf /sysConfigChange/userName
leaf /sysConfigChange/sessionId
list /sysConfigChange/editList
leaf /sysConfigChange/editList/target
leaf /sysConfigChange/editList/operation
notification /sysSessionStart
leaf /sysSessionStart/userName
leaf /sysSessionStart/sessionId
leaf /sysSessionStart/remoteHost
notification /sysSessionEnd
leaf /sysSessionEnd/userName
leaf /sysSessionEnd/sessionId
leaf /sysSessionEnd/remoteHost
leaf /sysSessionEnd/terminationReason


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24feb09      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <memory.h>
#include  <unistd.h>
#include  <errno.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_agt
#include  "agt.h"
#endif

#ifndef _H_agt_cap
#include  "agt_cap.h"
#endif

#ifndef _H_agt_cb
#include  "agt_cb.h"
#endif

#ifndef _H_agt_not
#include  "agt_not.h"
#endif

#ifndef _H_agt_rpc
#include  "agt_rpc.h"
#endif

#ifndef _H_agt_ses
#include  "agt_ses.h"
#endif

#ifndef _H_agt_sys
#include  "agt_sys.h"
#endif

#ifndef _H_agt_util
#include  "agt_util.h"
#endif

#ifndef _H_cfg
#include  "cfg.h"
#endif

#ifndef _H_getcb
#include  "getcb.h"
#endif

#ifndef _H_log
#include  "log.h"
#endif

#ifndef _H_ncxmod
#include  "ncxmod.h"
#endif

#ifndef _H_ncxtypes
#include  "ncxtypes.h"
#endif

#ifndef _H_rpc
#include  "rpc.h"
#endif

#ifndef _H_rpc_err
#include  "rpc_err.h"
#endif

#ifndef _H_ses
#include  "ses.h"
#endif

#ifndef _H_ses_msg
#include  "ses_msg.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tstamp
#include  "tstamp.h"
#endif

#ifndef _H_val
#include  "val.h"
#endif

#ifndef _H_val_util
#include  "val_util.h"
#endif

#ifndef _H_xmlns
#include  "xmlns.h"
#endif

#ifndef _H_xml_util
#include  "xml_util.h"
#endif

#ifndef _H_xml_wr
#include  "xml_wr.h"
#endif

#ifndef _H_yangconst
#include  "yangconst.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define AGT_SYS_DEBUG 1
#endif


#define system_N_system (const xmlChar *)"system"
#define system_N_sysName (const xmlChar *)"sysName"
#define system_N_sysCurrentDateTime (const xmlChar *)"sysCurrentDateTime"
#define system_N_sysBootDateTime (const xmlChar *)"sysBootDateTime"

#define system_N_sysStartup (const xmlChar *)"sysStartup"
#define system_N_sysShutdown (const xmlChar *)"sysShutdown"
#define system_N_sysConfigChange (const xmlChar *)"sysConfigChange"
#define system_N_sysSessionStart (const xmlChar *)"sysSessionStart"
#define system_N_sysSessionEnd (const xmlChar *)"sysSessionEnd"

#define system_N_userName (const xmlChar *)"userName"
#define system_N_sessionId (const xmlChar *)"sessionId"
#define system_N_remoteHost (const xmlChar *)"remoteHost"
#define system_N_killedBy (const xmlChar *)"killedBy"
#define system_N_terminationReason (const xmlChar *)\
    "terminationReason"

#define system_N_target (const xmlChar *)"target"
#define system_N_operation (const xmlChar *)"operation"

/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S			    *
*                                                                   *
*********************************************************************/

static boolean              agt_sys_init_done = FALSE;

/* system.yang */
static ncx_module_t         *sysmod;

/* cached pointer to the <system> element template */
static const obj_template_t *systemobj;

/* cached pointers to the eventType nodes for this module */
static const obj_template_t *sysStartupobj;
static const obj_template_t *sysShutdownobj;
static const obj_template_t *sysConfigChangeobj;
static const obj_template_t *sysSessionStartobj;
static const obj_template_t *sysSessionEndobj;



/********************************************************************
* FUNCTION get_currentDateTime
*
* <get> operation handler for the sysCurrentDateTime leaf
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_currentDateTime (ses_cb_t *scb,
			 getcb_mode_t cbmode,
			 val_value_t *virval,
			 val_value_t  *dstval)
{
    xmlChar      *buff;

    (void)scb;
    (void)virval;

    if (cbmode == GETCB_GET_VALUE) {
	buff = (xmlChar *)m__getMem(TSTAMP_MIN_SIZE);
	if (!buff) {
	    return ERR_INTERNAL_MEM;
	}
	tstamp_datetime(buff);
	VAL_STR(dstval) = buff;
	return NO_ERR;
    } else {
	return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

} /* get_currentDateTime */


/********************************************************************
* FUNCTION send_sysStartup
*
* Queue the <sysStartup> notification
*
*********************************************************************/
static void
    send_sysStartup (void)
{
    agt_not_msg_t         *not;
    cfg_template_t        *cfg;
    val_value_t           *leafval, *bootErrorval;
    rpc_err_rec_t         *rpcerror;
    const obj_template_t  *bootErrorobj;
    status_t               res;

    if (LOGDEBUG) {
	log_debug("\nagt_sys: generating <sysStartup> "
		  "notification");
    }

    not = agt_not_new_notification(sysStartupobj);
    if (!not) {
	log_error("\nError: malloc failed; cannot "
		  "send <sysStartup>");
	return;
    }

    /* add sysStartup/startupSource */
    cfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    if (!cfg) {
	SET_ERROR(ERR_INTERNAL_VAL);
    } else if (cfg->src_url) {
	leafval = agt_make_leaf(sysStartupobj,
				(const xmlChar *)"startupSource",
				cfg->src_url,
				&res);
	if (leafval) {
	    agt_not_add_to_payload(not, leafval);
	} else {
	    log_error("\nError: cannot make payload leaf (%s)",
		      get_error_string(res));
	}
    }

    /* add sysStartup/bootError for each error recorded */
    if (!dlq_empty(&cfg->load_errQ)) {
	/* get the bootError object */
	bootErrorobj = obj_find_child(sysStartupobj,
				      AGT_SYS_MODULE,
				      (const xmlChar *)"bootError");
	if (!bootErrorobj) {
	    SET_ERROR(ERR_INTERNAL_VAL);
	} else {
	    for (rpcerror = (rpc_err_rec_t *)
		     dlq_firstEntry(&cfg->load_errQ);
		 rpcerror != NULL;
		 rpcerror = (rpc_err_rec_t *)
		     dlq_nextEntry(rpcerror)) {
		/* make the bootError value struct */
		bootErrorval = val_new_value();
		if (!bootErrorval) {
		    log_error("\nError: malloc failed; cannot "
			      "make <bootError> struct");
		} else {
		    res = agt_rpc_fill_rpc_error(rpcerror,
						 bootErrorval);
		    if (res != NO_ERR) {
			log_error("\nError: problems making "
				  "<bootError> (%s)",
				  get_error_string(res));
		    }
		    /* add even if there are some missing leafs */
		    agt_not_add_to_payload(not, bootErrorval);
		}
	    }
	}
    }

    agt_not_queue_notification(not);

} /* send_sysStartup */


/********************************************************************
* FUNCTION add_common_session_parms
*
* Add the leafs from the SysCommonSessionParms grouping
*
* INPUTS:
*   scb == session control block to use for payload values
*   not == notification msg to use to add parms into
*
* OUTPUTS:
*   'not' payloadQ has malloced entries added to it
*********************************************************************/
static void
    add_common_session_parms (const ses_cb_t *scb,
			      agt_not_msg_t *not)
{
    val_value_t     *leafval;
    xmlChar          numbuff[NCX_MAX_NUMLEN];
    status_t         res;

    /* add userName */
    if (scb->username) {
	leafval = agt_make_leaf(not->notobj,
				system_N_userName,
				scb->username,
				&res);
	if (leafval) {
	    agt_not_add_to_payload(not, leafval);
	} else {
	    log_error("\nError: cannot make payload leaf (%s)",
		      get_error_string(res));
	}
    }

    /* add sessionId */
    sprintf((char *)numbuff, "%u", scb->sid);
    leafval = agt_make_leaf(not->notobj,
			    system_N_sessionId,
			    numbuff,
			    &res);
    if (leafval) {
	agt_not_add_to_payload(not, leafval);
    } else {
	log_error("\nError: cannot make payload leaf (%s)",
		  get_error_string(res));
    }

    /* add remoteHost */
    if (scb->peeraddr) {
	leafval = agt_make_leaf(not->notobj,
				system_N_remoteHost,
				scb->peeraddr,
				&res);
	if (leafval) {
	    agt_not_add_to_payload(not, leafval);
	} else {
	    log_error("\nError: cannot make payload leaf (%s)",
		      get_error_string(res));
	}
    }

} /* add_common_session_parms */


/********************************************************************
* FUNCTION init_static_vars
*
* Init the static vars
*
*********************************************************************/
static void
    init_static_sys_vars (void)
{
    sysmod = NULL;
    systemobj = NULL;
    sysStartupobj = NULL;
    sysShutdownobj = NULL;
    sysConfigChangeobj = NULL;
    sysSessionStartobj = NULL;
    sysSessionEndobj = NULL;

} /* init_static_sys_vars */


/************* E X T E R N A L    F U N C T I O N S ***************/


/********************************************************************
* FUNCTION agt_sys_init
*
* INIT 1:
*   Initialize the agent notification module data structures
*
* INPUTS:
*   none
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_sys_init (void)
{
    status_t   res;

    if (agt_sys_init_done) {
	return SET_ERROR(ERR_INTERNAL_INIT_SEQ);
    }

#ifdef AGT_SYS_DEBUG
    if (LOGDEBUG2) {
	log_debug2("\nagt_sys: Loading notifications module");
    }
#endif

    init_static_sys_vars();
    agt_sys_init_done = TRUE;

    /* load the system module */
    res = ncxmod_load_module(AGT_SYS_MODULE, NULL, &sysmod);
    if (res != NO_ERR) {
	return res;
    }

    /* find the object definition for the system element */
    systemobj = ncx_find_object(sysmod,
				system_N_system);

    if (!systemobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    sysStartupobj = 
	ncx_find_object(sysmod,
			system_N_sysStartup);
    if (!sysStartupobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    sysShutdownobj = 
	ncx_find_object(sysmod,
			system_N_sysShutdown);
    if (!sysShutdownobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    sysConfigChangeobj = 
	ncx_find_object(sysmod,
			system_N_sysConfigChange);
    if (!sysConfigChangeobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    sysSessionStartobj = 
	ncx_find_object(sysmod,
			system_N_sysSessionStart);
    if (!sysSessionStartobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    sysSessionEndobj = 
	ncx_find_object(sysmod,
			system_N_sysSessionEnd);
    if (!sysSessionEndobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    return NO_ERR;

}  /* agt_sys_init */


/********************************************************************
* FUNCTION agt_sys_init2
*
* INIT 2:
*   Initialize the monitoring data structures
*   This must be done after the <running> config is loaded
*
* INPUTS:
*   none
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_sys_init2 (void)
{
    val_value_t           *topval, *childval;
    cfg_template_t        *runningcfg;
    const xmlChar         *myhostname;
    status_t               res;
    xmlChar                tstampbuff[TSTAMP_MIN_SIZE];

    if (!agt_sys_init_done) {
	return SET_ERROR(ERR_INTERNAL_INIT_SEQ);
    }

    /* get the running config to add some static data into */
    runningcfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    if (!runningcfg || !runningcfg->root) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* add /system */
    topval = val_new_value();
    if (!topval) {
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(topval, systemobj);

    /* handing off the malloced memory here */
    val_add_child(topval, runningcfg->root);

    /* add /system/sysName */
    myhostname = (const xmlChar *)getenv("HOSTNAME");
    if (!myhostname) {
	myhostname = (const xmlChar *)"localhost";
    }
    childval = agt_make_leaf(systemobj,
			     system_N_sysName,
			     myhostname,
			     &res);
    if (childval) {
	val_add_child(childval, topval);
    }

    /* add /system/sysCurrentDateTime */
    childval = agt_make_virtual_leaf(systemobj,
				     system_N_sysCurrentDateTime,
				     get_currentDateTime,
				     &res);
    if (childval) {
	val_add_child(childval, topval);
    }

    /* add /system/sysBootDateTime */
    tstamp_datetime(tstampbuff);
    childval = agt_make_leaf(systemobj,
			     system_N_sysBootDateTime,
			     tstampbuff,
			     &res);
    if (childval) {
	val_add_child(childval, topval);
    }


    /* add sysStartup to notificationQ */
    send_sysStartup();

    return NO_ERR;

}  /* agt_sys_init2 */


/********************************************************************
* FUNCTION agt_sys_cleanup
*
* Cleanup the module data structures
*
* INPUTS:
*   
* RETURNS:
*   none
*********************************************************************/
void 
    agt_sys_cleanup (void)
{
    if (agt_sys_init_done) {
	init_static_sys_vars();
	agt_sys_init_done = FALSE;
    }
}  /* agt_sys_cleanup */


/********************************************************************
* FUNCTION agt_sys_send_sysSessionStart
*
* Queue the <sysSessionStart> notification
*
* INPUTS:
*   scb == session control block to use for payload values
*
* OUTPUTS:
*   notification generated and added to notificationQ
*
*********************************************************************/
void
    agt_sys_send_sysSessionStart (const ses_cb_t *scb)
{
    agt_not_msg_t         *not;

    if (LOGDEBUG) {
	log_debug("\nagt_sys: generating <sysSessionStart> "
		  "notification");
    }

    not = agt_not_new_notification(sysSessionStartobj);
    if (!not) {
	log_error("\nError: malloc failed; cannot "
		  "send <sysSessionStartup>");
	return;
    }

    add_common_session_parms(scb, not);

    agt_not_queue_notification(not);

} /* agt_sys_send_sysSessionStart */


/********************************************************************
* FUNCTION agt_sys_send_sysSessionEnd
*
* Queue the <sysSessionEnd> notification
*
* INPUTS:
*   scb == session control block to use for payload values
*   termreason == enum for the terminationReason leaf
*   killedby == session-id for killedBy leaf if termreason == "killed"
*               ignored otherwise
*
* OUTPUTS:
*   notification generated and added to notificationQ
*
*********************************************************************/
void
    agt_sys_send_sysSessionEnd (const ses_cb_t *scb,
				ses_term_reason_t termreason,
				ses_id_t killedby)
{
    agt_not_msg_t         *not;
    val_value_t           *leafval;
    const xmlChar         *termreasonstr;
    status_t               res;
    xmlChar                numbuff[NCX_MAX_NUMLEN];

#ifdef DEBUG
    if (!scb || !termreason) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (termreason == SES_TR_NOSTART) {
	/* session did not start; just being killed
	 * in the <ncxconnect> message handler
	 */
	return;
    }

    if (LOGDEBUG) {
	log_debug("\nagt_sys: generating <sysSessionEnd> "
		  "notification");
    }

    not = agt_not_new_notification(sysSessionEndobj);
    if (!not) {
	log_error("\nError: malloc failed; cannot "
		  "send <sysSessionEnd>");
	return;
    }

    add_common_session_parms(scb, not);

    /* add sysSessionEnd/killedBy */
    if (termreason == SES_TR_KILLED) {
	sprintf((char *)numbuff, "%u", killedby);
	leafval = agt_make_leaf(sysSessionEndobj,
				system_N_killedBy,
				numbuff,
				&res);
	if (leafval) {
	    agt_not_add_to_payload(not, leafval);
	} else {
	    log_error("\nError: cannot make payload leaf (%s)",
		      get_error_string(res));
	}
    }

    switch (termreason) {
    case SES_TR_NONE:
	SET_ERROR(ERR_INTERNAL_VAL);
	termreasonstr = (const xmlChar *)"other";
	break;
    case SES_TR_CLOSED:
	termreasonstr = (const xmlChar *)"closed";
	break;
    case SES_TR_KILLED:
	termreasonstr = (const xmlChar *)"killed";
	break;
    case SES_TR_DROPPED:
	termreasonstr = (const xmlChar *)"dropped";
	break;
    case SES_TR_TIMEOUT:
	termreasonstr = (const xmlChar *)"timeout";
	break;
    case SES_TR_OTHER:
	termreasonstr = (const xmlChar *)"other";
	break;
    case SES_TR_NOSTART:
    default:
	SET_ERROR(ERR_INTERNAL_VAL);
	termreasonstr = (const xmlChar *)"other";
    }

    /* add sysSessionEnd/terminationReason */
    leafval = agt_make_leaf(sysSessionEndobj,
			    system_N_terminationReason,
			    termreasonstr,
			    &res);
    if (leafval) {
	agt_not_add_to_payload(not, leafval);
    } else {
	log_error("\nError: cannot make payload leaf (%s)",
		  get_error_string(res));
    }

    agt_not_queue_notification(not);

} /* agt_sys_send_sysSessionEnd */


/********************************************************************
* FUNCTION agt_sys_send_sysConfigChange
*
* Queue the <sysConfigChange> notification
*
* INPUTS:
*   scb == session control block to use for payload values
*   target == instance-identifier of edit target
*   operation == nc:operation attribute value
*
* OUTPUTS:
*   notification generated and added to notificationQ
*
*********************************************************************/
void
    agt_sys_send_sysConfigChange (const ses_cb_t *scb,
				  const xmlChar *target,
				  const xmlChar *operation)
{
    agt_not_msg_t         *not;
    val_value_t           *leafval;
    status_t               res;

#ifdef DEBUG
    if (!scb || !target || !operation) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (LOGDEBUG) {
	log_debug("\nagt_sys: generating <sysConfigChange> "
		  "notification");
    }

    not = agt_not_new_notification(sysConfigChangeobj);
    if (!not) {
	log_error("\nError: malloc failed; cannot "
		  "send <sysConfigChange>");
	return;
    }

    add_common_session_parms(scb, not);

    /* add sysConfigChange/target */
    leafval = agt_make_leaf(sysConfigChangeobj,
			    system_N_target,
			    target,
			    &res);
    if (leafval) {
	agt_not_add_to_payload(not, leafval);
    } else {
	log_error("\nError: cannot make payload leaf (%s)",
		  get_error_string(res));
    }

    /* add sysConfigChange/operation */
    leafval = agt_make_leaf(sysConfigChangeobj,
			    system_N_operation,
			    operation,
			    &res);
    if (leafval) {
	agt_not_add_to_payload(not, leafval);
    } else {
	log_error("\nError: cannot make payload leaf (%s)",
		  get_error_string(res));
    }

    agt_not_queue_notification(not);

} /* agt_sys_send_sysConfigChange */


/* END file agt_sys.c */
