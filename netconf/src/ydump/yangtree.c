
/*
 * Copyright (c) 2018, Vladimir Vassilev, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangtree.c

  Generate YANG tree output for a specific YANG module

/********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include <libxml/xmlstring.h>
#include <libxml/xmlreader.h>
#include  "procdefs.h"
#include "c_util.h"
#include "dlq.h"
#include "ext.h"
#include "ncx_grp.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "py_util.h"
#include "ses.h"
#include "yangtree.h"
#include "status.h"
#include "tstamp.h"
#include "typ.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "xpath.h"
#include "xsd_util.h"
#include "yang.h"
#include "yangconst.h"
#include "yangdump.h"
#include "yangdump_util.h"


/* if the object is input it checks if the following output is empty of children or not */
boolean is_last_non_empty_rpcio(obj_template_t* obj)
{
    obj_template_t* prev_obj=obj;
    obj_template_t* next_obj;

    if(obj->objtype!=OBJ_TYP_RPCIO) {
        return FALSE;
    }

    while(next_obj=(obj_template_t *)dlq_nextEntry(prev_obj)) {
        if(next_obj->objtype==OBJ_TYP_RPCIO) {
            dlq_hdr_t* ccq=obj_get_datadefQ(next_obj);
            if(ccq && dlq_firstEntry(ccq)) {
               /*do not print empty input and output*/
               return FALSE;
           }
        }

        prev_obj = next_obj;
    }
    return TRUE;
}


boolean is_last_non_uses(obj_template_t* obj)
{
    obj_template_t* prev_obj=obj;
    obj_template_t* next_obj;

    while(next_obj=(obj_template_t *)dlq_nextEntry(prev_obj)) {
        if(next_obj->objtype!=OBJ_TYP_USES) {
            return FALSE;
        }
        prev_obj = next_obj;
    }
    return TRUE;
}

boolean is_last_data(obj_template_t* obj)
{
    obj_template_t* prev_obj=obj;
    obj_template_t* next_obj;

    while(next_obj=(obj_template_t *)dlq_nextEntry(prev_obj)) {
        if(next_obj->objtype==OBJ_TYP_CONTAINER || next_obj->objtype==OBJ_TYP_LEAF || next_obj->objtype==OBJ_TYP_LIST) {
            return FALSE;
        }
        prev_obj = next_obj;
    }
    return TRUE;
}

boolean is_last_rpc(obj_template_t* obj)
{
    obj_template_t* prev_obj=obj;
    obj_template_t* next_obj;

    while(next_obj=(obj_template_t *)dlq_nextEntry(prev_obj)) {
        if(next_obj->objtype==OBJ_TYP_RPC) {
            return FALSE;
        }
        prev_obj = next_obj;
    }
    return TRUE;
}

boolean is_last_notification(obj_template_t* obj)
{
    obj_template_t* prev_obj=obj;
    obj_template_t* next_obj;

    while(next_obj=(obj_template_t *)dlq_nextEntry(prev_obj)) {
        if(next_obj->objtype==OBJ_TYP_NOTIF) {
            return FALSE;
        }
        prev_obj = next_obj;
    }
    return TRUE;
}

unsigned int get_child_name_width_max(obj_template_t *obj, boolean from_top)
{
    obj_template_t* chobj;
    dlq_hdr_t* childdatadefQ;
    unsigned int len_max;

    childdatadefQ = obj_get_datadefQ(obj);
    assert(childdatadefQ);
    len_max=0;
    if(!from_top && (obj->objtype == OBJ_TYP_CHOICE || obj->objtype == OBJ_TYP_CASE)) {
        return get_child_name_width_max(obj->parent, FALSE);
    }
    for (chobj = (obj_template_t *)dlq_firstEntry(childdatadefQ);
         chobj != NULL;
         chobj = (obj_template_t *)dlq_nextEntry(chobj)) {
        unsigned int len;
        if(chobj->objtype == OBJ_TYP_USES) {
            continue;
        } else if(chobj->objtype == OBJ_TYP_CHOICE || chobj->objtype == OBJ_TYP_CASE) {
            len = get_child_name_width_max(chobj, TRUE);
        } else {
            len = strlen(obj_get_name(chobj));
        }
        if(len_max<len) {
            len_max=len;
        }
    }

    return len_max;
}

char* get_status_letter(obj_template_t* obj)
{
    if(obj_get_status(obj) == NCX_STATUS_OBSOLETE) {
        return "o";
    } else if (obj_get_status(obj) == NCX_STATUS_DEPRECATED){
        return "x";
    } else {
        return "+";
    }
}

void print_feature_deps(obj_template_t* obj)
{
    /*<if-features>*/
    if(!dlq_empty(&obj->iffeatureQ)){
        ncx_iffeature_t  *iff;
        printf(" {");

        for (iff = (ncx_iffeature_t *)
                       dlq_firstEntry(&obj->iffeatureQ);
             iff != NULL;
             iff = (ncx_iffeature_t *)dlq_nextEntry(iff)) {

            if(iff->tkerr.mod!=obj->tkerr.mod) {
                printf("%s:%s",iff->tkerr.mod->prefix,iff->name);
            } else {
                printf("%s",iff->name);
            }
            printf("%s",dlq_nextEntry(iff)?" ":"");
        }
        printf("}?");
    }
}

void print_data_obj(obj_template_t *obj, char* line_prefix)
{
    obj_template_t* chobj;
    dlq_hdr_t* childdatadefQ;
    int j;
    unsigned int name_width_max;
    char* child_line_prefix;

    childdatadefQ = obj_get_datadefQ(obj);
    if (!childdatadefQ) {
        return;
    }

    name_width_max = get_child_name_width_max(obj, FALSE);

    for (chobj = (obj_template_t *)dlq_firstEntry(childdatadefQ);
         chobj != NULL;
         chobj = (obj_template_t *)dlq_nextEntry(chobj)) {
        boolean check_for_subnodes;
        unsigned int name_width_cur=0;

        if(chobj->objtype==OBJ_TYP_USES) {
            continue;
        }

        if(chobj->objtype==OBJ_TYP_RPCIO) {
           dlq_hdr_t* ccq=obj_get_datadefQ(chobj);
           if(!ccq || !dlq_firstEntry(ccq)) {
               /*do not print empty input and output*/
               continue;
           }
        }

        printf("%s",line_prefix);
        /*<status>*/
        printf("%s--",get_status_letter(chobj));

        /*<flags>*/
        if(chobj->objtype!=OBJ_TYP_CASE) {
            if(obj_in_rpc(chobj) || obj_in_rpc_reply(chobj)) {
                printf("%s ", obj_in_rpc(chobj)?"-w":"ro");
            } else if(chobj->objtype==OBJ_TYP_RPC /*should be OBJ_TYP_ACTION*/) {
                printf("%s ", "-x");
            } else {
                printf("%s ", obj_get_config_flag(chobj)?"rw":"ro");
            }
        }
        /*<name>*/
        if(chobj->objtype==OBJ_TYP_CHOICE) {
            name_width_cur+=printf("(%s)", obj_get_name(chobj));
        } else if(chobj->objtype==OBJ_TYP_CASE) {
            name_width_cur+=printf(":(%s)", obj_get_name(chobj));
        } else {
            name_width_cur+=printf("%s", obj_get_name(chobj));
        }
 
        /*<opts>*/
        if((chobj->objtype == OBJ_TYP_LEAF || chobj->objtype == OBJ_TYP_CHOICE || chobj->objtype == OBJ_TYP_ANYDATA || chobj->objtype == OBJ_TYP_ANYXML) && !obj_is_mandatory(chobj)) {
            /* ?  for an optional leaf, choice, anydata or anyxml */
            name_width_cur+=printf("?");
        } else if((chobj->objtype == OBJ_TYP_CONTAINER) && !obj_is_np_container(chobj)) {
            /* !  for a presence container */
            name_width_cur+=printf("!");
        } else if((chobj->objtype == OBJ_TYP_LEAF_LIST)) {
            /* *  for a leaf-list or list */
            name_width_cur+=printf("*");
        } else if(chobj->objtype == OBJ_TYP_LIST) {
            /* [<keys>] for a list's keys */
            obj_key_t *key;
            printf("* [");
            for (key = obj_first_key(chobj);
                 key != NULL;
                 key = obj_next_key(key)) {
                printf("%s%s",obj_get_name(key->keyobj),obj_next_key(key)?" ":"");
            }
            printf("]");
        }
        /* /  for a top-level data node in a mounted module */
        /* @  for a top-level data node in a parent referenced module */

        if(chobj->objtype == OBJ_TYP_LIST || chobj->objtype == OBJ_TYP_CONTAINER || chobj->objtype == OBJ_TYP_CHOICE || chobj->objtype == OBJ_TYP_CASE || chobj->objtype == OBJ_TYP_RPCIO || chobj->objtype == OBJ_TYP_RPC /*should be OBJ_TYP_ACTION*/) {
            check_for_subnodes=TRUE;
        } else {
            check_for_subnodes=FALSE;
            /*type alignment - line += "%s %-*s   %s" % (flags, width+1, name, t)*/
            for(j=0;j<(int)name_width_max+1-(int)name_width_cur;j++) {
                printf(" ");
            }
            printf("  ");

            /*<type>*/
            {
                typ_def_t * typdef;
                typdef = obj_get_typdef(chobj);

                if(typdef && typdef->typenamestr) {
                    if(typdef->prefix) {
                        printf(" %s:%s", typdef->prefix, typdef->typenamestr);
                    } else {
                        if(0==strcmp("leafref",typdef->typenamestr)) {
                            printf(" -> %s", typdef->def.simple.xleafref->exprstr);
                        } else {
                            printf(" %s", typdef->typenamestr);
                        }
                    }
                } else if(chobj->objtype==OBJ_TYP_ANYXML) {
                    printf(" %s", "<anyxml>");
                } else if(chobj->objtype==OBJ_TYP_ANYDATA) {
                    printf(" %s", "<anydata>");
                }
            }
        }
//        printf(obj_get_type_name(chobj));
//        printf(" %s",obj_get_basetype(obj));

        /*<if-features>*/
        print_feature_deps(chobj);

        if(check_for_subnodes) {
            child_line_prefix=malloc(strlen(line_prefix)+3+1);
            if(!is_last_non_uses(chobj) && !is_last_non_empty_rpcio(chobj)) {
                sprintf(child_line_prefix, "%s|  ",line_prefix);
            } else {
                sprintf(child_line_prefix, "%s   ",line_prefix);
            }
            print_data_obj(chobj, child_line_prefix);
            free(child_line_prefix);
        }
    }
}

void print_data(const ncx_module_t    *mod)
{
    obj_template_t* obj;
    if (dlq_empty(&mod->datadefQ)) {
        return;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(&mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(obj)) {

        if (!obj_has_name(obj) || obj_is_cli(obj) || obj_is_abstract(obj) || obj->objtype==OBJ_TYP_AUGMENT || obj->objtype==OBJ_TYP_RPC || obj->objtype==OBJ_TYP_NOTIF) {
            continue;
        }
        printf("\n  %s--",get_status_letter(obj)); // printf("\n  +--");

        printf("%s ", obj_get_config_flag(obj)?"rw":"ro");
        printf("%s", obj_get_name(obj));
        if((obj->objtype == OBJ_TYP_CONTAINER) && !obj_is_np_container(obj)) {
            /* !  for a presence container */
            printf("!");
        }   print_feature_deps(obj);
        if(!is_last_data(obj)) {
            print_data_obj(obj, "\n  |  ");
        } else {
            print_data_obj(obj, "\n     ");
        }
    }
}

void print_augmentations(const ncx_module_t    *mod)
{
    obj_template_t* obj;
    if (dlq_empty(&mod->datadefQ)) {
        return;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(&mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(obj)) {

        if (obj->objtype!=OBJ_TYP_AUGMENT) {
            continue;
        }
        printf("\n  augment %s:", obj->def.augment->target);
        print_data_obj(obj, "\n    ");
    }
}

void print_rpcs(const ncx_module_t    *mod)
{
    obj_template_t* obj;
    unsigned int cnt=0;
    if (dlq_empty(&mod->datadefQ)) {
        return;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(&mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(obj)) {

        if (obj->objtype!=OBJ_TYP_RPC) {
            continue;
        }
        if(cnt==0) {
            printf("\n  rpcs:");
        }
        printf("\n    %s---x %s", get_status_letter(obj), obj_get_name(obj));

        print_feature_deps(obj);

        if(!is_last_rpc(obj)) {
            print_data_obj(obj, "\n    |  ");
        } else {
            print_data_obj(obj, "\n       ");
        }
        cnt++;
    }
}

void print_notifications(const ncx_module_t    *mod)
{
    obj_template_t* obj;
    unsigned int cnt=0;
    if (dlq_empty(&mod->datadefQ)) {
        return;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(&mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(obj)) {

        if (obj->objtype!=OBJ_TYP_NOTIF) {
            continue;
        }
        if(cnt==0) {
            printf("\n  notifications:");
        }
        printf("\n    %s---n %s", get_status_letter(obj), obj_get_name(obj));

        print_feature_deps(obj);

        if(!is_last_notification(obj)) {
            print_data_obj(obj, "\n    |  ");
        } else {
            print_data_obj(obj, "\n       ");
        }
        cnt++;
    }
}

#if 0
        if (obj->objtype == OBJ_TYP_NOTIF) {
            return TRUE;
        }
#endif
/*********     E X P O R T E D   F U N C T I O N S    **************/


/********************************************************************
* FUNCTION yangtree_convert_module
* 
*  Convert the YANG module to YANG tree format
*
* INPUTS:
*   pcb == parser control block of module to convert
*          This is returned from ncxmod_load_module_ex
*   cp == convert parms struct to use
*   scb == session to use for output
*
* RETURNS:
*   status
*********************************************************************/
status_t
    yangtree_convert_module(const yang_pcb_t *pcb,
                              const yangdump_cvtparms_t *cp,
                              ses_cb_t *scb)
{
    const ncx_module_t    *mod;
    ncx_include_t  *inc;

    /* the module should already be parsed and loaded */
    mod = pcb->top;
    if (!mod) {
        return SET_ERROR(ERR_NCX_MOD_NOT_FOUND);
    }
    //ses_putstr(scb, "module: ");
    if (dlq_empty(&mod->datadefQ)) {
        return NO_ERR;
    }
    printf("module: %s", mod->name);

    /*draft-ietf-netmod-yang-tree-diagrams-04 sec.2*/
    print_data(mod);
    /* check for submodule match */
    for (inc = (ncx_include_t *)dlq_firstEntry(&mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(inc)) {
        ncx_module_t  *submod = inc->submod;
        print_data(submod);
    }
    print_augmentations(mod);
    /* check for submodule match */
    for (inc = (ncx_include_t *)dlq_firstEntry(&mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(inc)) {
        ncx_module_t  *submod = inc->submod;
        print_augmentations(submod);
    }
    print_rpcs(mod);
    /* check for submodule match */
    for (inc = (ncx_include_t *)dlq_firstEntry(&mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(inc)) {
        ncx_module_t  *submod = inc->submod;
        print_rpcs(submod);
    }
    print_notifications(mod);
    /* check for submodule match */
    for (inc = (ncx_include_t *)dlq_firstEntry(&mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(inc)) {
        ncx_module_t  *submod = inc->submod;
        print_notifications(submod);
    }
#if 0
    print_groupings();
    print_yangdata();
#endif
    return NO_ERR;

}   /* yangtree_convert_module */


/* END file yangtree.c */
