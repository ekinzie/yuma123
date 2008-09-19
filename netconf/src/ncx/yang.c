/*  FILE: yang.c


   YANG module parser utilities

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
15novt07      abb      begun; split out from yang_parse.c


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>

#include <xmlstring.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_def_reg
#include "def_reg.h"
#endif

#ifndef _H_dlq
#include  "dlq.h"
#endif

#ifndef _H_ext
#include  "ext.h"
#endif

#ifndef _H_grp
#include  "grp.h"
#endif

#ifndef _H_log
#include "log.h"
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

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tstamp
#include  "tstamp.h"
#endif

#ifndef _H_typ
#include  "typ.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_yang
#include "yang.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define YANG_DEBUG 1
#endif

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/



/********************************************************************
* FUNCTION yang_consume_semiapp
* 
* Consume a semi-colon to end a simple clause, or
* consume a vendor extension
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   appinfoQ == queue to receive any vendor extension found
*
* OUTPUTS:
*   *appinfoQ has the extesnion added if any
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_semiapp (tk_chain_t *tkc,
			  ncx_module_t *mod,
			  dlq_hdr_t *appinfoQ)
{
    const char   *expstr;
    status_t      res, retres;
    boolean       done;

#ifdef DEBUG
    if (!tkc) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    expstr = "semi-colon or left brace";
    retres = NO_ERR;

    /* get semicolon or left brace */
    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_mod_exp_err(tkc, mod, res, expstr);
	return res;
    }
	
    switch (TK_CUR_TYP(tkc)) {
    case TK_TT_SEMICOL:
	return NO_ERR;
    case TK_TT_LBRACE:
	break;
    case TK_TT_NONE:
    default:
	res = ERR_NCX_WRONG_TKTYPE;
	ncx_mod_exp_err(tkc, mod, res, expstr);
	switch (TK_CUR_TYP(tkc)) {
	case TK_TT_TSTRING:
	case TK_TT_MSTRING:
	case TK_TT_RBRACE:
	    /* try to recover and keep parsing */
	    TK_BKUP(tkc);
	    break;
	default:
	    ;
	}
	return res;
    }

    /* got the start of a vendor extension section
     * got left brace okay
     */
    done = FALSE;
    while (!done) {
	res = ncx_consume_appinfo2(tkc, mod, appinfoQ);
	if (res == ERR_NCX_SKIPPED) {
	    res = NO_ERR;
	    done = TRUE;
	}  else {
	    CHK_EXIT;
	}
    }

    res = ncx_consume_token(tkc, mod, TK_TT_RBRACE);
    CHK_EXIT;

    return retres;

}  /* yang_consume_semiapp */


/********************************************************************
* FUNCTION yang_consume_string
* 
* Consume a YANG string token in any of the 3 forms
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   field == address of field to get the string (may be NULL)
*
* OUTPUTS:
*   if field not NULL,
*        *field is set with a malloced string in NO_ERR
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_string (tk_chain_t *tkc,
			 ncx_module_t *mod,
			 xmlChar **field)
{
    const char *expstr;
    xmlChar    *str;
    status_t    res;

    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    if (TK_CUR_STR(tkc)) {
	if (field) {
	    if (TK_CUR_MOD(tkc)) {
		*field = m__getMem(TK_CUR_MODLEN(tkc)+TK_CUR_LEN(tkc)+2);
		if (*field) {
		    str = *field;
		    str += xml_strncpy(str, TK_CUR_MOD(tkc),
				       TK_CUR_MODLEN(tkc));
		    *str++ = ':';
		    if (TK_CUR_VAL(tkc)) {
			xml_strncpy(str, TK_CUR_VAL(tkc),
				    TK_CUR_LEN(tkc));
		    } else {
			*str = 0;
		    }
		}
	    } else {
		if (TK_CUR_VAL(tkc)) {
		    *field = xml_strdup(TK_CUR_VAL(tkc));
		} else {
		    *field = xml_strdup((const xmlChar *)"");
		}
	    }
	    if (!*field) {
		res = ERR_INTERNAL_MEM;
		ncx_print_errormsg(tkc, mod, res);
	    }
	}
    } else {
	switch (TK_CUR_TYP(tkc)) {
	case TK_TT_NONE:
	    res = ERR_NCX_EOF;
	    ncx_print_errormsg(tkc, mod, res);
	    break;
	case TK_TT_LBRACE:
	case TK_TT_RBRACE:
	case TK_TT_SEMICOL:
	    res = ERR_NCX_WRONG_TKTYPE;
	    expstr = "string";
	    ncx_mod_exp_err(tkc, mod, res, expstr);
	    break;
	default:
	    if (field) {
		if (TK_CUR_VAL(tkc)) {
		    *field = xml_strdup(TK_CUR_VAL(tkc));
		} else {
		    *field = 
			xml_strdup((const xmlChar *)
				   tk_get_token_name(TK_CUR_TYP(tkc)));
		}
		if (!*field) {
		    res = ERR_INTERNAL_MEM;
		    ncx_print_errormsg(tkc, mod, res);
		}
	    }
	}
    }

    return res;

}  /* yang_consume_string */


/********************************************************************
* FUNCTION yang_consume_keyword
* 
* Consume a YANG keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   prefix == address of field to get the prefix string (may be NULL)
*   field == address of field to get the name string (may be NULL)
*
* OUTPUTS:
*   if prefix not NULL,
*        *prefix is set with a malloced string in NO_ERR
*   if field not NULL,
*        *field is set with a malloced string in NO_ERR
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_keyword (tk_chain_t *tkc,
			  ncx_module_t *mod,
			  xmlChar **prefix,
			  xmlChar **field)
{
    tk_type_t      tktyp;
    status_t       res, retres;


    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    retres = NO_ERR;
    tktyp = TK_CUR_TYP(tkc);

    if (tktyp==TK_TT_QSTRING || tktyp==TK_TT_SQSTRING) {
	res = ERR_NCX_INVALID_VALUE;
	log_error("\nError: quoted strings not allowed for keywords");
    } else if (TK_CUR_ID(tkc)) {
	if (TK_CUR_VAL(tkc)) {

	    /* right kind of tokens, validate id name string */
	    if (ncx_valid_name(TK_CUR_VAL(tkc), TK_CUR_LEN(tkc))) {
		if (field) {
		    *field = xml_strdup(TK_CUR_VAL(tkc));
		    if (!*field) {
			res = ERR_INTERNAL_MEM;
		    }
		}
	    } else {
		res = ERR_NCX_INVALID_NAME;
	    }
	    
	    if (res != NO_ERR) {
		ncx_mod_exp_err(tkc, mod, res, "identifier-ref string");
		retres = res;
		res = NO_ERR;
	    }

	    /* validate prefix name string if any */
	    if (TK_CUR_MOD(tkc)) {
		if (ncx_valid_name(TK_CUR_MOD(tkc),
				   TK_CUR_MODLEN(tkc))) {
		    if (prefix) {
			*prefix = xml_strdup(TK_CUR_MOD(tkc));
			if (!*prefix) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		} else {
		    res = ERR_NCX_INVALID_NAME;
		}
	    }
	} else {
	    res = ERR_NCX_INVALID_NAME;
	}
    } else {
	res = ERR_NCX_WRONG_TKTYPE;
    }

    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	retres = res;
    }
    return retres;

}  /* yang_consume_keyword */


/********************************************************************
* FUNCTION yang_consume_nowsp_string
* 
* Consume a YANG string token in any of the 3 forms
* Check No whitespace allowed!
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   field == address of field to get the string (may be NULL)
*
* OUTPUTS:
*   if field not NULL,
*      *field is set with a malloced string in NO_ERR
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_nowsp_string (tk_chain_t *tkc,
			       ncx_module_t *mod,
			       xmlChar **field)
{
    xmlChar      *str;
    status_t      res;

    res = TK_ADV(tkc);
    if (res == NO_ERR) {
	if (TK_CUR_STR(tkc)) {
	    if (TK_CUR_MOD(tkc)) {
		if (field) {
		    *field = m__getMem(TK_CUR_MODLEN(tkc)+TK_CUR_LEN(tkc)+2);
		    if (*field) {
			str = *field;
			str += xml_strncpy(str, TK_CUR_MOD(tkc),
					   TK_CUR_MODLEN(tkc));
			*str++ = ':';
			if (TK_CUR_VAL(tkc)) {
			    xml_strncpy(str, TK_CUR_VAL(tkc),
					TK_CUR_LEN(tkc));
			} else {
			    *str = 0;
			}
		    }
		}
	    } else if (TK_CUR_VAL(tkc)) {
		if (!tk_is_wsp_string(TK_CUR(tkc))) {
		    if (field) {
			*field = xml_strdup(TK_CUR_VAL(tkc));
			if (!*field) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		} else {
		    res = ERR_NCX_INVALID_VALUE;
		}
	    } else {
		res = ERR_NCX_INVALID_VALUE;
	    }
	} else {
	    res = ERR_NCX_WRONG_TKTYPE;
	}
    }

    if (res != NO_ERR) {
	ncx_mod_exp_err(tkc, mod, res, "string w/o whitespace");
    }
    return res;
    
}  /* yang_consume_nowsp_string */


/********************************************************************
* FUNCTION yang_consume_id_string
* 
* Consume a YANG string token in any of the 3 forms
* Check that it is a valid YANG identifier string

* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   field == address of field to get the string (may be NULL)
*
* OUTPUTS:
*  if field not NULL:
*       *field is set with a malloced string in NO_ERR
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_id_string (tk_chain_t *tkc,
			    ncx_module_t *mod,
			    xmlChar **field)
{
    status_t      res;

    res = TK_ADV(tkc);
    if (res == NO_ERR) {
	if (TK_CUR_ID(tkc) ||
	    (TK_CUR_STR(tkc) && !tk_is_wsp_string(TK_CUR(tkc)))) {
	    if (TK_CUR_MOD(tkc)) {
		log_error("\nError: Prefix '%s' not allowed",
			  TK_CUR_MOD(tkc));
		res = ERR_NCX_INVALID_NAME;
	    } else if (TK_CUR_VAL(tkc)) {
		if (ncx_valid_name(TK_CUR_VAL(tkc), TK_CUR_LEN(tkc))) {
		    if (field) {
			*field = xml_strdup(TK_CUR_VAL(tkc));
			if (!*field) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		} else {
		    res = ERR_NCX_INVALID_NAME;
		}
	    } else {
		res = ERR_NCX_INVALID_NAME;
	    }
	} else {
	    res = ERR_NCX_WRONG_TKTYPE;
	}
    }

    if (res != NO_ERR) {
	ncx_mod_exp_err(tkc, mod, res, "identifier string");
    }
    return res;
    
}  /* yang_consume_id_string */


/********************************************************************
* FUNCTION yang_consume_pid_string
* 
* Consume a YANG string token in any of the 3 forms
* Check that it is a valid identifier-ref-string
* Get the prefix if any is set
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain
*   mod == module in progress
*   prefix == address of prefix string if any is used (may be BULL)
*   field == address of field to get the string (may be NULL)
*
* OUTPUTS:
*  if field not NULL:
*       *field is set with a malloced string in NO_ERR
*   current token is advanced
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_pid_string (tk_chain_t *tkc,
			     ncx_module_t *mod,
			     xmlChar **prefix,
			     xmlChar **field)
{
    const xmlChar *p;
    tk_type_t      tktyp;
    status_t       res, retres;
    uint32         plen, nlen;

    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    retres = NO_ERR;
    tktyp = TK_CUR_TYP(tkc);

    if (tktyp==TK_TT_QSTRING || tktyp==TK_TT_SQSTRING) {
	p = TK_CUR_VAL(tkc);
	while (*p && *p != ':') {
	    p++;
	}
	if (*p) {
	    /* found the prefix separator char,
	     * try to parse this quoted string
	     * as a prefix:identifier
	     */
	    plen = (uint32)(p - TK_CUR_VAL(tkc));
	    nlen = TK_CUR_LEN(tkc) - (plen+1);

	    if (plen && plen+1 < TK_CUR_LEN(tkc)) {
		/* have a possible string structure to try */
		if (ncx_valid_name(TK_CUR_VAL(tkc), plen) &&
		    ncx_valid_name(p+1, nlen)) {

		    /* have valid syntax for prefix:identifier */
		    if (prefix) {
			*prefix = xml_strndup(TK_CUR_VAL(tkc), plen);
			if (!*prefix) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		    if (field) {
			*field = xml_strndup(p+1, nlen);
			if (!*field) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		    if (res != NO_ERR) {
			ncx_print_errormsg(tkc, mod, res);
		    }
		    return res;
		}
	    }
	}
    }

    if (TK_CUR_ID(tkc) ||
	(TK_CUR_STR(tkc) && !tk_is_wsp_string(TK_CUR(tkc)))) {
	if (TK_CUR_VAL(tkc)) {
	    /* right kind of tokens, validate id name string */
	    if (ncx_valid_name(TK_CUR_VAL(tkc), TK_CUR_LEN(tkc))) {
		if (field) {
		    *field = xml_strdup(TK_CUR_VAL(tkc));
		    if (!*field) {
			res = ERR_INTERNAL_MEM;
		    }
		}
	    } else {
		res = ERR_NCX_INVALID_NAME;
	    }
	    
	    if (res != NO_ERR) {
		ncx_mod_exp_err(tkc, mod, res, "identifier-ref string");
		retres = res;
		res = NO_ERR;
	    }

	    /* validate prefix name string if any */
	    if (TK_CUR_MOD(tkc)) {
		if (ncx_valid_name(TK_CUR_MOD(tkc),
				   TK_CUR_MODLEN(tkc))) {
		    if (prefix) {
			*prefix = xml_strdup(TK_CUR_MOD(tkc));
			if (!*prefix) {
			    res = ERR_INTERNAL_MEM;
			}
		    }
		} else {
		    res = ERR_NCX_INVALID_NAME;
		}
	    }
	} else {
	    res = ERR_NCX_INVALID_NAME;
	}
    } else {
	res = ERR_NCX_WRONG_TKTYPE;
    }

    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	retres = res;
    }
    return retres;
    
}  /* yang_consume_pid_string */


/********************************************************************
* FUNCTION yang_consume_error_stmts
* 
* Parse the sub-section as a sub-section for error-app-tag
* and error-message clauses
*
* Current token is the starting left brace for the sub-section
* that is extending a range, length, pattern, or must statement
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   errinfo == address of pointer to get the malloced 
*              ncx_errinfo_t struct, filled in by this fn
*   appinfoQ == Q to hold any extensions found
*
* OUTPUTS:
*   *errinfo filled in with any clauses found
*   *appinfoQ filled in with any extensions found
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_error_stmts (tk_chain_t  *tkc,
			      ncx_module_t *mod,
			      ncx_errinfo_t  **errinfo,
			      dlq_hdr_t *appinfoQ)
{
    const xmlChar *val;
    const char    *expstr;
    ncx_errinfo_t *err;
    tk_type_t      tktyp;
    status_t       res, retres;
    boolean        done, desc, ref, etag, emsg;

#ifdef DEBUG
    if (!tkc || !errinfo) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    expstr = "description, reference, error-app-tag, "
	"or error-message keyword";
    done = FALSE;
    desc = FALSE;
    ref = FALSE;
    etag = FALSE;
    emsg = FALSE;
    res = NO_ERR;
    retres = NO_ERR;

    err = ncx_new_errinfo();
    if (!err) {
	res = ERR_INTERNAL_MEM;
	ncx_print_errormsg(tkc, mod, res);
	return res;
    } else {
	*errinfo = err;
    }

    while (!done) {

	/* get the next token */
	res = TK_ADV(tkc);
	if (res != NO_ERR) {
	    ncx_print_errormsg(tkc, mod, res);
	    return res;
	}

	tktyp = TK_CUR_TYP(tkc);
	val = TK_CUR_VAL(tkc);

	/* check the current token type */
	switch (tktyp) {
	case TK_TT_NONE:
	    res = ERR_NCX_EOF;
	    ncx_print_errormsg(tkc, mod, res);
	    return res;
	case TK_TT_MSTRING:
	    /* vendor-specific clause found instead */
	    res = ncx_consume_appinfo(tkc, mod, appinfoQ);
	    CHK_EXIT;
	    continue;
	case TK_TT_RBRACE:
	    done = TRUE;
	    continue;
	case TK_TT_TSTRING:
	    break;  /* YANG clause assumed */
	default:
	    retres = ERR_NCX_WRONG_TKTYPE;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	    continue;
	}

	/* Got a token string so check the value */
        if (!xml_strcmp(val, YANG_K_DESCRIPTION)) {
	    /* Optional 'description' field is present */
	    res = yang_consume_descr(tkc, mod, &err->descr,
				     &desc, appinfoQ);
	    CHK_EXIT;
        } else if (!xml_strcmp(val, YANG_K_REFERENCE)) {
	    /* Optional 'description' field is present */
	    res = yang_consume_descr(tkc, mod, &err->ref,
				     &ref, appinfoQ);
	    CHK_EXIT;
        } else if (!xml_strcmp(val, YANG_K_ERROR_APP_TAG)) {
	    /* Optional 'error-app-tag' field is present */
	    res = yang_consume_strclause(tkc, mod,
					 &err->error_app_tag,
					 &etag, appinfoQ);
	    CHK_EXIT;
        } else if (!xml_strcmp(val, YANG_K_ERROR_MESSAGE)) {
	    /* Optional 'error-app-tag' field is present */
	    res = yang_consume_strclause(tkc, mod,
					 &err->error_message,
					 &emsg, appinfoQ);
	    CHK_EXIT;
	} else {
	    retres = ERR_NCX_WRONG_TKVAL;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);	    
	}
    }

    return retres;

}  /* yang_consume_error_stmts */


/********************************************************************
* FUNCTION yang_consume_descr
* 
* Parse the description or reference statement
*
* Current token is the starting keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   str == string to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found
*
* OUTPUTS:
*   *str set to the value if str not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_descr (tk_chain_t  *tkc,
			ncx_module_t *mod,
			xmlChar **str,
			boolean *dupflag,
			dlq_hdr_t *appinfoQ)
{
    status_t    res, retres;
    boolean     save;

    save = TRUE;
    res = NO_ERR;
    retres = NO_ERR;

    if (dupflag) {
	if (*dupflag) {
	    retres = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, retres);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    /* get the string value */
    if (ncx_save_descr() && str && save) {
	res = yang_consume_string(tkc, mod, str);
    } else {
	res = yang_consume_string(tkc, mod, NULL);
    }
    CHK_EXIT;

    /* finish the clause */
    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

} /* yang_consume_descr */


/********************************************************************
* FUNCTION yang_consume_strclause
* 
* Parse the string-parameter-based statement
*
* Current token is the starting keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   str == string to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   *str set to the value if str not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_strclause (tk_chain_t  *tkc,
			    ncx_module_t *mod,
			    xmlChar **str,
			    boolean *dupflag,
			    dlq_hdr_t *appinfoQ)
{
    status_t    res, retres;
    boolean     save;

    save = TRUE;
    res = NO_ERR;
    retres = NO_ERR;

    if (dupflag) {
	if (*dupflag) {
	    retres = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, retres);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    /* get the string value */
    if (str && save) {
	res = yang_consume_string(tkc, mod, str);
    } else {
	res = yang_consume_string(tkc, mod, NULL);
    }
    CHK_EXIT;

    /* finish the clause */
    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

}  /* yang_consume_strclause */


/********************************************************************
* FUNCTION yang_consume_status
* 
* Parse the status statement
*
* Current token is the 'status' keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   stat == status object to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   *stat set to the value if stat not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_status (tk_chain_t  *tkc,
			 ncx_module_t *mod,
			 ncx_status_t *status,
			 boolean *dupflag,
			 dlq_hdr_t *appinfoQ)
{
    xmlChar       *str;
    const char    *expstr;
    ncx_status_t   stat2;
    status_t       res, retres;
    boolean        save;

    expstr = "status enumeration string";
    retres = NO_ERR;
    save = TRUE;

    if (dupflag) {
	if (*dupflag) {
	    res = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, res);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    /* get the string value */
    res = yang_consume_string(tkc, mod, &str);
    if (res != NO_ERR) {
	retres = res;
	if (NEED_EXIT) {
	    if (str) {
		m__free(str);
	    }
	    return res;
	}
    }

    if (str) {
	if (status && save) {
	    *status = ncx_get_status_enum(str);
	    stat2 = *status;
	} else {
	    stat2 = ncx_get_status_enum(str);
	}

	if (save && stat2 == NCX_STATUS_NONE) {
	    retres = ERR_NCX_INVALID_VALUE;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	}
    }

    m__free(str);

    /* finish the clause */
    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

}  /* yang_consume_status */


/********************************************************************
* FUNCTION yang_consume_must
* 
* Parse the must statement
*
* Current token is the 'must' keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   mustQ  == address of Q of ncx_errinfo_t struct to store
*             a new malloced ncx_errinfo_t 
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   must entry malloced and  added to mustQ (if NO_ERR)
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t
    yang_consume_must (tk_chain_t  *tkc,
		       ncx_module_t *mod,
		       dlq_hdr_t *mustQ,
		       dlq_hdr_t *appinfoQ)
{
    const xmlChar *val;
    const char    *expstr;
    ncx_errinfo_t *must;
    tk_type_t      tktyp;
    status_t       res, retres;
    boolean        done, etag, emsg, desc, ref;
    
#ifdef DEBUG
    if (!tkc || !mustQ) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    must = ncx_new_errinfo();
    if (!must) {
	res = ERR_INTERNAL_MEM;
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    retres = NO_ERR;
    expstr = "Xpath expression string";
    done = FALSE;
    etag = FALSE;
    emsg = FALSE;
    desc = FALSE;
    ref = FALSE;

    /* get the Xpath string for the must expression */
    res = yang_consume_string(tkc, mod, &must->xpath);
    if (res != NO_ERR) {
	retres = res;
	if (NEED_EXIT) {
	    ncx_free_errinfo(must);
	    return retres;
	}
    }

    expstr = "error-message, error-app-tag, "
	"description, or reference keywords";

    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	ncx_free_errinfo(must);
	return res;
    }

    switch (TK_CUR_TYP(tkc)) {
    case TK_TT_SEMICOL:
	dlq_enque(must, mustQ);
	return retres;
    case TK_TT_LBRACE:
	break;
    default:
	res = ERR_NCX_WRONG_TKTYPE;
	ncx_mod_exp_err(tkc, mod, res, expstr);
	ncx_free_errinfo(must);
	return res;
    }

    while (!done) {

	/* get the next token */
	res = TK_ADV(tkc);
	if (res != NO_ERR) {
	    ncx_print_errormsg(tkc, mod, res);
	    ncx_free_errinfo(must);
	    return res;
	}

	tktyp = TK_CUR_TYP(tkc);
	val = TK_CUR_VAL(tkc);

	/* check the current token type */
	switch (tktyp) {
	case TK_TT_NONE:
	    res = ERR_NCX_EOF;
	    ncx_print_errormsg(tkc, mod, res);
	    ncx_free_errinfo(must);
	    return res;
	case TK_TT_MSTRING:
	    /* vendor-specific clause found instead */
	    res = ncx_consume_appinfo(tkc, mod, appinfoQ);
	    if (res != NO_ERR) {
		retres = res;
		if (NEED_EXIT) {
		    ncx_free_errinfo(must);
		    return retres;
		}
	    }
	    continue;
	case TK_TT_RBRACE:
	    dlq_enque(must, mustQ);
	    return retres;
	case TK_TT_TSTRING:
	    break;  /* YANG clause assumed */
	default:
	    retres = ERR_NCX_WRONG_TKTYPE;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	    ncx_free_errinfo(must);
	    continue;
	}

	/* Got a token string so check the value */
        if (!xml_strcmp(val, YANG_K_ERROR_APP_TAG)) {
	    /* Optional 'error-app-tag' field is present */
	    res = yang_consume_strclause(tkc, mod,
					 &must->error_app_tag,
					 &etag, appinfoQ);
        } else if (!xml_strcmp(val, YANG_K_ERROR_MESSAGE)) {
	    /* Optional 'error-message' field is present */
	    res = yang_consume_strclause(tkc, mod,
					 &must->error_message,
					 &emsg, appinfoQ);
        } else if (!xml_strcmp(val, YANG_K_DESCRIPTION)) {
	    /* Optional 'description' field is present */
	    res = yang_consume_descr(tkc, mod, &must->descr,
				     &desc, appinfoQ);
        } else if (!xml_strcmp(val, YANG_K_REFERENCE)) {
	    /* Optional 'reference' field is present */
	    res = yang_consume_descr(tkc, mod, &must->ref,
				     &ref, appinfoQ);
	} else {
	    expstr = "must sub-statement";
	    res = ERR_NCX_WRONG_TKVAL;
	    ncx_mod_exp_err(tkc, mod, res, expstr);
	}
	if (res != NO_ERR) {
	    retres = res;
	    if (NEED_EXIT) {
		ncx_free_errinfo(must);
		return retres;
	    }
	}
    }

    return retres;

}  /* yang_consume_must */


/********************************************************************
* FUNCTION yang_consume_boolean
* 
* Parse the boolean parameter based statement
*
* Current token is the starting keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   bool == boolean value to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   *str set to the value if str not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_boolean (tk_chain_t  *tkc,
			  ncx_module_t *mod,
			  boolean *bool,
			  boolean *dupflag,
			  dlq_hdr_t *appinfoQ)
{
    xmlChar    *str;
    const char *expstr;
    status_t    res, retres;
    boolean     save;

    retres = NO_ERR;
    expstr = "true or false keyword";
    str = NULL;
    save = TRUE;

    if (dupflag) {
	if (*dupflag) {
	    retres = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, retres);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    /* get the string value */
    res = yang_consume_string(tkc, mod, &str);
    if (res != NO_ERR) {
	retres = res;
	if (NEED_EXIT) {
	    if (str) {
		m__free(str);
	    }
	    return res;
	}
    }

    if (str) {
	if (!xml_strcmp(str, NCX_EL_TRUE)) {
	    if (save) {
		*bool = TRUE;
	    }
	} else if (!xml_strcmp(str, NCX_EL_FALSE)) {
	    if (save) {
		*bool = FALSE;
	    }
	} else {
	    retres = ERR_NCX_WRONG_TKVAL;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	}
	m__free(str);
    }

    /* finish the clause */
    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

}  /* yang_consume_boolean */


/********************************************************************
* FUNCTION yang_consume_int32
* 
* Parse the int32 based parameter statement
*
* Current token is the starting keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   num    == int32 value to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   *num set to the value if num not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_int32 (tk_chain_t  *tkc,
			ncx_module_t *mod,
			int32 *num,
			boolean *dupflag,
			dlq_hdr_t *appinfoQ)
{
    const char    *expstr;
    ncx_num_t      numstruct;
    status_t       res, retres;
    boolean        save;

    save = TRUE;
    retres = NO_ERR;
    expstr = "signed integer number";

    if (dupflag) {
	if (*dupflag) {
	    retres = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, retres);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    if (TK_CUR_NUM(tkc) || TK_CUR_STR(tkc)) {
	res = ncx_convert_tkcnum(tkc, NCX_BT_INT32, &numstruct);
	if (res == NO_ERR) {
	    if (num && save) {
		*num = numstruct.i;
	    }
	} else {
	    retres = res;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	}
    } else {
	retres = ERR_NCX_WRONG_TKTYPE;
	ncx_mod_exp_err(tkc, mod, retres, expstr);
    }

    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

}  /* yang_consume_int32 */


/********************************************************************
* FUNCTION yang_consume_uint32
* 
* Parse the uint32 based parameter statement
*
* Current token is the starting keyword
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   num    == uint32 value to set  (may be NULL)
*   dupflag == flag to check if entry already found (may be NULL)
*   appinfoQ == Q to hold any extensions found (may be NULL)
*
* OUTPUTS:
*   *num set to the value if num not NULL
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_consume_uint32 (tk_chain_t  *tkc,
			 ncx_module_t *mod,
			 uint32 *num,
			 boolean *dupflag,
			 dlq_hdr_t *appinfoQ)
{
    const char    *expstr;
    ncx_num_t      numstruct;
    status_t       res, retres;
    boolean        save;

    save = TRUE;
    retres = NO_ERR;
    expstr = "non-negative number";

    if (dupflag) {
	if (*dupflag) {
	    retres = ERR_NCX_ENTRY_EXISTS;
	    ncx_print_errormsg(tkc, mod, retres);
	    save = FALSE;
	} else {
	    *dupflag = TRUE;
	}
    }

    res = TK_ADV(tkc);
    if (res != NO_ERR) {
	ncx_print_errormsg(tkc, mod, res);
	return res;
    }

    if (TK_CUR_NUM(tkc) || TK_CUR_STR(tkc)) {
	res = ncx_convert_tkcnum(tkc, NCX_BT_UINT32, &numstruct);
	if (res == NO_ERR) {
	    if (num && save) {
		*num = numstruct.u;
	    }
	} else {
	    retres = res;
	    ncx_mod_exp_err(tkc, mod, retres, expstr);
	}
    } else {
	retres = ERR_NCX_WRONG_TKTYPE;
	ncx_mod_exp_err(tkc, mod, retres, expstr);
    }	

    if (save) {
	res = yang_consume_semiapp(tkc, mod, appinfoQ);
    } else {
	res = yang_consume_semiapp(tkc, mod, NULL);
    }
    CHK_EXIT;

    return retres;

}  /* yang_consume_uint32 */


/********************************************************************
* FUNCTION yang_find_imp_typedef
* 
* Find the specified imported typedef
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   prefix  == prefix value to use
*   name == type name to use
*   errtk == token to use in error messages (may be NULL)
*   typ  == address of return typ_template_t pointer
*
* OUTPUTS:
*   *typ set to the found typ_template_t, if NO_ERR
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_find_imp_typedef (tk_chain_t  *tkc,
			   ncx_module_t *mod,
			   const xmlChar *prefix,
			   const xmlChar *name,
			   tk_token_t *errtk,
			   typ_template_t **typ)
{
    const ncx_import_t   *imp;
    tk_token_t           *savetk;
    ncx_node_t            dtyp;
    status_t              res;

#ifdef DEBUG
    if (!tkc || !mod || !prefix || !name || !typ) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    imp = ncx_find_pre_import(mod, prefix);
    if (!imp) {
	res = ERR_NCX_PREFIX_NOT_FOUND;
	log_error("\nError: import for prefix '%s' not found",
		  prefix);
    } else {
	/* found import OK, look up imported type definition */
	dtyp = NCX_NT_TYP;
	*typ = (typ_template_t *)
	    ncx_locate_modqual_import(imp->module, name, 
				      mod->diffmode, &dtyp);
	if (!*typ) {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    log_error("\nError: typedef definition for '%s:%s' not found"
		      " in module %s", 
		      prefix, name, imp->module);

	} else {
	    return NO_ERR;
	}
    }

    if (errtk) {
	savetk = tkc->cur;
	tkc->cur = errtk;
	ncx_print_errormsg(tkc, mod, res);
	tkc->cur = savetk;
    } else {
	ncx_print_errormsg(tkc, mod, res);
    }

    return res;

}  /* yang_find_imp_typedef */


/********************************************************************
* FUNCTION yang_find_imp_grouping
* 
* Find the specified imported grouping
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   prefix  == prefix value to use
*   name == grouping name to use
*   errtk == token to use in error messages (may be NULL)
*   grp  == address of return grp_template_t pointer
*
* OUTPUTS:
*   *grp set to the found grp_template_t, if NO_ERR
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_find_imp_grouping (tk_chain_t  *tkc,
			    ncx_module_t *mod,
			    const xmlChar *prefix,
			    const xmlChar *name,
			    tk_token_t *errtk,
			    grp_template_t **grp)
{
    ncx_import_t   *imp;
    tk_token_t     *savetk;
    ncx_node_t      dtyp;
    status_t        res;

#ifdef DEBUG
    if (!tkc || !mod || !prefix || !name || !grp) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    res = NO_ERR;

    imp = ncx_find_pre_import(mod, prefix);
    if (!imp) {
	res = ERR_NCX_PREFIX_NOT_FOUND;
	log_error("\nError: import for prefix '%s' not found",
		  prefix);
    } else {
	/* found import OK, look up imported type definition */
	dtyp = NCX_NT_GRP;
	*grp = (grp_template_t *)
	    ncx_locate_modqual_import(imp->module, name, 
				      mod->diffmode, &dtyp);
	if (!*grp) {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    log_error("\nError: grouping definition for '%s:%s' not found"
		      " in module %s", 
		      prefix, name, imp->module);
	} else {
	    return NO_ERR;
	}
    }

    if (errtk) {
	savetk = tkc->cur;
	tkc->cur = errtk;
	ncx_print_errormsg(tkc, mod, res);
	tkc->cur = savetk;
    } else {
	ncx_print_errormsg(tkc, mod, res);
    }

    return res;

}  /* yang_find_imp_grouping */


/********************************************************************
* FUNCTION yang_find_imp_extension
* 
* Find the specified imported extension
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc    == token chain
*   mod    == module in progress
*   prefix  == prefix value to use
*   name == extension name to use
*   errtk == token to use in error messages (may be NULL)
*   ext  == address of return ext_template_t pointer
*
* OUTPUTS:
*   *ext set to the found ext_template_t, if NO_ERR
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    yang_find_imp_extension (tk_chain_t  *tkc,
			     ncx_module_t *mod,
			     const xmlChar *prefix,
			     const xmlChar *name,
			     tk_token_t *errtk,
			     ext_template_t **ext)
{
    ncx_import_t   *imp;
    ncx_module_t   *imod;
    tk_token_t     *savetk;
    status_t        res, retres;

#ifdef DEBUG
    if (!tkc || !mod || !prefix || !name || !ext) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    imod = NULL;
    res = NO_ERR;
    retres = NO_ERR;

    imp = ncx_find_pre_import(mod, prefix);
    if (!imp) {
	res = ERR_NCX_PREFIX_NOT_FOUND;
	log_error("\nError: import for prefix '%s' not found", prefix);
    } else {
	/* First find or load the module */
	if (mod->diffmode) {
	    imod = ncx_find_module(imp->module);
	} else {
	    imod = def_reg_find_module(imp->module);
	}
	if (!imod) {
	    res = ncxmod_load_module(imp->module);
	    CHK_EXIT;

	    /* try again to find the module; should not fail */
	    if (mod->diffmode) {
		imod = ncx_find_module(imp->module);
	    } else {
		imod = def_reg_find_module(imp->module);
	    }
	    if (!imod) {
		log_error("\nError: failure importing module '%s'",
			  imp->module);
		res = ERR_NCX_DEF_NOT_FOUND;
	    }
	}

	/* found import OK, look up imported extension definition */
	if (imod) {
	    *ext = ext_find_extension(&imod->extensionQ, name);
	    if (!*ext) {
		res = ERR_NCX_DEF_NOT_FOUND;
		log_error("\nError: extension definition for '%s:%s' not found"
			  " in module %s", 
			  prefix, name, imp->module);
	    } else {
		return NO_ERR;
	    }
	}
    }

    if (errtk) {
	savetk = tkc->cur;
	tkc->cur = errtk;
	ncx_print_errormsg(tkc, mod, res);
	tkc->cur = savetk;
    } else {
	ncx_print_errormsg(tkc, mod, res);
    }

    return res;

}  /* yang_find_imp_extension */


/********************************************************************
* FUNCTION yang_check_obj_used
* 
* Check if the local typedefs and groupings are actually used
* Generate warnings if not used
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain in progress
*   mod == module in progress
*   typeQ == typedef Q to check
*   grpQ == pgrouping Q to check
*
*********************************************************************/
void
    yang_check_obj_used (tk_chain_t *tkc,
			 ncx_module_t *mod,
			 dlq_hdr_t *typeQ,
			 dlq_hdr_t *grpQ)
{
    typ_template_t *testtyp;
    grp_template_t *testgrp;

#ifdef DEBUG
    if (!tkc || !mod || !typeQ || !grpQ) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    for (testtyp = (typ_template_t *)dlq_firstEntry(typeQ);
	 testtyp != NULL;
	 testtyp = (typ_template_t *)dlq_nextEntry(testtyp)) {
	if (!testtyp->used) {
	    log_warn("\nWarning: Local typedef '%s' not used",
		     testtyp->name);
	    tkc->cur = testtyp->tk;
	    ncx_print_errormsg(tkc, mod, ERR_NCX_TYPDEF_NOT_USED);
	}
    }
    for (testgrp = (grp_template_t *)dlq_firstEntry(grpQ);
	 testgrp != NULL;
	 testgrp = (grp_template_t *)dlq_nextEntry(testgrp)) {
	if (!testgrp->used) {
	    log_warn("\nWarning: Local grouping '%s' not used",
		     testgrp->name);
	    tkc->cur = testgrp->tk;
	    ncx_print_errormsg(tkc, mod, ERR_NCX_GRPDEF_NOT_USED);
	}
    }
} /* yang_check_obj_used */


/********************************************************************
* FUNCTION yang_check_imports_used
* 
* Check if the imports statements are actually used
* Generate warnings if not used
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain in progress
*   mod == module in progress
*
*********************************************************************/
void
    yang_check_imports_used (tk_chain_t *tkc,
			     ncx_module_t *mod)
{
    ncx_import_t *testimp;

#ifdef DEBUG
    if (!tkc || !mod) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    for (testimp = (ncx_import_t *)dlq_firstEntry(&mod->importQ);
	 testimp != NULL;
	 testimp = (ncx_import_t *)dlq_nextEntry(testimp)) {
	if (!testimp->used) {
	    log_warn("\nWarning: Module '%s' not used", testimp->module);
	    tkc->cur = testimp->tk;
	    ncx_print_errormsg(tkc, mod, ERR_NCX_IMPORT_NOT_USED);
	}
    }
} /* yang_check_imports_used */


/********************************************************************
* FUNCTION yang_new_node
* 
* Create a new YANG parser node
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_node_t *
    yang_new_node (void)
{
    yang_node_t *node;

    node = m__getObj(yang_node_t);
    if (!node) {
	return NULL;
    }
    memset(node, 0x0, sizeof(yang_node_t));
    return node;

} /* yang_new_node */


/********************************************************************
* FUNCTION yang_free_node
* 
* Delete a YANG parser node
*
* INPUTS:
*  node == parser node to delete
*
*********************************************************************/
void
    yang_free_node (yang_node_t *node)
{
#ifdef DEBUG
    if (!node) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (node->submod) {
	ncx_free_module(node->submod);
    }
    if (node->failed) {
	m__free(node->failed);
    }
    if (node->tkc) {
	tk_free_chain(node->tkc);
    }
    m__free(node);

}  /* yang_free_node */


/********************************************************************
* FUNCTION yang_clean_nodeQ
* 
* Delete all the node entries in a YANG parser node Q
*
* INPUTS:
*    que == Q of yang_node_t to clean
*
*********************************************************************/
void
    yang_clean_nodeQ (dlq_hdr_t *que)
{
    yang_node_t *node;

#ifdef DEBUG
    if (!que) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    while (!dlq_empty(que)) {
	node = (yang_node_t *)dlq_deque(que);
	yang_free_node(node);
    }

}  /* yang_clean_nodeQ */


/********************************************************************
* FUNCTION yang_find_node
* 
* Find a YANG parser node in the specified Q
*
* INPUTS:
*    que == Q of yang_node_t to check
*    name == module name to find
*
* RETURNS:
*    pointer to found node, NULL if not found
*********************************************************************/
yang_node_t *
    yang_find_node (dlq_hdr_t *que,
		    const xmlChar *name)
{
    yang_node_t *node;

#ifdef DEBUG
    if (!que || !name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    for (node = (yang_node_t *)dlq_firstEntry(que);
	 node != NULL;
	 node = (yang_node_t *)dlq_nextEntry(node)) {

	if (!xml_strcmp(node->name, name)) {
	    return node;
	}
    }
    return NULL;

}  /* yang_find_node */


/********************************************************************
* FUNCTION yang_dump_nodeQ
* 
* log_debug output for contents of the specified nodeQ
*
* INPUTS:
*    que == Q of yang_node_t to check
*    name == Q name (may be NULL)
*
* RETURNS:
*    pointer to found node, NULL if not found
*********************************************************************/
void
    yang_dump_nodeQ (dlq_hdr_t *que,
		     const char *name)
{
    yang_node_t *node;
    boolean      anyout;

#ifdef DEBUG
    if (!que) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (name) {
	anyout = TRUE;
	log_debug3("\n%s Q:", name);
    }

    for (node = (yang_node_t *)dlq_firstEntry(que);
	 node != NULL;
	 node = (yang_node_t *)dlq_nextEntry(node)) {

	anyout = TRUE;
	log_debug3("\nNode %s ", node->name);

	if (node->res != NO_ERR) {
	    log_debug3("res: %s ", get_error_string(node->res));
	}

	if (node->mod) {
	    log_debug3("%smod:%s",
		      node->mod->ismod ? "" : "sub", node->mod->name);
	}
    }

    if (anyout) {
	log_debug3("\n");
    }

}  /* yang_dump_nodeQ */


/********************************************************************
* FUNCTION yang_new_pcb
* 
* Create a new YANG parser control block
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_pcb_t *
    yang_new_pcb (void)
{
    yang_pcb_t *pcb;

    pcb = m__getObj(yang_pcb_t);
    if (!pcb) {
	return NULL;
    }

    memset(pcb, 0x0, sizeof(yang_pcb_t));
    dlq_createSQue(&pcb->impchainQ);
    dlq_createSQue(&pcb->allimpQ);
    dlq_createSQue(&pcb->allincQ);
    dlq_createSQue(&pcb->incchainQ);
    dlq_createSQue(&pcb->failedQ);
    /* needed for init load and imports for yangdump
     * do not do this for the agent, which will set 'save descr' 
     * mode to FALSE to indicate do not care about module display
     */
    pcb->stmtmode = ncx_save_descr(); 
    return pcb;

} /* yang_new_pcb */


/********************************************************************
* FUNCTION yang_free_pcb
* 
* Delete a YANG parser control block
*
* INPUTS:
*    pcb == parser control block to delete
*
*********************************************************************/
void
    yang_free_pcb (yang_pcb_t *pcb)
{
#ifdef DEBUG
    if (!pcb) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (pcb->top && !pcb->top->ismod) {
	ncx_free_module(pcb->top);
    }

    yang_clean_import_ptrQ(&pcb->allimpQ);

    yang_clean_nodeQ(&pcb->impchainQ);
    yang_clean_nodeQ(&pcb->allincQ);
    yang_clean_nodeQ(&pcb->incchainQ);
    yang_clean_nodeQ(&pcb->failedQ);
    m__free(pcb);

}  /* yang_free_pcb */


/********************************************************************
* FUNCTION yang_new_typ_stmt
* 
* Create a new YANG stmt node for a typedef
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_stmt_t *
    yang_new_typ_stmt (typ_template_t *typ)
{
    yang_stmt_t *stmt;

#ifdef DEBUG
    if (!typ) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    stmt = m__getObj(yang_stmt_t);
    if (!stmt) {
	return NULL;
    }
    memset(stmt, 0x0, sizeof(yang_stmt_t));
    stmt->stmttype = YANG_ST_TYPEDEF;
    stmt->s.typ = typ;
    return stmt;

} /* yang_new_typ_stmt */


/********************************************************************
* FUNCTION yang_new_grp_stmt
* 
* Create a new YANG stmt node for a grouping
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_stmt_t *
    yang_new_grp_stmt (grp_template_t *grp)
{
    yang_stmt_t *stmt;

#ifdef DEBUG
    if (!grp) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    stmt = m__getObj(yang_stmt_t);
    if (!stmt) {
	return NULL;
    }
    memset(stmt, 0x0, sizeof(yang_stmt_t));
    stmt->stmttype = YANG_ST_GROUPING;
    stmt->s.grp = grp;
    return stmt;

} /* yang_new_grp_stmt */


/********************************************************************
* FUNCTION yang_new_ext_stmt
* 
* Create a new YANG stmt node for an extension
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_stmt_t *
    yang_new_ext_stmt (ext_template_t *ext)
{
    yang_stmt_t *stmt;

#ifdef DEBUG
    if (!ext) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    stmt = m__getObj(yang_stmt_t);
    if (!stmt) {
	return NULL;
    }
    memset(stmt, 0x0, sizeof(yang_stmt_t));
    stmt->stmttype = YANG_ST_EXTENSION;
    stmt->s.ext = ext;
    return stmt;

} /* yang_new_ext_stmt */


/********************************************************************
* FUNCTION yang_new_obj_stmt
* 
* Create a new YANG stmt node for an object
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_stmt_t *
    yang_new_obj_stmt (obj_template_t *obj)
{
    yang_stmt_t *stmt;

#ifdef DEBUG
    if (!obj) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    stmt = m__getObj(yang_stmt_t);
    if (!stmt) {
	return NULL;
    }
    memset(stmt, 0x0, sizeof(yang_stmt_t));
    stmt->stmttype = YANG_ST_OBJECT;
    stmt->s.obj = obj;
    return stmt;

} /* yang_new_obj_stmt */


/********************************************************************
* FUNCTION yang_free_stmt
* 
* Delete a YANG statement node
*
* INPUTS:
*    stmt == yang_stmt_t node to delete
*
*********************************************************************/
void
    yang_free_stmt (yang_stmt_t *stmt)
{
#ifdef DEBUG
    if (!stmt) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    m__free(stmt);

}  /* yang_free_stmt */


/********************************************************************
* FUNCTION yang_clean_stmtQ
* 
* Delete a Q of YANG statement node
*
* INPUTS:
*    que == Q of yang_stmt_t node to delete
*
*********************************************************************/
void
    yang_clean_stmtQ (dlq_hdr_t *que)
{
    yang_stmt_t *stmt;

#ifdef DEBUG
    if (!que) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif
    while (!dlq_empty(que)) {
	stmt = (yang_stmt_t *)dlq_deque(que);
	yang_free_stmt(stmt);
    }

}  /* yang_clean_stmtQ */


/********************************************************************
* FUNCTION yang_validate_date_string
* 
* Validate a YANG date string for a revision entry
*
* Expected format is:
*
*    YYYY-MM-DD
*
* Error messages are printed by this function
*
* INPUTS:
*    tkc == token chain in progress
*    mod == module in progress
*    errtk == error token to use (may be NULL to use tkc->cur)
*    datestr == string to validate
*
* RETURNS:
*   status
*********************************************************************/
status_t
    yang_validate_date_string (tk_chain_t *tkc,
			      ncx_module_t *mod,
			      tk_token_t *errtk,
			      const xmlChar *datestr)
{

#define DATE_STR_LEN 10

    tk_token_t     *useerr, *savetk;
    status_t        res, retres;
    uint32          len, i;
    int             ret;
    ncx_num_t       num;
    xmlChar         numbuff[NCX_MAX_NUMLEN];
    xmlChar         curdate[16];

#ifdef DEBUG
    if (!tkc || !mod || !datestr) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    retres = NO_ERR;
    len = xml_strlen(datestr);
    useerr = (errtk) ? errtk : tkc->cur;
    savetk = tkc->cur;
    tkc->cur = useerr;
    tstamp_date(curdate);

    /* validate the length */
    if (len != DATE_STR_LEN) {
	retres = ERR_NCX_INVALID_VALUE;
	log_error("\nError: Invalid date string length (%u)", len);
	ncx_print_errormsg(tkc, mod, retres);
    }

    /* validate the year field */
    if (retres == NO_ERR) {
	for (i=0; i<4; i++) {
	    numbuff[i] = datestr[i];
	}
	numbuff[4] = 0;
	res = ncx_decode_num(numbuff, NCX_BT_UINT32, &num);
	if (res != NO_ERR) {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid year string (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, retres);
	} else if (num.u < 1970) {
	    log_warn("\nWarning: Invalid revision year (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, ERR_NCX_DATE_PAST);
	} 
    }

    /* validate the first separator */
    if (retres == NO_ERR) {
	if (datestr[4] != '-') {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid date string separator (%c)",
		      datestr[4]);
	    ncx_print_errormsg(tkc, mod, retres);
	} 
    }

    /* validate the month field */
    if (retres == NO_ERR) {
	numbuff[0] = datestr[5];
	numbuff[1] = datestr[6];
	numbuff[2] = 0;
	res = ncx_decode_num(numbuff, NCX_BT_UINT32, &num);
	if (res != NO_ERR) {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid month string (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, retres);
	} else if (num.u < 1 || num.u > 12) {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid month string (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, retres);
	} 
    }

    /* validate the last separator */
    if (retres == NO_ERR) {
	if (datestr[7] != '-') {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid date string separator (%c)",
		      datestr[7]);
	    ncx_print_errormsg(tkc, mod, retres);
	} 
    }

    /* validate the day field */
    if (retres == NO_ERR) {
	numbuff[0] = datestr[8];
	numbuff[1] = datestr[9];
	numbuff[2] = 0;
	res = ncx_decode_num(numbuff, NCX_BT_UINT32, &num);
	if (res != NO_ERR) {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid day string (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, retres);
	} else if (num.u < 1 || num.u > 31) {
	    retres = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Invalid day string (%s)", numbuff);
	    ncx_print_errormsg(tkc, mod, retres);
	}
    }

    /* check the date against the future */
    if (retres == NO_ERR) {
	ret = xml_strcmp(curdate, datestr);
	if (ret < 0) {
	    log_warn("\nWarning: Revision date in the future (%s)",
		     datestr);
	    ncx_print_errormsg(tkc, mod, ERR_NCX_DATE_FUTURE);
	}
    }

    tkc->cur = savetk;
    return retres;

}  /* yang_validate_date_string */


/********************************************************************
* FUNCTION yang_skip_statement
* 
* Skip past the current invalid statement, starting at
* an invalid keyword
*
* INPUTS:
*    tkc == token chain in progress
*    mod == module in progress
*********************************************************************/
void
    yang_skip_statement (tk_chain_t *tkc,
			 ncx_module_t *mod)
{
    uint32    bracecnt;
    status_t  res;

#ifdef DEBUG
    if (!tkc || !tkc->cur) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    res = NO_ERR;
    bracecnt = 0;

    while (res==NO_ERR) {

	/* current should be value string or stmtend */
	switch (TK_CUR_TYP(tkc)) {
	case TK_TT_NONE:
	    return;
	case TK_TT_SEMICOL:
	    if (!bracecnt) {
		return;
	    }
	    break;
	case TK_TT_LBRACE:
	    bracecnt++;
	    break;
	case TK_TT_RBRACE:
	    if (bracecnt) {
		bracecnt--;
		if (!bracecnt) {
		    return;
		}
	    } else {
		return;
	    }
	default:
	    ;
	}

	res = TK_ADV(tkc);
	if (res != NO_ERR) {
	    ncx_print_errormsg(tkc, mod, res);
	}
    }
    
}  /* yang_skip_statement */


/********************************************************************
* FUNCTION yang_top_keyword
* 
* Check if the string is a top-level YANG keyword
*
* INPUTS:
*    keyword == string to check
*
* RETURNS:
*   TRUE if a top-level YANG keyword, FALSE otherwise
*********************************************************************/
boolean
    yang_top_keyword (const xmlChar *keyword)
{
#ifdef DEBUG
    if (!keyword) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return FALSE;
    }
#endif

    if (!xml_strcmp(keyword, YANG_K_YANG_VERSION)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_NAMESPACE)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_PREFIX)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_BELONGS_TO)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_ORGANIZATION)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_CONTACT)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_DESCRIPTION)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_REFERENCE)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_IMPORT)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_INCLUDE)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_REVISION)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_TYPEDEF)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_GROUPING)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_ANYXML)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_CONTAINER)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_LEAF)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_LEAF_LIST)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_LIST)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_CHOICE)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_USES)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_AUGMENT)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_RPC)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_NOTIFICATION)) {
	return TRUE;
    }
    if (!xml_strcmp(keyword, YANG_K_EXTENSION)) {
	return TRUE;
    }
    return FALSE;

}  /* yang_top_keyword */


/********************************************************************
* FUNCTION yang_new_import_ptr
* 
* Create a new YANG import pointer node
*
* INPUTS:
*   import name to set
*
* RETURNS:
*   pointer to new and initialized struct, NULL if memory error
*********************************************************************/
yang_import_ptr_t *
    yang_new_import_ptr (const xmlChar *modname,
			 const xmlChar *modprefix)
{
    yang_import_ptr_t *impptr;

    impptr = m__getObj(yang_import_ptr_t);
    if (!impptr) {
	return NULL;
    }
    memset(impptr, 0x0, sizeof(yang_import_ptr_t));
    impptr->modprefix = xml_strdup(modprefix);
    if (!impptr->modprefix) {
	m__free(impptr);
	return NULL;
    }
    impptr->modname = xml_strdup(modname);
    if (!impptr->modname) {
	m__free(impptr->modprefix);
	m__free(impptr);
	return NULL;
    }

    return impptr;

} /* yang_new_import_ptr */


/********************************************************************
* FUNCTION yang_free_impotr_ptr
* 
* Delete a YANG import pointer node
*
* INPUTS:
*  impptr == import pointer node to delete
*
*********************************************************************/
void
    yang_free_import_ptr (yang_import_ptr_t *impptr)
{
#ifdef DEBUG
    if (!impptr) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (impptr->modname) {
	m__free(impptr->modname);
    }
    if (impptr->modprefix) {
	m__free(impptr->modprefix);
    }

    m__free(impptr);

}  /* yang_free_import_ptr */


/********************************************************************
* FUNCTION yang_clean_import_ptrQ
* 
* Delete all the node entries in a YANG import pointer node Q
*
* INPUTS:
*    que == Q of yang_import_ptr_t to clean
*
*********************************************************************/
void
    yang_clean_import_ptrQ (dlq_hdr_t *que)
{
    yang_import_ptr_t *impptr;

#ifdef DEBUG
    if (!que) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    while (!dlq_empty(que)) {
	impptr = (yang_import_ptr_t *)dlq_deque(que);
	yang_free_import_ptr(impptr);
    }

}  /* yang_clean_import_ptrQ */


/********************************************************************
* FUNCTION yang_find_import_ptr
* 
* Find a YANG import pointer node in the specified Q
*
* INPUTS:
*    que == Q of yang_import_ptr_t to check
*    name == module name to find
*
* RETURNS:
*    pointer to found node, NULL if not found
*********************************************************************/
yang_import_ptr_t *
    yang_find_import_ptr (dlq_hdr_t *que,
			  const xmlChar *name)
{
    yang_import_ptr_t *impptr;

#ifdef DEBUG
    if (!que || !name) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    for (impptr = (yang_import_ptr_t *)dlq_firstEntry(que);
	 impptr != NULL;
	 impptr = (yang_import_ptr_t *)dlq_nextEntry(impptr)) {

	if (!xml_strcmp(impptr->modname, name)) {
	    return impptr;
	}
    }
    return NULL;

}  /* yang_find_import_ptr */


/* END file yang.c */
