#ifndef _H_yang
#define _H_yang

/*  FILE: yang.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    YANG Module parser utilities

    
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
15-nov-07    abb      Begun; start from yang_parse.c

*/

#include <xmlstring.h>

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_ext
#include "ext.h"
#endif

#ifndef _H_grp
#include "grp.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_tk
#include "tk.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* YANG parser mode entry types */
typedef enum yang_parsetype_t_ {
    YANG_PT_NONE,
    YANG_PT_TOP,          /* called from top level [sub]module */
    YANG_PT_INCLUDE,         /* called from module include-stmt */
    YANG_PT_IMPORT,           /* called from module import-stmt */
    YANG_PT_TOP_INCL     /* called from submodule to get prefix */
} yang_parsetype_t;


/* YANG statement types for yang_stmt_t order struct */
typedef enum yang_stmttype_t_ {
    YANG_ST_NONE,
    YANG_ST_TYPEDEF,            /* node is typ_template_t */
    YANG_ST_GROUPING,           /* node is grp_template_t */
    YANG_ST_EXTENSION,          /* node is ext_template_t */
    YANG_ST_OBJECT              /* node is obj_template_t */
} yang_stmttype_t;


/* YANG statement node to track top-level statement order for doc output */
typedef struct yang_stmt_t_ {
    dlq_hdr_t        qhdr;
    yang_stmttype_t  stmttype;
    /* these node pointers are back-pointers and not malloced here */
    union s_ {
	typ_template_t *typ;
	grp_template_t *grp;
	ext_template_t *ext;
	obj_template_t *obj;
    } s;
} yang_stmt_t;


/* YANG node entry to track if a module has been used already */
typedef struct yang_node_t_ {
    dlq_hdr_t     qhdr;
    const xmlChar *name;    /* module name in imp/inc/failed */
    ncx_module_t  *mod;     /* back-ptr to module w/ imp/inc */
    tk_token_t    *tk;      /* back-ptr to token for imp/inc */
    ncx_module_t  *submod;             /* submod for allincQ */
    xmlChar       *failed;  /* saved name for failed entries */
    tk_chain_t    *tkc;      /* saved token chain for errors */
    status_t       res;         /* saved result for 'failed' */
} yang_node_t;


/* YANG import pointer node to track all imports used */
typedef struct yang_import_ptr_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *modname;
    xmlChar     *modprefix;
} yang_import_ptr_t;


/* YANG parser control block
 *
 * top level parse can be for a module or a submodule
 * if module:
 *     top == module being processed
 *     mod == NULL (not used)
 * If submodule:
 *     top == submodule being processed
 *     mod == module identified by 'belongs-to', parsed
 *            just enough to get the namespace and prefix
 *
 * The allimpQ is a cache of pointers to all the imports that
 *  have been processed.  This is used by yangdump for
 *  document processing and error detection.  Imports
 *  are stored in the def_reg, so this Q is not really needed
 *  by manager or agent applications.

 * The impchainQ is really a stack of imports that
 * are being processed, used for import loop detection
 * 
 * The allincQ is a cache of all the includes that have been
 * processed.  Includes can be nested, and are only parsed once,
 *
 * The incchainQ is really a stack of includes that
 * are being processed, used for include loop detection
 *
 * The failedQ is a list of all the modules that had fatal
 * errors, and have already been processed.  This is used
 * to prevent error messages from imports to be printed more
 * than once, and speeds up validation processing
 */
typedef struct yang_pcb_t_ {
    struct ncx_module_t_ *mod;      /* top-level module */
    struct ncx_module_t_ *top;        /* top-level file */
    boolean       subtree_mode;
    boolean       with_submods;
    boolean       stmtmode;      /* save top-level stmt order */
    boolean       diffmode;        /* TRUE = yangdiff old ver */
    dlq_hdr_t     allimpQ;          /* Q of yang_import_ptr_t */

    /* 4 Qs of yang_node_t */
    dlq_hdr_t     impchainQ;      /* cur chain of import used */
    dlq_hdr_t     allincQ;               /* all includes used */
    dlq_hdr_t     incchainQ;     /* cur chain of include used */
    dlq_hdr_t     failedQ;       /* load mod or submod failed */
} yang_pcb_t;


/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/* consume a stmtsep clause */
extern status_t 
    yang_consume_semiapp (tk_chain_t *tkc,
			  ncx_module_t *mod,
			  dlq_hdr_t *appinfoQ);


/* consume 1 string token */
extern status_t 
    yang_consume_string (tk_chain_t *tkc,
			 ncx_module_t *mod,
			 xmlChar **field);

/* consume 1 YANG keyword or vendor extension keyword */
extern status_t 
    yang_consume_keyword (tk_chain_t *tkc,
			  ncx_module_t *mod,
			  xmlChar **prefix,
			  xmlChar **field);

/* consume 1 string without any whitespace */
extern status_t 
    yang_consume_nowsp_string (tk_chain_t *tkc,
			       ncx_module_t *mod,
			       xmlChar **field);

/* consume an identifier-str token */
extern status_t 
    yang_consume_id_string (tk_chain_t *tkc,
			    ncx_module_t *mod,
			    xmlChar **field);

/* consume an identifier-ref-str token */
extern status_t 
    yang_consume_pid_string (tk_chain_t *tkc,
			     ncx_module_t *mod,
			     xmlChar **prefix,
			     xmlChar **field);

/* consume the range. length. pattern. must error info extensions */
extern status_t 
    yang_consume_error_stmts (tk_chain_t  *tkc,
			      ncx_module_t *mod,
			      ncx_errinfo_t **errinfo,
			      dlq_hdr_t *appinfoQ);

/* consume one descriptive string clause */
extern status_t 
    yang_consume_descr (tk_chain_t  *tkc,
			ncx_module_t *mod,
			xmlChar **str,
			boolean *dupflag,
			dlq_hdr_t *appinfoQ);

/* consume one normative string clause */
extern status_t 
    yang_consume_strclause (tk_chain_t  *tkc,
			    ncx_module_t *mod,
			    xmlChar **str,
			    boolean *dupflag,
			    dlq_hdr_t *appinfoQ);


/* consume one status clause */
extern status_t 
    yang_consume_status (tk_chain_t  *tkc,
			 ncx_module_t *mod,
			 ncx_status_t *status,
			 boolean *dupflag,
			 dlq_hdr_t *appinfoQ);

/* consume one must-stmt into mustQ */
extern status_t
    yang_consume_must (tk_chain_t  *tkc,
		       ncx_module_t *mod,
		       dlq_hdr_t *mustQ,
		       dlq_hdr_t *appinfoQ);

/* consume one boolean clause */
extern status_t 
    yang_consume_boolean (tk_chain_t  *tkc,
			  ncx_module_t *mod,
			  boolean *bool,
			  boolean *dupflag,
			  dlq_hdr_t *appinfoQ);

/* consume one int32 clause */
extern status_t 
    yang_consume_int32 (tk_chain_t  *tkc,
			ncx_module_t *mod,
			int32 *num,
			boolean *dupflag,
			dlq_hdr_t *appinfoQ);

/* consume one uint32 clause */
extern status_t 
    yang_consume_uint32 (tk_chain_t  *tkc,
			 ncx_module_t *mod,
			 uint32 *num,
			 boolean *dupflag,
			 dlq_hdr_t *appinfoQ);


/* find an imported typedef */
extern status_t 
    yang_find_imp_typedef (tk_chain_t  *tkc,
			   ncx_module_t *mod,
			   const xmlChar *prefix,
			   const xmlChar *name,
			   tk_token_t *errtk,
			   typ_template_t **typ);

/* find an imported grouping */
extern status_t 
    yang_find_imp_grouping (tk_chain_t  *tkc,
			    ncx_module_t *mod,
			    const xmlChar *prefix,
			    const xmlChar *name,
			    tk_token_t *errtk,
			    grp_template_t **grp);

extern status_t 
    yang_find_imp_extension (tk_chain_t  *tkc,
			     ncx_module_t *mod,
			     const xmlChar *prefix,
			     const xmlChar *name,
			     tk_token_t *errtk,
			     ext_template_t **ext);


/* generate warnings if local typedefs/groupings not used */
extern void
    yang_check_obj_used (tk_chain_t *tkc,
			 ncx_module_t *mod,
			 dlq_hdr_t *typeQ,
			 dlq_hdr_t *grpQ);

/* generate warnings if imports not used */
extern void
    yang_check_imports_used (tk_chain_t *tkc,
			     ncx_module_t *mod);

extern yang_node_t *
    yang_new_node (void);

extern void
    yang_free_node (yang_node_t *node);

extern void
    yang_clean_nodeQ (dlq_hdr_t *que);

extern yang_node_t *
    yang_find_node (dlq_hdr_t *que,
		    const xmlChar *name);

extern yang_pcb_t *
    yang_new_pcb (void);

extern void
    yang_free_pcb (yang_pcb_t *pcb);


extern yang_stmt_t *
    yang_new_typ_stmt (typ_template_t *typ);

extern yang_stmt_t *
    yang_new_grp_stmt (grp_template_t *grp);

extern yang_stmt_t *
    yang_new_ext_stmt (ext_template_t *ext);

extern yang_stmt_t *
    yang_new_obj_stmt (obj_template_t *obj);

extern void
    yang_free_stmt (yang_stmt_t *stmt);

extern void
    yang_clean_stmtQ (dlq_hdr_t *que);

extern void
    yang_dump_nodeQ (dlq_hdr_t *que,
		     const char *name);

extern status_t
    yang_validate_date_string (tk_chain_t *tkc,
			      ncx_module_t *mod,
			      tk_token_t *errtk,
			      const xmlChar *datestr);

extern void
    yang_skip_statement (tk_chain_t *tkc,
			 ncx_module_t *mod);

extern boolean
    yang_top_keyword (const xmlChar *keyword);

extern yang_import_ptr_t *
    yang_new_import_ptr (const xmlChar *modname,
			 const xmlChar *modprefix);

extern void
    yang_free_import_ptr (yang_import_ptr_t *impptr);

extern void
    yang_clean_import_ptrQ (dlq_hdr_t *que);

extern yang_import_ptr_t *
    yang_find_import_ptr (dlq_hdr_t *que,
			  const xmlChar *name);

#endif	    /* _H_yang */
