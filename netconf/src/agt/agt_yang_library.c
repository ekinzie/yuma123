/*
 * Copyright (c) 2017, Vladimir Vassilev, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: agt_yang_library.c

   NETCONF ietf-yang-library.yang Data Model implementation: Server Side Support


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
26jul17      vv       begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sys/utsname.h>
#include <assert.h>
#include <openssl/sha.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_cap.h"
#include "agt_cb.h"
#include "agt_cfg.h"
#include "agt_cli.h"
#include "agt_not.h"
#include "agt_rpc.h"
#include "agt_ses.h"
#include "agt_sys.h"
#include "agt_util.h"
#include "cfg.h"
#include "getcb.h"
#include "log.h"
#include "ncxmod.h"
#include "ncxtypes.h"
#include "ncx_feature.h"
#include "ncx_list.h"
#include "rpc.h"
#include "rpc_err.h"
#include "ses.h"
#include "ses_msg.h"
#include "status.h"
#include "tstamp.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_wr.h"
#include "yangconst.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define ietf_yang_library_N_modules_state (const xmlChar *)"modules-state"


/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

static boolean              agt_yang_library_init_done = FALSE;

/* ietf-yang-library.yang */
static ncx_module_t         *ietf_yang_library_mod;

/* cached pointers*/
static obj_template_t *ietf_yang_library_modules_state_obj;

/************* I N T E R N A L    F U N C T I O N S ***************/
 
static status_t
    get_modules_state(ses_cb_t *scb,
                       getcb_mode_t cbmode,
                       val_value_t *vir_val,
                       val_value_t  *dst_val)
{
    status_t res;
    ncx_module_t *mod;
    obj_template_t *module_obj;
    obj_template_t *deviation_obj;
    obj_template_t *submodule_obj;
    val_value_t *module_set_id_val;
    val_value_t *module_val;
    val_value_t *deviation_val;
    val_value_t *submodule_val;
    val_value_t *childval;
    ncx_feature_t *feature;
    ncx_save_deviations_t *devmod;
    ncx_include_t *inc;

    module_obj = obj_find_child(ietf_yang_library_modules_state_obj, "ietf-yang-library", "module");
    assert(module_obj);

    /* add all modules */
    for (mod = ncx_get_first_module();
         mod != NULL;
         mod = ncx_get_next_module(mod)) {

        if (!agt_advertise_module_needed(mod->name)) {
            continue;
        }

        /* create schema node */
        module_val = val_new_value();
        assert(module_val);

        val_init_from_template(module_val, module_obj);

        /* name */
        childval = agt_make_leaf(module_obj,
                             "name",
                             ncx_get_modname(mod), 
                             &res);
        assert(res==NO_ERR && childval);

        val_add_child(childval, module_val);

        /* revision */
        childval = agt_make_leaf(module_obj,
                             "revision",
                             ncx_get_modversion(mod)?ncx_get_modversion(mod):(xmlChar *)"",
                             &res);
        assert(res==NO_ERR && childval);
        val_add_child(childval, module_val);


        /* namespace */
        childval = agt_make_leaf(module_obj,
                             "namespace",
                             ncx_get_modnamespace(mod), 
                             &res);
        assert(res==NO_ERR && childval);
        val_add_child(childval, module_val);

        /* conformance-type */
        childval = agt_make_leaf(module_obj,
                             "conformance-type",
                             mod->implemented?"implement":"import",
                             &res);
        assert(res==NO_ERR && childval);
        val_add_child(childval, module_val);

        /* feature */
        for (feature = (ncx_feature_t *)dlq_firstEntry(&mod->featureQ);
            feature != NULL;
            feature = (ncx_feature_t *)dlq_nextEntry(feature)) {

            if (ncx_feature_enabled(feature)) {
                childval = agt_make_leaf(module_obj,
                             "feature",
                             feature->name,
                             &res);
                assert(res==NO_ERR && childval);
                val_add_child(childval, module_val);
            }
        }

        /* deviation */
        deviation_obj = obj_find_child(module_obj, "ietf-yang-library", "deviation");
        assert(deviation_obj);
        for (devmod = dlq_firstEntry(&mod->devmodlist);
             devmod != NULL;
             devmod = (ncx_save_deviations_t *)dlq_nextEntry(devmod)) {
            ncx_module_t *match_mod;
            char* revision_str;

            deviation_val = val_new_value();
            assert(deviation_val);
            val_init_from_template(deviation_val, deviation_obj);

            childval = agt_make_leaf(deviation_obj,
                                 "name",
                                 devmod->devmodule,
                                &res);
            assert(res==NO_ERR && childval);
            val_add_child(childval, deviation_val);

            childval = agt_make_leaf(deviation_obj,
                                 "revision",
                                 devmod->devrevision,
                                &res);
            assert(res==NO_ERR && childval);

            val_add_child(childval, deviation_val);

            res = val_gen_index_chain(deviation_obj, deviation_val);
            assert(res == NO_ERR);

            val_add_child(deviation_val, module_val);
        }

        /* submodule */
        submodule_obj = obj_find_child(module_obj, "ietf-yang-library", "submodule");
        assert(submodule_obj);
        for (inc = (ncx_include_t *)dlq_firstEntry(&mod->includeQ);
             inc != NULL;
             inc = (ncx_include_t *)dlq_nextEntry(inc)) {

            submodule_val = val_new_value();
            assert(submodule_val);
            val_init_from_template(submodule_val, submodule_obj);

            /* name */
            childval = agt_make_leaf(submodule_obj,
                                 "name",
                                 inc->submodule,
                                 &res);
            assert(res==NO_ERR && childval);

            val_add_child(childval, submodule_val);

            /* revision */
            childval = agt_make_leaf(submodule_obj,
                                 "revision",
                                 inc->revision?inc->revision:(xmlChar *)"",
                                 &res);
            assert(res==NO_ERR && childval);
            val_add_child(childval, submodule_val);

            val_add_child(submodule_val, module_val);
            res = val_gen_index_chain(submodule_obj, submodule_val);
            assert(res == NO_ERR);
        }
        /* submodule - ends */
        res = val_gen_index_chain(module_obj, module_val);
        assert(res == NO_ERR);

        val_add_child(module_val, dst_val);

        if(0==strcmp(ncx_get_modname(mod), NCXMOD_NETCONF)) {
            /* ietf-netconf is overloaded internally */
            val_value_t* newval;
            val_value_t* curval;

            /* name */
            newval = agt_make_leaf(module_obj,
                                 "name",
                                 NCXMOD_IETF_NETCONF,
                                 &res);
            assert(res==NO_ERR && newval);
            curval = val_find_child(module_val, "ietf-yang-library", "name");
            assert(curval);
            val_swap_child(newval,curval);
            val_free_value(curval);

            /* revision */
            newval = agt_make_leaf(module_obj,
                                 "revision",
                                 NCXMOD_IETF_NETCONF_REVISION,
                                 &res);
            assert(res==NO_ERR && newval);
            curval = val_find_child(module_val, "ietf-yang-library", "revision");
            assert(curval);
            val_swap_child(newval,curval);
            val_free_value(curval);
        }
    }

    /* module-set-id */
    {
        unsigned int i;
        char* modules_state_str;
        unsigned char hash[SHA_DIGEST_LENGTH];
        unsigned char hash_str[SHA_DIGEST_LENGTH*2+1];
        res=val_make_serialized_string(dst_val, NCX_DISPLAY_MODE_XML, (xmlChar **)&modules_state_str);
        assert(res==NO_ERR);
        SHA1(modules_state_str, strlen(modules_state_str), hash);
        free(modules_state_str);
        for(i=0;i<SHA_DIGEST_LENGTH;i++) {
            sprintf(hash_str + (i*2), "%02x", hash[i]);
        }
        hash_str[SHA_DIGEST_LENGTH*2]=0;
        module_set_id_val = agt_make_leaf(ietf_yang_library_modules_state_obj,
                                            "module-set-id",
                                            hash_str,
                                            &res);
        assert(res==NO_ERR);
        val_add_child(module_set_id_val, dst_val);

        return NO_ERR;
    }
}

/************* E X T E R N A L    F U N C T I O N S ***************/


/********************************************************************
* FUNCTION agt_yang_library_init
*
* INIT 1:
*   Initialize the server notification module data structures
*
* INPUTS:
*   none
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_yang_library_init (void)
{
    agt_profile_t  *agt_profile;
    status_t        res;

    agt_profile = agt_get_profile();

    /* load the module */
    res = ncxmod_load_module("ietf-yang-library", 
                             "2016-06-21",
                             &agt_profile->agt_savedevQ,
                             &ietf_yang_library_mod);
    assert(res == NO_ERR);

    /* find the object definition for the modules-state container */
    ietf_yang_library_modules_state_obj = ncx_find_object(ietf_yang_library_mod,
                                ietf_yang_library_N_modules_state);

    assert(ietf_yang_library_modules_state_obj);

    return NO_ERR;

}  /* agt_yang_library_init */


/********************************************************************
* FUNCTION agt_yang_library_init2
*
* INIT 2:
*   Initialize the yang library data structures
*
* INPUTS:
*   none
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_yang_library_init2 (void)
{
    val_value_t           *ietf_yang_library_modules_state_val;
    cfg_template_t        *runningcfg;
    status_t               res;

    /* get the running config */
    runningcfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    if (!runningcfg || !runningcfg->root) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* add /system-state */
    ietf_yang_library_modules_state_val = val_new_value();
    assert(ietf_yang_library_modules_state_val);

    val_init_virtual(ietf_yang_library_modules_state_val,
                     get_modules_state,
                     ietf_yang_library_modules_state_obj);

    /* handing off the malloced memory here */
    val_add_child_sorted(ietf_yang_library_modules_state_val, runningcfg->root);

    return NO_ERR;

}  /* agt_yang_library_init2 */


/********************************************************************
* FUNCTION agt_yang_library_cleanup
*
* Cleanup the module data structures
*
* INPUTS:
*   
* RETURNS:
*   none
*********************************************************************/
void 
    agt_yang_library_cleanup (void)
{
}  /* agt_yang_library_cleanup */

/* END file agt_yang_library.c */
