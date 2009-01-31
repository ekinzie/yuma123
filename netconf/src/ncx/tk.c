/*  FILE: tk.c


    NCX Token Manager : Low level token management functions

     - Tokenization of NCX modules
     - Tokenization of text config files

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
12nov05      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include <xmlstring.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
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

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

#ifndef _H_tk
#include "tk.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* #define TK_DEBUG 1 */
/* #define TK_RDLN_DEBUG 1 */

#define FL_YANG   bit0                /* source is TK_SOURCE_YANG */
#define FL_CONF   bit1                /* source is TK_SOURCE_CONF */
#define FL_XPATH  bit2               /* source is TK_SOURCE_XPATH */
#define FL_REDO   bit3                /* source is TK_SOURCE_REDO */

#define FL_NCX    bit4                /* place-holder; deprecated */

#define FL_ALL    (FL_YANG|FL_CONF|FL_XPATH|FL_REDO)

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* One quick entry token lookup */
typedef struct tk_ent_t_ {
    tk_type_t       ttyp;
    uint32          tlen;
    const char     *tid;
    const char     *tname;
    uint32          flags;
} tk_ent_t;

/* One quick entry built-in type name lookup */
typedef struct tk_btyp_t_ {
    ncx_btype_t     btyp;
    uint32          blen;
    const xmlChar  *bid;
    uint32          flags;
} tk_btyp_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S			    *
*                                                                   *
*********************************************************************/

/* lookup list of all the non-textual/numeric tokens 
 *  !! MAKE SURE THIS TABLE ORDER MATCHES THE ORDER OF tk_type_t 
 * The lengths in this table do not include the terminating zero
 */
static tk_ent_t tlist [] = {
    /* align array with token enumeration values, skip 0 */
    { TK_TT_NONE, 0, NULL, "none", 0 }, 
    
    /* ONE CHAR TOKENS */
    { TK_TT_LBRACE, 1, "{", "left brace", FL_ALL },
    { TK_TT_RBRACE, 1, "}", "right brace", FL_ALL },
    { TK_TT_SEMICOL, 1, ";", "semicolon", FL_YANG },
    { TK_TT_LPAREN, 1, "(", "left paren", FL_XPATH },
    { TK_TT_RPAREN, 1, ")", "right paren", FL_XPATH },
    { TK_TT_LBRACK, 1, "[", "left bracket", FL_XPATH },
    { TK_TT_RBRACK, 1, "]", "right bracket", FL_XPATH },
    { TK_TT_COMMA, 1, ",", "comma", FL_XPATH },
    { TK_TT_EQUAL, 1, "=", "equals sign", FL_XPATH },
    { TK_TT_BAR, 1, "|", "vertical bar", (FL_YANG|FL_XPATH|FL_REDO) },
    { TK_TT_STAR, 1, "*", "asterisk", FL_XPATH },
    { TK_TT_ATSIGN, 1, "@", "at sign", FL_XPATH },
    { TK_TT_PLUS, 1, "+", "plus sign", (FL_YANG|FL_XPATH|FL_REDO) },
    { TK_TT_COLON, 1, ":", "colon", FL_XPATH },
    { TK_TT_PERIOD, 1, ".", "period", FL_XPATH },
    { TK_TT_FSLASH, 1, "/", "forward slash", FL_XPATH },
    { TK_TT_MINUS, 1, "-", "minus", FL_XPATH },
    { TK_TT_LT, 1, "<", "less than", FL_XPATH },
    { TK_TT_GT, 1, ">", "greater than", FL_XPATH },

    /* TWO CHAR TOKENS */
    { TK_TT_RANGESEP, 2, "..", "range seperator", 
      (FL_REDO|FL_XPATH) },
    { TK_TT_DBLCOLON, 2, "::", "double colon", FL_XPATH },
    { TK_TT_DBLFSLASH, 2, "//", "double foward slash", FL_XPATH },
    { TK_TT_NOTEQUAL, 2, "!=", "not equal sign", FL_XPATH },
    { TK_TT_LEQUAL, 2, "<=", "less than or equal", FL_XPATH },
    { TK_TT_GEQUAL, 2, ">=", "greater than or equal", FL_XPATH },

    /* string classification tokens, all here for name and placeholder */
    { TK_TT_STRING, 0, "", "unquoted string", FL_ALL},
    { TK_TT_SSTRING, 0, "", "scoped ID string", FL_ALL},
    { TK_TT_TSTRING, 0, "", "token string", FL_ALL},
    { TK_TT_MSTRING, 0, "", "prefix qualified ID string", FL_ALL},
    { TK_TT_MSSTRING, 0, "", "prefix qualified scoped ID string", FL_ALL},
    { TK_TT_QSTRING, 0, "", "double quoted string", FL_ALL},
    { TK_TT_SQSTRING, 0, "", "single quoted string", FL_ALL},

    /* variable binding '$NCName' or '$QName' */
    { TK_TT_VARBIND, 0, "", "varbind", FL_XPATH },
    { TK_TT_QVARBIND, 0, "", "qvarbind", FL_XPATH },

    /* XPath NameTest, form 2: 'NCName:*' */
    { TK_TT_NCNAME_STAR, 0, "", "NCName:*", FL_XPATH },

    /* number classification tokens */
    { TK_TT_DNUM, 0, "", "decimal number", FL_ALL},
    { TK_TT_HNUM, 0, "", "hex number", FL_ALL},
    { TK_TT_RNUM, 0, "", "real number", FL_ALL},

    /* newline for conf file parsing only */
    { TK_TT_NEWLINE, 0, "", "newline", FL_CONF}, 

    /* EO Array marker */
    { TK_TT_NONE, 0, NULL, "none", 0}
};


/* lookup list of NCX builtin type names 
 * 
 *  !!! ORDER IS HARD-WIRED TO PREVENT RUNTIME INIT FUNCTION !!!
 *  !!! NEED TO CHANGE IF NCX DATA TYPE NAMES ARE CHANGED !!!
 *  !!! NEED TO KEEP ARRAY POSITION AND NCX_BT_ VALUE THE SAME !!!
 *
 * In YANG, the NCX_BT_ANY is not represented with a leaf
 * but rather a separate 'anyxml' construct.  Even so, it is
 * stored intervally as a leaf with type NCX_BT_ANY
 *
 * hack: string lengths are stored so only string compares
 * with the correct number of chars are actually made
 * when parsing the YANG type statements
 *
 * TBD: change to runtime strlen evaluation 
 * instead of hardwire lengths.
 */
static tk_btyp_t blist [] = {
    { NCX_BT_NONE, 4, (const xmlChar *)"NONE", 0 },
    { NCX_BT_ANY, 6, NCX_EL_ANYXML, 0 },
    { NCX_BT_BITS, 4, NCX_EL_BITS, FL_YANG },
    { NCX_BT_ENUM, 11, NCX_EL_ENUMERATION, FL_YANG },
    { NCX_BT_EMPTY, 5, NCX_EL_EMPTY, FL_YANG },
    { NCX_BT_BOOLEAN, 7, NCX_EL_BOOLEAN, FL_YANG },
    { NCX_BT_INT8, 4, NCX_EL_INT8, FL_YANG },
    { NCX_BT_INT16, 5, NCX_EL_INT16, FL_YANG },
    { NCX_BT_INT32, 5, NCX_EL_INT32, FL_YANG },
    { NCX_BT_INT64, 5, NCX_EL_INT64, FL_YANG },
    { NCX_BT_UINT8, 5, NCX_EL_UINT8, FL_YANG },
    { NCX_BT_UINT16, 6, NCX_EL_UINT16, FL_YANG },
    { NCX_BT_UINT32, 6, NCX_EL_UINT32, FL_YANG },
    { NCX_BT_UINT64, 6, NCX_EL_UINT64, FL_YANG },
    { NCX_BT_FLOAT32, 7, NCX_EL_FLOAT32, FL_YANG },
    { NCX_BT_FLOAT64, 7, NCX_EL_FLOAT64, FL_YANG },
    { NCX_BT_STRING, 6, NCX_EL_STRING, FL_YANG },
    { NCX_BT_BINARY, 6, NCX_EL_BINARY, FL_YANG },
    { NCX_BT_INSTANCE_ID, 19, NCX_EL_INSTANCE_IDENTIFIER, FL_YANG },
    { NCX_BT_UNION, 5, NCX_EL_UNION, FL_YANG },
    { NCX_BT_LEAFREF, 7, NCX_EL_LEAFREF, FL_YANG },
    { NCX_BT_IDREF, 11, NCX_EL_IDENTITYREF, FL_YANG },
    { NCX_BT_SLIST, 5, NCX_EL_SLIST, 0 },
    { NCX_BT_CONTAINER, 9, NCX_EL_CONTAINER, 0 },
    { NCX_BT_CHOICE, 6, NCX_EL_CHOICE, 0 },
    { NCX_BT_CASE, 4, NCX_EL_CASE, 0 },
    { NCX_BT_LIST, 4, NCX_EL_LIST, 0 },
    { NCX_BT_NONE, 4, (const xmlChar *)"NONE", 0 }
};


/********************************************************************
* FUNCTION new_token
* 
* Allocatate a new token with a value string that will be copied
*
* INPUTS:
*  ttyp == token type
*  tval == token value or NULL if not used
*  tlen == token value length if used; Ignored if tval == NULL
*
* RETURNS:
*   new token or NULL if some error
*********************************************************************/
static tk_token_t *
    new_token (tk_type_t ttyp, 
	       const xmlChar *tval,
	       uint32  tlen)
{
    tk_token_t  *tk;

    tk = m__getObj(tk_token_t);
    if (!tk) {
        return NULL;
    }
    memset(tk, 0x0, sizeof(tk_token_t));
    tk->typ = ttyp;
    if (tval) {
        tk->len = tlen;
        tk->val = xml_strndup(tval, tlen);
        if (!tk->val) {
            m__free(tk);
            return NULL;
        }
    }

#ifdef TK_DEBUG
    log_debug3("\ntk: new token (%s) ", tk_get_token_name(ttyp));
    if (LOGDEBUG3 && tval) {
        while (tlen--) {
            log_debug3("%c", *tval++);
        }
    } else {
        log_debug3("%s", tk_get_token_sym(ttyp));
    }
#endif
            
    return tk;
    
} /* new_token */


/********************************************************************
* FUNCTION new_mtoken
* 
* Allocatate a new token with a value string that will be
* consumed and freed later, and not copied 
*
* INPUTS:
*  ttyp == token type
*  tval == token value or NULL if not used
*
* RETURNS:
*   new token or NULL if some error
*********************************************************************/
static tk_token_t *
    new_mtoken (tk_type_t ttyp, 
	       xmlChar *tval)
{
    tk_token_t  *tk;

    tk = m__getObj(tk_token_t);
    if (!tk) {
        return NULL;
    }
    memset(tk, 0x0, sizeof(tk_token_t));
    tk->typ = ttyp;
    if (tval) {
        tk->len = xml_strlen(tval);
        tk->val = tval;
    }

#ifdef TK_DEBUG
    log_debug3("\ntk: new mtoken (%s) ", tk_get_token_name(ttyp));
    if (LOGDEBUG3 && tval) {
        while (tlen--) {
            log_debug3("%c", *tval++);
        }
    } else {
        log_debug3("%s", tk_get_token_sym(ttyp));
    }
#endif
            
    return tk;
    
} /* new_mtoken */


/********************************************************************
* FUNCTION free_token
* 
* Cleanup and deallocate a tk_token_t 
*
* RETURNS:
*  none
*********************************************************************/
static void 
    free_token (tk_token_t *tk)
{
#ifdef TK_DEBUG
    log_debug3("\ntk: free_token: (%s)", tk_get_token_name(tk->typ));
    if (tk->val) {
        log_debug3(" val=(%s) ", tk->val);
    }
#endif

    if (tk->mod) {
        m__free(tk->mod);
    }
    if (tk->val) {
        m__free(tk->val);
    }
    m__free(tk);

} /* free_token */


/********************************************************************
* FUNCTION new_token_wmod
* 
* Allocatate a new identifier token with a <name> qualifier
* <name)> is a module name in NCX
*         is a prefix name in YANG
*
* INPUTS:
*  ttyp == token type
*  mod == module name string, not z-terminated
*  modlen == tmodule name length
*  tval == token value
*  tlen == token value length
*
* RETURNS:
*   new token or NULL if some error
*********************************************************************/
static tk_token_t *
    new_token_wmod (tk_type_t ttyp, 
		       const xmlChar *mod,
		       uint32 modlen,
		       const xmlChar *tval, 
		       uint32 tlen)
{
    tk_token_t  *ret;

    ret = new_token(ttyp, tval, tlen);
    if (ret) {
        ret->modlen = modlen;
        ret->mod = xml_strndup(mod, modlen);
        if (!ret->mod) {
            free_token(ret);
            return NULL;
        }
    }

#ifdef TK_DEBUG
    log_debug("  mod: ");
    while (modlen--) {
        log_debug("%c", *mod++);
    }
#endif

    return ret;
}  /* new_token_wmod */


/********************************************************************
* FUNCTION add_new_token
* 
* Allocatate a new token with the specified type
* Use the token chain directly
*
*    tkc->bptr == first char in the value string to save
* 
*    Add the token to the chain if no error
*    Update the buffer pointer to 'str' after token is added OK
*
* INPUTS:
*    tkc == token chain
*    ttyp == token type to create
*    str == pointer to the first char after the last char in the value
*           value includes chars from bptr to str-1
*        == NULL if no value for this token
*    startpos == line position where this token started
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    add_new_token (tk_chain_t *tkc,
		   tk_type_t ttyp,
		   const xmlChar *str,
		   uint32 startpos)
{
    tk_token_t  *tk;
    uint32       total;

    total = (str) ? (uint32)(str - tkc->bptr) : 0;

    if (total > NCX_MAX_STRLEN) {
	return ERR_NCX_LEN_EXCEEDED;
    } else if (total == 0) {
	/* zero length value strings are allowed */
	tk = new_token(ttyp, NULL, 0);
    } else {
	/* normal case string -- non-zero length */
	tk = new_token(ttyp, tkc->bptr, total);
    }
    if (!tk) {
	return ERR_INTERNAL_MEM;
    }
    tk->linenum = tkc->linenum;
    tk->linepos = startpos;
    dlq_enque(tk, &tkc->tkQ);

    return NO_ERR;

}  /* add_new_token */


/********************************************************************
* FUNCTION add_new_qtoken
* 
* Allocatate a new TK_TT_QSTRING token
* Use the token chain directly
* Convert any escaped chars in the buffer first
* Adjust the leading and trailing whitespace around
* newline characters
*
*    tkc->bptr == first char in the value string to save
* 
*    Add the token to the chain if no error
*    Update the buffer pointer to 'str' after token is added OK
*
* INPUTS:
*    tkc == token chain to add new token to
*    isdouble == TRUE for TK_TT_QSTRING
*             == FALSE for TK_TT_SQSTRING
*    tkbuff == token input buffer to use
*    endstr == pointer to the first char after the last char in the value
*              value includes chars from bptr to str-1
*           == NULL if no value for this token
*    startline == line number where this token started
*    startpos == line position where this token started
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    add_new_qtoken (tk_chain_t *tkc,
		    boolean isdouble,
		    xmlChar *tkbuff,
		    const xmlChar *endstr,
		    uint32 startline,
		    uint32 startpos)
{
    tk_token_t    *tk;
    xmlChar       *buff, *outstr, *teststr;
    const xmlChar *instr, *spstr;
    uint32         total, linepos, cnt, tcnt, scnt, chcnt;
    boolean        done;

    buff = NULL;
    total = (endstr) ? (uint32)(endstr - tkbuff) : 0;

    if (total > NCX_MAX_STRLEN) {
	return ERR_NCX_LEN_EXCEEDED;
    } else if (total == 0) {
	/* zero length value strings are allowed */
	tk = new_token((isdouble) ? TK_TT_QSTRING : TK_TT_SQSTRING,
		       NULL, 0);
    } else if (!isdouble) {
	/* single quote string */
	tk = new_token(TK_TT_SQSTRING, tkbuff, total);
    } else {
	/* double quote normal case -- non-zero length QSTRING
	 * fill the buffer, while converting escaped chars
	 */
	buff = (xmlChar *)m__getMem(total+1);
	if (!buff) {
	    return ERR_INTERNAL_MEM;
	}

	instr = tkbuff;  /* points to next char to be read */
	outstr = buff;   /* points to next char to be filled */

	while (instr < endstr) {

	    /* translate escape char or copy regular char */
	    if (*instr == '\\') {
		switch (instr[1]) {
		case 'n':
		    *outstr++ = '\n';
		    break;
		case 't':
		    *outstr++ = '\t';
		    break;
		case '"':
		    *outstr++ = '"';
		    break;
		case '\\':
		    *outstr++ = '\\';
		    break;
		case '\0':
		    /* let the next loop exit on EO-buffer */
		    break;
		default:
		    /* pass through the escape sequence */
		    *outstr++ = '\\';
		    *outstr++ = instr[1];
		}

		/* adjust the in pointer if not EO-buffer */
		if (instr[1]) {
		    instr += 2;
		} else {
		    instr++;
		}
	    } else {
		*outstr++ = *instr++;
	    }

	    /* check if last char written was a newline 
	     * DO NOT ADJUST XPATH STRINGS
	     */
	    if (*(outstr-1) == '\n' && (tkc->source != TK_SOURCE_XPATH)) {
		/* trim any trailing whitespace */
		if (outstr-2 >= tkbuff) {
		    teststr = outstr-2;
		    while ((*teststr != '\n') && 
			   xml_isspace(*teststr) && (teststr > tkbuff)) {
			teststr--;
		    }
		    teststr[1] = '\n';
		    outstr = &teststr[2];
		}

		/* find end of leading whitespace */
		spstr = instr;
		linepos = 0;
		chcnt = 0;
		done = FALSE;
		while (!done) {
		    if (*spstr == ' ') {
			chcnt++;
			linepos++;
			spstr++;
		    } else if (*spstr == '\t') {
			chcnt++;
			linepos += NCX_TABSIZE;
			spstr++;
		    } else {
			done = TRUE;
		    }
		}

		/* linepos is the indent total for the next line
		 * subtract the start position and indent the rest
		 */
		if (linepos > startpos) {
		    cnt = linepos - startpos;
		    tcnt = cnt / NCX_TABSIZE;
		    scnt = cnt % NCX_TABSIZE;

		    /* make sure not to write more chars than
		     * were read.  E.g., do not replace 2 tabs
                     * with 7 spaces and over flow the buffer
		     */
		    if (tcnt+scnt <= chcnt) {
			/* do correct indent count */
			while (tcnt) {
			    *outstr++ = '\t';
			    tcnt--;
			}
			while (scnt) {
			    *outstr++ = ' ';
			    scnt--;
			}
		    } else {
			/* indent as many chars as possible */
			tcnt = min(tcnt, chcnt);
			while (tcnt) {
			    *outstr++ = '\t';
			    tcnt--;
			    chcnt--;
			}
			scnt = min(scnt, chcnt);
			while (scnt) {
			    *outstr++ = ' ';
			    scnt--;
			}
		    }
		}

		/* skip over whitespace that was just processed */
		instr = spstr;
	    }
	}

	/* finish the string */
	*outstr = 0;
	tk = new_mtoken(TK_TT_QSTRING, buff);
    }

    if (!tk) {
	if (buff) {
	    m__free(buff);
	}
	return ERR_INTERNAL_MEM;
    }

    tk->linenum = startline;
    tk->linepos = startpos;
    dlq_enque(tk, &tkc->tkQ);

    return NO_ERR;

}  /* add_new_qtoken */


/********************************************************************
* FUNCTION yang_hack_char_test
* 
* Check a char to see if it could be a YANG char that would not
* be part of an unquoted string
*
* INPUTS:
*  str == pointer to char to check
*
* RETURNS:
*   TRUE if char is a break char
*   FALSE if char could be in an unquopted string
*********************************************************************/
static boolean
    yang_hack_char_test (const xmlChar *str)
{
    if (xml_isspace(*str)) {
	return TRUE;
    } else if (*str == '"' || *str =='\'') {
	return TRUE;
    } else if (!*str || *str == '\n') {
	return TRUE;
    }
    return FALSE;

}  /* yang_hack_char_test */


/********************************************************************
* FUNCTION get_token_id
* 
* Check if the spceified string is a NCX non-string token
* Checks for 1-char and 2-char tokens only!!
*
* INPUTS:
*   buff == token string to check -- NOT ZERO-TERMINATED
*   len == length of string to check
*   srctyp == parsing source
*    (TK_SOURCE_CONF, TK_SOURCE_YANG, TK_SOURCE_CONF, TK_SOURCE_REDO)
*
* RETURNS:
*   token type found or TK_TT_NONE if no match
*********************************************************************/
static tk_type_t 
    get_token_id (const xmlChar *buff, 
		  uint32 len,
		  tk_source_t srctyp)
{
    tk_type_t t;
    uint32    flags;

    /* determine which subset of the token list will be checked */
    switch (srctyp) {
    case TK_SOURCE_CONF:
	flags = FL_CONF;
	break;
    case TK_SOURCE_YANG:
	flags = FL_YANG;
	break;
    case TK_SOURCE_XPATH:
	flags = FL_XPATH;
	break;
    case TK_SOURCE_REDO:
	flags = FL_REDO;
	break;
    default:
	SET_ERROR(ERR_INTERNAL_VAL);
	return TK_TT_NONE;
    }

    /* look in the tlist for the specified token 
     * This is an optimized hack to save time, instead
     * of a complete search through all data types
     *
     * !!! MAKE SURE THE FOR LOOPS BELOW ARE UPDATED
     * !!! IF THE tk_token_t ENUMERATION IS CHANGED
     * 
     * Only tokens for the specified source language
     * and the specified length will be returned
     */
    if (len==1) {
        for (t=TK_TT_LBRACE; t <= TK_TT_GT; t++) {
            if (*buff == *tlist[t].tid && (tlist[t].flags & flags)) {
                return tlist[t].ttyp;
            }
        }
    } else if (len==2) {
        for (t=TK_TT_RANGESEP; t <= TK_TT_GEQUAL; t++) {
            if (!xml_strncmp(buff, (const xmlChar *)tlist[t].tid, 2) &&
		(tlist[t].flags & flags)) {
		return tlist[t].ttyp;
            }
        }
    }
    return TK_TT_NONE;

} /* get_token_id */


/********************************************************************
* FUNCTION tokenize_qstring
* 
* Handle a double-quoted string ("string")
*
* Converts escaped chars during this first pass processing
*
* Does not realign whitespace in this function
* That is done when strings are concatenated
* with finish_qstrings
*
* INPUTS:
*   tkc == token chain 
*
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_qstring (tk_chain_t *tkc)
{
    xmlChar      *str, *tempbuff, *outstr;
    uint32        total, startline, startpos, linelen;
    boolean       done, done2;
    status_t      res;

    startline = tkc->linenum;
    startpos = tkc->linepos;

    /* the bptr is pointing at the quote char which
     * indicates the start of a quoted string
     * find the end of the string of end of the line/buffer
     * start bptr after the quote since that is not saved
     * as part of the content, and doesn't count againt maxlen
     *
     * This first loop does not process chars yet
     * because most quoted strings do not contain the
     * escaped chars, and an extra copy is not needed if this is
     * a simple quoted string on a single line
     */
    str = ++tkc->bptr;      /* skip over escaped double quotes */
    done = FALSE;    
    while (!done) {
	if (!*str) {
	    /* End of buffer */
	    done = TRUE;
	} else if (*str == NCX_QSTRING_CH && (*(str-1) != '\\')) {
	    /* ending double quote */
	    done = TRUE;
	} else {
	    if (*str == '\\') {
		if (str[1]) {
		    str++;
		}
		if (*str == 'n') {
		    tkc->linepos = 1;
		} else if (*str =='t') {
		    tkc->linepos += NCX_TABSIZE;
		} else {
		    tkc->linepos++;
		}
	    } else {
		if (*str == '\n') {
		    tkc->linepos = 1;
		} else if (*str =='\t') {
		    tkc->linepos += NCX_TABSIZE;
		} else {
		    tkc->linepos++;
		}
	    }
	    str++;
	}
    }

    total = (uint32)(str - tkc->bptr);

    if (*str == NCX_QSTRING_CH) {
        /* easy case, a quoted string on 1 line */
	res = add_new_qtoken(tkc, TRUE, tkc->bptr, str, 
				 startline, startpos);
        tkc->bptr = str+1;
        return res;
    }

    /* else we reached the end of the buffer without
     * finding the QSTRING_CH. If input from buffer this is 
     * an error
     */
    if (!tkc->filename) {
        return ERR_NCX_UNENDED_QSTRING;
    }

    /* FILE input, keep reading lines looking for QUOTE_CH 
     * need to get a temp buffer to store all the lines 
     */
    tempbuff = (xmlChar *)m__getMem(NCX_MAX_Q_STRLEN+1);
    if (!tempbuff) {
        return ERR_INTERNAL_MEM;
    }

    outstr = tempbuff;

    /* check total bytes parsed, and copy into temp buff if any */
    if (total) {
        outstr += xml_strcpy(outstr, tkc->bptr);
    } else {
	outstr[0] = 0;
    }

    /* ELSE real special case, start of QSTRING last char in the line 
     * This should not happen because a newline should at 
     * least follow the QSTRING char 
     */

    /* keep saving lines in tempbuff until the QSTRING_CH is found */
    done = FALSE;
    while (!done) {
        if (!fgets((char *)tkc->buff, TK_BUFF_SIZE, tkc->fp)) {
            /* read line failed -- assume EOF */
	    m__free(tempbuff);
            return ERR_NCX_UNENDED_QSTRING;
        } else {
	    tkc->linenum++;
	    tkc->linepos = 1;
	    tkc->bptr = tkc->buff;
	}

#ifdef TK_RDLN_DEBUG
        if (xml_strlen(tkc->buff) < 128) {
            log_debug3("\nNCX Parse: read line (%s)", tkc->buff);
        } else {
            log_debug3("\nNCX Parse: read line len  (%u)", 
		      xml_strlen(tkc->buff));
        }
#endif

	/* look for ending quote on this line */
        str = tkc->bptr;
	done2 = FALSE;
	while (!done2) {
	    if (!*str) {
		done2 = TRUE;
	    } else if (*str == NCX_QSTRING_CH && (*(str-1) != '\\')) {
		done2 = TRUE;
	    } else {
		str++;
	    }
	}

	linelen = (uint32)(str-tkc->bptr);

        if (*str) {
	    tkc->linepos = linelen+1;
            done = TRUE;   /* stopped on a double quote */
	}

	if (linelen + total < NCX_MAX_Q_STRLEN) {
	    /* copy this line to tempbuff */
	    outstr += xml_strncpy(outstr, tkc->bptr, linelen);
	    total += linelen;
	    tkc->bptr = str+1;
        } else {
	    /* would be a buffer overflow */
	    m__free(tempbuff);
	    return ERR_NCX_LEN_EXCEEDED;
	}
    }

    res = add_new_qtoken(tkc, TRUE, tempbuff, 
			 outstr, startline, startpos);
    m__free(tempbuff);
    return res;

}  /* tokenize_qstring */


/********************************************************************
* FUNCTION tokenize_sqstring
* 
* Handle a single-quoted string ('string')
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_sqstring (tk_chain_t *tkc)
{
    xmlChar      *str, *tempbuff, *outstr;
    uint32        total, startline, startpos, linelen;
    status_t      res;
    boolean       done;

    startline = tkc->linenum;
    startpos = tkc->linepos;
    
    /* the bptr is pointing at the quote char which
     * indicates the start of a quoted string
     * find the end of the string of end of the line/buffer
     * start bptr after the quote since that is not saved
     * as part of the content, and doesn't count againt maxlen
     */
    str = ++tkc->bptr;
    while (*str && (*str != NCX_SQSTRING_CH)) {
        str++;
    }

    total = (uint32)(str - tkc->bptr);

    if (*str == NCX_SQSTRING_CH) {
        /* easy case, a quoted string on 1 line; */
	res = add_new_token(tkc, TK_TT_SQSTRING, str, startpos);
        tkc->bptr = str+1;
	tkc->linepos += total+2;
        return NO_ERR;
    }

    /* else we reached the end of the buffer without
     * finding the SQSTRING_CH. If input from buffer this is 
     * an error
     */
    if (!tkc->filename) {
        return ERR_NCX_UNENDED_QSTRING;
    }

    /* FILE input, keep reading lines looking for QUOTE_CH 
     * need to get a temp buffer to store all the lines 
     */
    tempbuff = (xmlChar *)m__getMem(NCX_MAX_Q_STRLEN+1);
    if (!tempbuff) {
        return ERR_INTERNAL_MEM;
    }

    outstr = tempbuff;

    /* check total bytes parsed, and copy into temp buff if any */
    total = (uint32)(str - tkc->bptr);
    if (total) {
        outstr += xml_strcpy(outstr, tkc->bptr);
    } else {
	outstr[0] = 0;
    }

    /* keep saving lines in tempbuff until the QSTRING_CH is found */
    done = FALSE;
    while (!done) {
        if (!fgets((char *)tkc->buff, TK_BUFF_SIZE, tkc->fp)) {
            /* read line failed -- assume EOF */
	    m__free(tempbuff);
            return ERR_NCX_UNENDED_QSTRING;
        } else {
	    tkc->linenum++;
	    tkc->linepos = 1;
	    tkc->bptr = tkc->buff;
	}

#ifdef TK_RDLN_DEBUG
        if (xml_strlen(tkc->buff) < 128) {
            log_debug3("\nNCX Parse: read line (%s)", tkc->buff);
        } else {
            log_debug3("\nNCX Parse: read line len  (%u)", 
		      xml_strlen(tkc->buff));
        }
#endif
        str = tkc->bptr;
        while (*str && (*str != NCX_SQSTRING_CH)) {
            str++;
        }

	linelen = (uint32)(str-tkc->bptr);
	
        if (*str) {
	    tkc->bptr = str+1;
	    tkc->linepos = linelen+1;
            done = TRUE;   /* stopped on a single quote */
        }

	if (linelen + total < NCX_MAX_Q_STRLEN) {
	    outstr += xml_strncpy(outstr, tkc->bptr, linelen);
	    total += linelen;
	} else {
	    m__free(tempbuff);
	    return ERR_NCX_LEN_EXCEEDED;
        }
    }

    /* get a token and save the SQSTRING */
    res = add_new_qtoken(tkc, FALSE, tempbuff,
			 outstr, startline, startpos);
    m__free(tempbuff);
    return res;

}  /* tokenize_sqstring */


/********************************************************************
* FUNCTION skip_yang_cstring
* 
* Handle a YANG multi-line comment string   TK_TT_COMMENT
* Advance current buffer pointer past the comment
*
* INPUTS:
*   tkc == token chain 
*
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    skip_yang_cstring (tk_chain_t *tkc)
{
    xmlChar   *str;
    boolean    done;

    /* the bptr is pointing at the comment char which
     * indicates the start of a C style comment
     */
    tkc->linepos += 2;
    tkc->bptr += 2;

    /* look for star-slash '*' '/' EO comment sequence */
    str = tkc->bptr;
    while (*str && !(*str=='*' && str[1]=='/')) {
	if (*str == '\t') {
	    tkc->linepos += NCX_TABSIZE;
	} else {
	    tkc->linepos++;
	}
	str++;
    }

    if (*str) {
	/* simple case, stopped at end of 1 line comment */
	tkc->bptr = str+2;
	tkc->linepos += 2;
	return NO_ERR;
    }

    /* else stopped at end of buffer, get rest of multiline comment */
    if (!tkc->filename) {
        return ERR_NCX_UNENDED_COMMENT;
    }

    /* FILE input, keep reading lines looking for the
     * end of comment
     */
    done = FALSE;
    while (!done) {
        if (!fgets((char *)tkc->buff, TK_BUFF_SIZE, tkc->fp)) {
            /* read line failed -- assume EOF */
            return ERR_NCX_UNENDED_COMMENT;
        } else {
	    tkc->linenum++;
	    tkc->linepos = 1;
	    tkc->bptr = tkc->buff;
	}

#ifdef TK_RDLN_DEBUG
        if (xml_strlen(tkc->buff) < 128) {
            log_debug3("\nNCX Parse: read line (%s)", tkc->buff);
        } else {
            log_debug3("\nNCX Parse: read line len  (%u)", 
		      xml_strlen(tkc->buff));
        }
#endif

        str = tkc->bptr;
	while (*str && !(*str=='*' && str[1]=='/')) {
	    if (*str == '\t') {
		tkc->linepos += NCX_TABSIZE;
	    } else {
		tkc->linepos++;
	    }
            str++;
        }

        if (*str) {
	    tkc->linepos += 2;
	    tkc->bptr = str+2;
            done = TRUE;   /* stopped on an end of comment */
        }
    }

    return NO_ERR;

}  /* skip_yang_cstring */


/********************************************************************
* FUNCTION tokenize_number
* 
* Handle some sort of number string
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_number (tk_chain_t *tkc)
{
    xmlChar      *str;
    uint32        startpos, total;
    status_t      res;

    startpos = tkc->linepos;
    str = tkc->bptr;

    /* the bptr is pointing at the first number char which
     * is a valid (0 - 9) digit, or a + or - char
     * get the sign first
     */
    if (*str == '+' || *str == '-') {
	str++;
    }

    /* check if this is a hex number */
    if (*str == '0' && NCX_IS_HEX_CH(str[1])) {
	/* move the start of number portion */
	str += 2;
        while (isxdigit(*str)) {
            str++;
        }

	total = (uint32)(str - tkc->bptr);

        /* make sure we ended on a proper char and have a proper len */
        if (isalpha(*str) || str==tkc->bptr || total > NCX_MAX_HEXCHAR) {
            return ERR_NCX_INVALID_HEXNUM;
        }

	res = add_new_token(tkc, TK_TT_HNUM, str, startpos);
        tkc->bptr = str;
	tkc->linepos += total;
        return res;
    }

    /* else not a hex number so just find the end or a '.' */
    while (isdigit(*str)) {
        str++;
    }

    /* check if we stopped on a dot, indicating a real number 
     * make sure it's not 2 dots, which is the range separator
     */
    if (*str == '.' && str[1] != '.') {
        /* this is a real number, get the rest of the value */
        str++;     /* skip dot char */
        while (isdigit(*str)) {
            str++;
        }

	total = (uint32)(str - tkc->bptr);

        /* make sure we ended on a proper char and have a proper len
	 * This function does not support number entry in
	 * scientific notation, just numbers with decimal points
	 */
        if (isalpha(*str) || *str=='.' || total > NCX_MAX_RCHAR) {
            return ERR_NCX_INVALID_REALNUM;
        }
        
	res = add_new_token(tkc, TK_TT_RNUM, str, startpos);
        tkc->bptr = str;
	tkc->linepos += total;
        return res;
    }

    /* else this is a decimal number */
    total = (uint32)(str - tkc->bptr);
                          
    /* make sure we ended on a proper char and have a proper len */
    if (isalpha(*str) || total > NCX_MAX_DCHAR) {
        return ERR_NCX_INVALID_NUM;
    }

    res = add_new_token(tkc, TK_TT_DNUM, str, startpos);
    tkc->bptr = str;
    tkc->linepos += total;
    return res;

}  /* tokenize_number */


/********************************************************************
* FUNCTION get_name_comp
* 
* Get a name component
* This will stop the identifier name on the first non-name char
* whatever it is.
*
* INPUTS:
*   str == start of name component
* OUTPUTS
*   *len == length of valid name component, if NO_ERR
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_name_comp (const xmlChar *str, 
		   uint32 *len)
{
    const xmlChar *start;

    start = str;
    *len = 0;

    if (!ncx_valid_fname_ch(*str)) {
        return ERR_NCX_INVALID_NAME;
    }
        
    str++;
    while (ncx_valid_name_ch(*str)) {
        str++;
    }
    if ((str - start) < NCX_MAX_NLEN) {
        /* got a valid identifier fragment */
        *len = (uint32)(str - start);
        return NO_ERR;
    } else {
        return ERR_NCX_LEN_EXCEEDED;
    }
    /*NOTREACHED*/

}  /* get_name_comp */


/********************************************************************
* FUNCTION finish_string
* 
* Finish off a suspected identifier as a plain string
*
* INPUTS:
*   tkc == token chain
*   str == rest of string to process
* RETURNS:
*   status
*********************************************************************/
static status_t
    finish_string (tk_chain_t  *tkc,
		   xmlChar *str)
{
    boolean      done, first;
    tk_type_t    ttyp;
    uint32       startpos, total;
    status_t     res;

    startpos = tkc->linepos;

    first = TRUE;
    done = FALSE;
    while (!done) {
	if (!*str) {
	    done = TRUE;
	} else if (xml_isspace(*str) || *str=='\n') {
	    done = TRUE;
	} else if (tkc->source == TK_SOURCE_YANG) {
	    ttyp = get_token_id(str, 1, tkc->source);
	    switch (ttyp) {
	    case TK_TT_NONE:
		break;
	    case TK_TT_BAR:
	    case TK_TT_PLUS:
		/* need a hack for YANG because these tokens
		 * are skipped if part of an unquoted string
		 * but could be tokens is not in an unquoted string
		 *
		 * This may misclassify a simple string as a
		 * one char token.  If so, the YANG parser code
		 * will use the token symbol as the string
		 */
		if (first) {
		    if (yang_hack_char_test(str+1)) {
			done = TRUE;
		    } /* else inside an unquoted string */
		} else {
		    if (yang_hack_char_test(str-1) ||
			yang_hack_char_test(str+1)) {
			done = TRUE;
		    } /* else inside an unquoted string */
		}
		break;
	    default:
		done = TRUE;
	    }
	} else if (get_token_id(str, 1, tkc->source) != TK_TT_NONE) {
	    done = TRUE;
	}

	if (!done) {
	    if (*str == '\t') {
		tkc->linepos += NCX_TABSIZE;
	    } else {
		tkc->linepos++;
	    }
	    str++;
	}

	first = FALSE;
    }

    total = (uint32)(str - tkc->bptr);
    if (total > NCX_MAX_Q_STRLEN) {
        return ERR_NCX_LEN_EXCEEDED;
    }

    res = add_new_token(tkc, TK_TT_STRING, str, startpos);
    tkc->bptr = str;    /* advance the buffer pointer */
    return NO_ERR;

}  /* finish_string */


/********************************************************************
* FUNCTION tokenize_varbind_string
* 
* Handle some sort of $string, which could be a varbind string
* in the form $QName 
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_varbind_string (tk_chain_t *tkc)
{
    xmlChar        *str;
    const xmlChar  *prefix, *item;
    tk_token_t     *tk;
    uint32          len, prelen;
    status_t        res;

    prefix = NULL;
    item = NULL;

    /* the bptr is pointing at the dollar sign char */
    str = tkc->bptr+1;
    while (ncx_valid_name_ch(*str)) {
	str++;
    }

    /* check reasonable length for a QName */
    len = (uint32)(str - tkc->bptr+1);
    if (!len || len > NCX_MAX_NLEN) {
        return finish_string(tkc, str);
    }

    /* else got a string fragment that could be a valid ID format */
    if (*str == NCX_MODSCOPE_CH && str[1] != NCX_MODSCOPE_CH) {
        /* stopped on the module-scope-identifier token
         * the first identifier component must be a prefix
	 * or possibly a module name 
         */
        prefix = tkc->bptr+1;
        prelen = len;
        item = ++str;

        /* str now points at the start of the imported item 
         * There needs to be at least one valid name component
         * after the module qualifier
         */
        res = get_name_comp(str, &len);
        if (res != NO_ERR) {
            return finish_string(tkc, str);
        }
        str += len;
        /* drop through -- either we stopped on a scope char or
         * the end of the module-scoped identifier string
         */
    } 

    if (prefix) {
	/* XPath $prefix:identifier */
	tk = new_token_wmod(TK_TT_QVARBIND,
			    prefix, prelen, 
			    item, (uint32)(str - item));
    } else {
	/* XPath $identifier */
	tk = new_token(TK_TT_VARBIND,  tkc->bptr+1, len);
    }

    if (!tk) {
	return ERR_INTERNAL_MEM;
    }
    tk->linenum = tkc->linenum;
    tk->linepos = tkc->linepos;
    dlq_enque(tk, &tkc->tkQ);

    len = (uint32)(str - tkc->bptr);
    tkc->bptr = str;    /* advance the buffer pointer */
    tkc->linepos += len;
    return NO_ERR;

}  /* tokenize_varbind_string */


/********************************************************************
* FUNCTION tokenize_id_string
* 
* Handle some sort of non-quoted string, which could be an ID string
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_id_string (tk_chain_t *tkc)
{
    xmlChar        *str;
    const xmlChar  *prefix, *item;
    boolean         scoped, namestar;
    tk_token_t     *tk;
    uint32          len, prelen;
    status_t        res;

    prefix = NULL;
    item = NULL;
    scoped = FALSE;
    namestar = FALSE;
    tk = NULL;

    /* the bptr is pointing at the first string char which
     * is a valid identifier first char; start at the next char
      */
    str = tkc->bptr+1;
    if (tkc->source == TK_SOURCE_REDO) {
	/* partial ID syntax; redo so range clause 
	 * components are not included 
	 */
	while ((*str != '.') && (*str != '-') && 
	       ncx_valid_name_ch(*str)) {
	    str++;
	}
    } else {
	/* allow full YANG identifier syntax */
	while (ncx_valid_name_ch(*str)) {
	    str++;
	}
    }

    /* check max identifier length */
    if ((str - tkc->bptr) > NCX_MAX_NLEN) {
        /* string is too long to be an identifier so just
         * look for any valid 1-char token or whitespace to end it.  
         *
         * This will ignore any multi-char tokens that may be embedded, 
         * but these unquoted strings are a special case
         * and this code depends on the fact that there are
         * no valid NCX token sequences in which a string is
         * followed by a milti-char token without any 
         * whitespace in between.
         */
        return finish_string(tkc, str);
    }

    /* check YANG parser stopped on proper end of ID */
    if (tkc->source == TK_SOURCE_YANG && 
	!(*str=='{' || *str==';' || *str == '/' || *str==':'
	  || xml_isspace(*str))) {
	return finish_string(tkc, str);
    }

    /* else got a string fragment that could be a valid ID format */
    if (*str == NCX_MODSCOPE_CH && str[1] != NCX_MODSCOPE_CH) {
        /* stopped on the prefix-scope-identifier token
         * the first identifier component must be a prefix name 
         */
        prefix = tkc->bptr;
        prelen = (uint32)(str - tkc->bptr);
        item = ++str;

	if (tkc->source == TK_SOURCE_XPATH && *item == '*') {
	    namestar = TRUE;
	    str++;                   /* consume the '*' char */
	} else {
	    /* str now points at the start of the imported item 
	     * There needs to be at least one valid name component
	     * after the prefix qualifier
	     */
	    res = get_name_comp(str, &len);
	    if (res != NO_ERR) {
		return finish_string(tkc, str);
	    }
	    str += len;
	    /* drop through -- either we stopped on a scope char or
	     * the end of the prefix-scoped identifier string
	     */
	}
    } 

    if (tkc->source != TK_SOURCE_XPATH) {
	/* got some sort of identifier string 
	 * keep going until we don't stop on the scope char
	 */
	while (*str == NCX_SCOPE_CH) {
	    res = get_name_comp(++str, &len);
	    if (res != NO_ERR) {
		return finish_string(tkc, str);
	    }
	    scoped = TRUE;
	    str += len;
	}

	/* for Xpath purposes in YANG, treat scoped ID as a string now */
	if (scoped && tkc->source==TK_SOURCE_YANG) {
	    return finish_string(tkc, str);
	}

	/* done with the string; create a token and save it */
	if (prefix) {
	    if ((str - item) > NCX_MAX_Q_STRLEN) {
		return ERR_NCX_LEN_EXCEEDED;
	    }
	    tk = new_token_wmod(scoped ? TK_TT_MSSTRING : TK_TT_MSTRING,
				prefix, prelen, item, (uint32)(str - item));
	} else {
	    if ((str - tkc->bptr) > NCX_MAX_Q_STRLEN) {
		return ERR_NCX_LEN_EXCEEDED;
	    }
	    tk = new_token(scoped ? TK_TT_SSTRING : TK_TT_TSTRING,
			   tkc->bptr, (uint32)(str - tkc->bptr));
	}
    } else if (prefix) {
	if (namestar) {
	    /* XPath 'prefix:*'  */
	    tk = new_token(TK_TT_NCNAME_STAR,  prefix, prelen);
	} else {
	    /* XPath prefix:identifier */
	    tk = new_token_wmod(TK_TT_MSTRING,
				prefix, prelen, 
				item, (uint32)(str - item));
	}
    } else {
	/* XPath identifier */
	tk = new_token(TK_TT_TSTRING,  tkc->bptr, 
		       (uint32)(str - tkc->bptr));
    }

    if (!tk) {
	return ERR_INTERNAL_MEM;
    }
    tk->linenum = tkc->linenum;
    tk->linepos = tkc->linepos;
    dlq_enque(tk, &tkc->tkQ);

    len = (uint32)(str - tkc->bptr);
    tkc->bptr = str;    /* advance the buffer pointer */
    tkc->linepos += len;
    return NO_ERR;

}  /* tokenize_id_string */


/********************************************************************
* FUNCTION tokenize_string
* 
* Handle some sort of non-quoted string, which is not an ID string
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t 
    tokenize_string (tk_chain_t *tkc)
{
    xmlChar  *str;

    /* the bptr is pointing at the first string char 
     * Don't need to check it.
     * Move past it, because if we got here, this is not
     * a valid token of any other kind.
     *
     * The 2nd pass of the parser may very well barf on this 
     * string token, or it could be something like a description
     * clause, where the content is irrelevant to the parser.
     */
    str = tkc->bptr+1;
    return finish_string(tkc, str);

}  /* tokenize_string */


/********************************************************************
* FUNCTION concat_qstrings
* 
* Go through the entire token chain and handle all the
* quoted string concatenation
*
* INPUTS:
*   tkc == token chain to adjust;
*          start token is the first token
*
* OUTPUTS:
*   tkc->tkQ will be adjusted as needed:
*   First string token in a concat chain will be modified
*   to include all the text from the chain.  All the other
*   tokens in the concat chain will be deleted
*
* RETURNS:
*   status of the operation
*********************************************************************/
static status_t
    concat_qstrings (tk_chain_t *tkc)
{
    tk_token_t  *first, *plus, *next, *last;
    xmlChar     *buff, *str;
    uint32       bufflen;
    boolean      done;

    /* find the last consecutive quoted string
     * concatenate the strings together if possible
     * and redo the first quoted string to contain
     * the concatenation of the other strings
     *
     *  "string1" + "string2" + 'string3' ...
     */

    first = (tk_token_t *)dlq_firstEntry(&tkc->tkQ);
    while (first) {
	/* check if 'first' is a quoted string */
	if (!(first->typ==TK_TT_QSTRING ||
	      first->typ==TK_TT_SQSTRING)) {
	    first = (tk_token_t *)dlq_nextEntry(first);
	    continue;
	}

	/* check if any string concat is requested */
	last = NULL;
	next = (tk_token_t *)dlq_nextEntry(first);
	bufflen = first->len;
	done = FALSE;
	while (!done) {
	    if (!next || next->typ != TK_TT_PLUS) {
		/* no token or not a '+', back to outer loop */
		done = TRUE;
	    } else {
		/* found '+', should find another string next */
		plus = next;
		next = (tk_token_t *)dlq_nextEntry(next);
		if (!next || !(next->typ==TK_TT_QSTRING ||
			       next->typ==TK_TT_SQSTRING)) {
		    /* error, missing or wrong token */
		    tkc->cur = (next) ? next : plus;
		    return ERR_NCX_INVALID_CONCAT;
		} else {
		    /* OK, get rid of '+' and setup next loop */
		    last = next;
		    bufflen += last->len;
		    dlq_remove(plus);
		    free_token(plus);
		    next = (tk_token_t *)dlq_nextEntry(next);
		}
	    }
	}

	if (last) {
	    /* need to concat some strings */
	    if (bufflen > NCX_MAX_STRLEN) {
		/* user intended one really big string (2 gig!) 
		 * but this cannot done so error exit
		 */
		tkc->cur = first;
		return ERR_NCX_LEN_EXCEEDED;
	    }

	    /* else fixup the consecutive strings
	     * get a buffer to store the result
	     */
	    buff = (xmlChar *)m__getMem(bufflen+1);
	    if (!buff) {
		tkc->cur = first;
		return ERR_INTERNAL_MEM;
	    }

	    /* copy the strings */
	    str = buff;
	    next = first;
	    done = FALSE;
	    while (!done) {
		str += xml_strcpy(str, next->val);
		if (next == last) {
		    done = TRUE;
		} else {
		    next = (tk_token_t *)dlq_nextEntry(next);
		}
	    }

	    /* fixup the first token */
	    first->len = bufflen;
	    m__free(first->val);
	    first->val = buff;
	
	    /* remove the 2nd through the 'last' token */
	    done = FALSE;
	    while (!done) {
		next = (tk_token_t *)dlq_nextEntry(first);
		dlq_remove(next);
		if (next == last) {
		    done = TRUE;
		}
		free_token(next);
	    }
	    first = (tk_token_t *)dlq_nextEntry(first);
	} else {
	    /* did not find a string concat, continue search */
	    first = next;
	}
    }
		
    return NO_ERR;

}  /* concat_qstrings */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION tk_new_chain
* 
* Allocatate a new token parse chain
*
* RETURNS:
*  new parse chain or NULL if memory error
*********************************************************************/
tk_chain_t * 
    tk_new_chain (void)
{
    tk_chain_t  *tkc;

    tkc = m__getObj(tk_chain_t);
    if (!tkc) {
        return NULL;
    }
    memset(tkc, 0x0, sizeof(tk_chain_t));
    dlq_createSQue(&tkc->tkQ);
    tkc->cur = (tk_token_t *)&tkc->tkQ;

    return tkc;
           
} /* tk_new_chain */


/********************************************************************
* FUNCTION tk_setup_chain_conf
* 
* Setup a previously allocated chain for a text config file
*
* INPUTS
*    tkc == token chain to setup
*    fp == open file to use for text source
*    filename == source filespec
*********************************************************************/
void
    tk_setup_chain_conf (tk_chain_t *tkc,
			 FILE *fp,
			 const xmlChar *filename)
{
#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    tkc->fp = fp;
    tkc->source = TK_SOURCE_CONF;
    tkc->filename = filename;
           
} /* tk_setup_chain_conf */


/********************************************************************
* FUNCTION tk_setup_chain_yang
* 
* Setup a previously allocated chain for a YANG file
*
* INPUTS
*    tkc == token chain to setup
*    fp == open file to use for text source
*    filename == source filespec
*********************************************************************/
void
    tk_setup_chain_yang (tk_chain_t *tkc,
			 FILE *fp,
			 const xmlChar *filename)
{
#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    tkc->fp = fp;
    tkc->source = TK_SOURCE_YANG;
    tkc->filename = filename;
           
} /* tk_setup_chain_yang */


/********************************************************************
* FUNCTION tk_free_chain
* 
* Cleanup and deallocate a tk_chain_t 
* INPUTS:
*   tkc == TK chain to delete
* RETURNS:
*  none
*********************************************************************/
void 
    tk_free_chain (tk_chain_t *tkc)
{
    tk_token_t *tk;

#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    if (!tkc) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
    while (!dlq_empty(&tkc->tkQ)) {
        tk = (tk_token_t *)dlq_deque(&tkc->tkQ);
        free_token(tk);
    }
    if (tkc->filename && tkc->buff) {
        m__free(tkc->buff);
    }
    m__free(tkc);
    
} /* tk_free_chain */


/********************************************************************
* FUNCTION tk_get_yang_btype_id
* 
* Check if the specified string is a YANG builtin type name
*
* INPUTS:
*   buff == token string to check -- NOT ZERO-TERMINATED
*   len == length of string to check
* RETURNS:
*   btype found or NCX_BT_NONE if no match
*********************************************************************/
ncx_btype_t 
    tk_get_yang_btype_id (const xmlChar *buff, 
			  uint32 len)
{
    uint32 i;

#ifdef DEBUG
    if (!buff) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NCX_BT_NONE;
    }
    if (!len) {
        SET_ERROR(ERR_INTERNAL_VAL);
        return NCX_BT_NONE;
    }
#endif

    /* hack first because of NCX_BT_ENUM */
    if (len==11 && !xml_strncmp(buff, NCX_EL_ENUMERATION, 11)) {
	return NCX_BT_ENUM;
    }

    /* look in the blist for the specified type name */
    for (i=1; blist[i].btyp != NCX_BT_NONE; i++) {
        if ((blist[i].blen == len) &&
            !xml_strncmp(blist[i].bid, buff, len)) {
	    if (blist[i].flags & FL_YANG) {
		return blist[i].btyp;
	    } else {
		return NCX_BT_NONE;
	    }
        }
    }
    return NCX_BT_NONE;

} /* tk_get_yang_btype_id */


/********************************************************************
* FUNCTION tk_get_token_name
* 
* Get the symbolic token name
*
* INPUTS:
*   ttyp == token type
* RETURNS:
*   const string to the name; will not be NULL
*********************************************************************/
const char *
    tk_get_token_name (tk_type_t ttyp) 
{
    if (ttyp <= TK_TT_RNUM) {
        return tlist[ttyp].tname;
    } else {
        return "--none--";
    }

} /* tk_get_token_name */


/********************************************************************
* FUNCTION tk_get_token_sym
* 
* Get the symbolic token symbol
*
* INPUTS:
*   ttyp == token type
* RETURNS:
*   const string to the symbol; will not be NULL
*********************************************************************/
const char *
    tk_get_token_sym (tk_type_t ttyp) 
{
    if (ttyp <= TK_TT_GEQUAL) {
        return tlist[ttyp].tid;
    } else {
        return "--none--";
    }

} /* tk_get_token_sym */


/********************************************************************
* FUNCTION tk_get_btype_sym
* 
* Get the symbolic token symbol for one of the base types
*
* INPUTS:
*   btyp == base type
* RETURNS:
*   const string to the symbol; will not be NULL
*********************************************************************/
const char *
    tk_get_btype_sym (ncx_btype_t btyp) 
{
    if (btyp <= NCX_LAST_DATATYPE) {
        return (const char *)blist[btyp].bid;
    } else if (btyp == NCX_BT_EXTERN) {
        return "extern";
    } else if (btyp == NCX_BT_INTERN) {
	return "intern";
    } else {
	return "none";
    }
} /* tk_get_btype_sym */


/********************************************************************
* FUNCTION tk_next_typ
* 
* Get the token type of the next token
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   token type
*********************************************************************/
tk_type_t
    tk_next_typ (tk_chain_t *tkc)
{
    tk_token_t *tk;

#ifdef DEBUG
    if (!tkc || !tkc->cur) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return TK_TT_NONE;
    }
#endif

    tk = (tk_token_t *)dlq_nextEntry(tkc->cur);
    return  (tk) ? tk->typ : TK_TT_NONE;

} /* tk_next_typ */


/********************************************************************
* FUNCTION tk_next_typ2
* 
* Get the token type of the token after the next token
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   token type
*********************************************************************/
tk_type_t
    tk_next_typ2 (tk_chain_t *tkc)
{
    tk_token_t *tk;

#ifdef DEBUG
    if (!tkc || !tkc->cur) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return TK_TT_NONE;
    }
#endif

    tk = (tk_token_t *)dlq_nextEntry(tkc->cur);
    if (tk) {
	tk = (tk_token_t *)dlq_nextEntry(tk);
	return  (tk) ? tk->typ : TK_TT_NONE;
    } else {
	return TK_TT_NONE;
    }

} /* tk_next_typ2 */


/********************************************************************
* FUNCTION tk_next_val
* 
* Get the token type of the next token
*
* INPUTS:
*   tkc == token chain 
* RETURNS:
*   token type
*********************************************************************/
const xmlChar *
    tk_next_val (tk_chain_t *tkc)
{
    tk_token_t *tk;

#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    tk = (tk_token_t *)dlq_nextEntry(tkc->cur);
    return  (tk) ? (const xmlChar *)tk->val : NULL;

} /* tk_next_val */


/********************************************************************
* FUNCTION tk_dump_chain
* 
* Debug printf the token chain
* !!! Very verbose !!!
*
* INPUTS:
*   tkc == token chain 
*
* RETURNS:
*   none
*********************************************************************/
void
    tk_dump_chain (tk_chain_t *tkc)
{
    tk_token_t *tk;
    int         i;

#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }
#endif

    i = 0;
    for (tk = (tk_token_t *)dlq_firstEntry(&tkc->tkQ);
	 tk != NULL;
	 tk = (tk_token_t *)dlq_nextEntry(tk)) {
	log_debug3("\n%s line(%u.%u), tk(%d), typ(%s)",
		   (tk==tkc->cur) ? "*cur*" : "",
		   tk->linenum, tk->linepos, 
		   ++i, tk_get_token_name(tk->typ));
	if (tk->val) {
	    if (xml_strlen(tk->val) > 40) {
		log_debug3("\n");
	    }
	    log_debug3("  val(%s)", (const char *)tk->val);
	}
    }

} /* tk_dump_chain */


/********************************************************************
* FUNCTION tk_is_wsp_string
* 
* Check if the current token is a string with whitespace in it
*
* INPUTS:
*   tk == token to check
*
* RETURNS:
*    TRUE if a string with whitespace in it
*    FALSE if not a string or no whitespace in the string
*********************************************************************/
boolean
    tk_is_wsp_string (const tk_token_t *tk)
{
    const xmlChar *str;

#ifdef DEBUG
    if (!tk) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return FALSE;
    }
#endif

    switch (tk->typ) {
    case TK_TT_QSTRING:
    case TK_TT_SQSTRING:
	str = tk->val;
	while (*str && (*str != '\n') && !xml_isspace(*str)) {
	    str++;
	}
	return (*str) ? TRUE : FALSE;
    default:
	return FALSE;
    }

} /* tk_is_wsp_string */


/********************************************************************
* FUNCTION tk_tokenize_input
* 
* Parse the input (FILE or buffer) into tk_token_t structs
*
* The tkc param must be initialized
*    tkc->filename
*    tkc->fp
*    tkc->source
*
* Error messages are printed by this function!!
* Do not duplicate error messages upon error return
*
* INPUTS:
*   tkc == token chain 
*   mod == module in progress (NULL if not used)
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    tk_tokenize_input (tk_chain_t *tkc,
		       ncx_module_t *mod)
{
    status_t      res;
    boolean       done;
    tk_token_t   *tk;
    tk_type_t     ttyp;

#ifdef DEBUG
    if (!tkc) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    /* check if a temp buffer is needed */
    if (tkc->filename) {
        tkc->buff = m__getMem(TK_BUFF_SIZE);
        if (!tkc->buff) {
	    res = ERR_INTERNAL_MEM;
	    ncx_print_errormsg(tkc, mod, res);
            return res;
        } else {
	    memset(tkc->buff, 0x0, TK_BUFF_SIZE);
	}
    } /* else tkc->buff expected to be setup already */

    /* setup buffer for parsing */
    res = NO_ERR;
    tkc->bptr = tkc->buff;

    /* outside loop iterates per buffer full only
     * if reading input from file
     */
    done = FALSE;
    while (!done) {
        /* get one line of input if parsing from FILE in buffer,
	 * or already have the buffer if parsing from memory
	 */
        if (tkc->filename) {
            if (!fgets((char *)tkc->buff, TK_BUFF_SIZE, tkc->fp)) {
                /* read line failed, not an error */
                res = NO_ERR;
		done = TRUE;
		continue;
            } else {
		/* save newline token for conf file only */
		if (tkc->source == TK_SOURCE_CONF) {
		    tk = new_token(TK_TT_NEWLINE, NULL, 0);
		    if (!tk) {
			res = ERR_INTERNAL_MEM;
			done = TRUE;
			continue;
		    } 
		    tk->linenum = tkc->linenum;
		    tk->linepos = tkc->linepos;
		    dlq_enque(tk, &tkc->tkQ);
		}
                tkc->linenum++;
		tkc->linepos = 1;
            }

	    /* set buffer pointer to start of buffer */
            tkc->bptr = tkc->buff;

#ifdef TK_RDLN_DEBUG
            if (xml_strlen(tkc->buff) < 80) {
                log_debug3("\ntk_tokenize: read line (%s)", tkc->buff);
            } else {
                log_debug3("\ntk_tokenize: read line len  (%d)", 
                       xml_strlen(tkc->buff));
            }
#endif
        }

        /* Have some sort of input in the buffer (tkc->buff) */
        while (*tkc->bptr && res==NO_ERR) { 

	    /* skip whitespace */
            while (*tkc->bptr && (*tkc->bptr != '\n') &&
		   xml_isspace(*tkc->bptr)) {
		if (*tkc->bptr == '\t') {
		    tkc->linepos += NCX_TABSIZE;
		} else {
		    tkc->linepos++;
		}
                tkc->bptr++;
            }

	    /* check the first non-whitespace char found or exit */
            if (!*tkc->bptr) {
                continue;                 /* EOS, exit loop */
	    } else if (*tkc->bptr == '\n') {
		/* save newline token for conf file only */
		if (tkc->source == TK_SOURCE_CONF) {
		    tk = new_token(TK_TT_NEWLINE, NULL, 0);
		    if (!tk) {
			res = ERR_INTERNAL_MEM;
			done = TRUE;
			continue;
		    } 
		    tk->linenum = tkc->linenum;
		    tk->linepos = ++tkc->linepos;
		    dlq_enque(tk, &tkc->tkQ);
		}
		tkc->bptr++;
            } else if ((tkc->source == TK_SOURCE_CONF &&
			*tkc->bptr == NCX_COMMENT_CH) ||
		       (tkc->source == TK_SOURCE_YANG &&
			*tkc->bptr == '/' && tkc->bptr[1] == '/')) {
		/* CONF files use the '# to eoln' comment format
		 * YANG files use the '// to eoln' comment format
		 * skip past the comment, make next char EOLN
		 *
		 * TBD: SAVE COMMENTS IN XMLDOC SESSION MODE
		 */
		while (*tkc->bptr && *tkc->bptr != '\n') {
		    tkc->bptr++;
		}
	    } else if (tkc->source == TK_SOURCE_YANG &&
		       *tkc->bptr == '/' && tkc->bptr[1] == '*') {
		/* found start of a C-style YANG comment */
		res = skip_yang_cstring(tkc);
            } else if (*tkc->bptr == NCX_QSTRING_CH) {
                /* get a dbl-quoted string which may span multiple lines */
                res = tokenize_qstring(tkc);
            } else if (*tkc->bptr == NCX_SQSTRING_CH) {
                /* get a single-quoted string which may span multiple lines */
                res = tokenize_sqstring(tkc);
	    } else if (tkc->source == TK_SOURCE_XPATH &&
		       *tkc->bptr == NCX_VARBIND_CH) {
		res = tokenize_varbind_string(tkc);
            } else if (ncx_valid_fname_ch(*tkc->bptr)) {
                /* get some some of unquoted ID string or regular string */
                res = tokenize_id_string(tkc);
            } else if ((*tkc->bptr=='+' || *tkc->bptr=='-') &&
		       isdigit(*(tkc->bptr+1)) &&
		       (tkc->source != TK_SOURCE_YANG) &&
		       (tkc->source != TK_SOURCE_XPATH)) {
		/* get some sort of number 
		 * YANG does not have +/- number sequences
		 * so they are parsed (first pass) as a string
		 * There are corner cases such as range 1..max
		 * that will be parsed wrong (2nd dot).  These
		 * strings use the tk_retokenize_cur_string fn
		 * to break up the string into more tokens
		 */
                res = tokenize_number(tkc);
	    } else if (isdigit(*tkc->bptr) &&
		       (tkc->source != TK_SOURCE_YANG)) {
                res = tokenize_number(tkc);
            } else {
		/* check for a 2 char token before 1 char token */
		ttyp = get_token_id(tkc->bptr, 2, tkc->source);
		if (ttyp != TK_TT_NONE) {
		    res = add_new_token(tkc, ttyp, 
					tkc->bptr+2, tkc->linepos);
		    tkc->bptr += 2;
		    tkc->linepos += 2;
		} else {
		    /* not a 2-char, check for a 1-char token */
		    ttyp = get_token_id(tkc->bptr, 1, tkc->source);
		    if (ttyp != TK_TT_NONE) {
			/* got a 1 char token */
			res = add_new_token(tkc, ttyp, 
					    tkc->bptr+1, tkc->linepos);
			tkc->bptr++;
			tkc->linepos++;
		    } else {
                        /* ran out of token type choices 
                         * call it a string 
                         */
                        res = tokenize_string(tkc);
                    }
                }
            }
        }  /* end while non-zero chars left in buff and NO_ERR */

	/* finish outer loop, once through for buffer mode */
        if (!tkc->filename) {
            done = TRUE;
        }
    }

    if (res == NO_ERR && tkc->source != TK_SOURCE_XPATH) {
	res = concat_qstrings(tkc);
    }

    if (res == NO_ERR) {
	/* setup the token queue current pointer */
	tkc->cur = (tk_token_t *)&tkc->tkQ;
    } else {
	ncx_print_errormsg(tkc, mod, res);
    }

    return res;

}  /* tk_tokenize_input */


/********************************************************************
* FUNCTION tk_retokenize_cur_string
* 
* The current token is some sort of a string
* Reparse it according to the full NCX token list, as needed
*
* The current token may be replaced with one or more tokens
*
* INPUTS:
*   tkc == token chain 
*   mod == module in progress (NULL if not used)
*
* RETURNS:
*   status of the operation
*********************************************************************/
status_t 
    tk_retokenize_cur_string (tk_chain_t *tkc,
			      ncx_module_t *mod)
{
    tk_chain_t *tkctest;
    tk_token_t *p;
    status_t    res;

#ifdef DEBUG
    if (!tkc || !tkc->cur) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }	
#endif

    if (!TK_CUR_STR(tkc)) {
	return NO_ERR;   /* not a string, leave it alone */
    }

    /* create a test chain and parse the string */
    tkctest = tk_new_chain();
    if (!tkctest) {
	return ERR_INTERNAL_MEM;
    }
    tkctest->source = TK_SOURCE_REDO;
    tkctest->bptr = tkctest->buff = TK_CUR_VAL(tkc);
    res = tk_tokenize_input(tkctest, mod);

    /* check if the token parse was different
     * if more than 1 token, then it was changed
     * from a single string to something else
     */
    if (res == NO_ERR) {
	/* redo token line info for these expanded tokens */
	for (p = (tk_token_t *)dlq_firstEntry(&tkctest->tkQ); 
	     p != NULL;
	     p = (tk_token_t *)dlq_nextEntry(p)) {
	    p->linenum = tkc->cur->linenum;
	    p->linepos = tkc->cur->linepos;
	}

	dlq_block_insertAfter(&tkctest->tkQ, tkc->cur);

	/* get rid of the original string and reset the cur token */
	p = (tk_token_t *)dlq_nextEntry(tkc->cur);
	dlq_remove(tkc->cur);
	free_token(tkc->cur);
	tkc->cur = p;
    }

    tk_free_chain(tkctest);

    return res;

} /* tk_retokenize_cur_string */


/********************************************************************
* FUNCTION tk_tokenize_metadata_string
* 
* The specified ncx:metadata string is parsed into tokens
*
* INPUTS:
*   mod == module in progress for error purposes (may be NULL)
*   str == string to tokenize
*   res == address of return status
*
* OUTPUTS:
*   *res == error status, if return NULL or non-NULL
*
* RETURNS:
*   pointer to malloced and filled in token chain
*   ready to be traversed; always check *res for valid syntax
*********************************************************************/
tk_chain_t *
    tk_tokenize_metadata_string (ncx_module_t *mod,
				 xmlChar *str,
				 status_t *res)
{
    tk_chain_t *tkc;

#ifdef DEBUG
    if (!str || !res) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }	
#endif

    /* create a new chain and parse the string */
    tkc = tk_new_chain();
    if (!tkc) {
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }
    tkc->source = TK_SOURCE_YANG;
    tkc->bptr = tkc->buff = str;
    *res = tk_tokenize_input(tkc, mod);
    return tkc;

} /* tk_tokenize_metadata_string */


/********************************************************************
* FUNCTION tk_tokenize_xpath_string
* 
* The specified XPath string is parsed into tokens
*
* INPUTS:
*   mod == module in progress for error purposes (may be NULL)
*   str == string to tokenize
*   curlinenum == current line number
*   curlinepos == current line position
*   res == address of return status
*
* OUTPUTS:
*   *res == error status, if return NULL or non-NULL
*
* RETURNS:
*   pointer to malloced and filled in token chain
*   ready to be traversed; always check *res for valid syntax
*********************************************************************/
tk_chain_t *
    tk_tokenize_xpath_string (ncx_module_t *mod,
			      xmlChar *str,
			      uint32 curlinenum,
			      uint32 curlinepos,
			      status_t *res)
{
    tk_chain_t *tkc;

#ifdef DEBUG
    if (!str || !res) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }	
#endif

    /* create a new chain and parse the string */
    tkc = tk_new_chain();
    if (!tkc) {
	*res = ERR_INTERNAL_MEM;
	return NULL;
    }
    tkc->source = TK_SOURCE_XPATH;
    tkc->bptr = tkc->buff = str;
    tkc->linenum = curlinenum;
    tkc->linepos = curlinepos;
    *res = tk_tokenize_input(tkc, mod);
    return tkc;

} /* tk_tokenize_xpath_string */


/********************************************************************
* FUNCTION tk_token_count
* 
* Get the number of tokens in the queue
*
* INPUTS:
*   tkc == token chain to check
*
* RETURNS:
*   number of tokens in the queue
*********************************************************************/
uint32
    tk_token_count (const tk_chain_t *tkc) 
{
    const tk_token_t *tk;
    uint32  cnt;

#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return 0;
    }	
#endif

    cnt = 0;
    for (tk = (const tk_token_t *)dlq_firstEntry(&tkc->tkQ);
	 tk != NULL;
	 tk = (const tk_token_t *)dlq_nextEntry(tk)) {
	cnt++;
    }
    return cnt;

} /* tk_token_count */


/********************************************************************
* FUNCTION tk_reset_chain
* 
* Reset the token chain current pointer to the start
*
* INPUTS:
*   tkc == token chain to reset
*
*********************************************************************/
void
    tk_reset_chain (tk_chain_t *tkc) 
{
#ifdef DEBUG
    if (!tkc) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return;
    }	
#endif
    tkc->cur = (tk_token_t *)&tkc->tkQ;

}  /* tk_reset_chain */


/* END file tk.c */
