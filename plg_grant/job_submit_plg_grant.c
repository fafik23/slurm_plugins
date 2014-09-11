/*****************************************************************************\
Author: Dominik Bartkiewicz (Interdycyplinary Center for Mathematical 
  and Computational Modeling. University of Warsaw)
\*****************************************************************************/

#if HAVE_CONFIG_H
#  include "config.h"
#  if STDC_HEADERS
#    include <string.h>
#  endif
#  if HAVE_SYS_TYPES_H
#    include <sys/types.h>
#  endif /* HAVE_SYS_TYPES_H */
#  if HAVE_UNISTD_H
#    include <unistd.h>
#  endif
#  if HAVE_INTTYPES_H
#    include <inttypes.h>
#  else /* ! HAVE_INTTYPES_H */
#    if HAVE_STDINT_H
#      include <stdint.h>
#    endif
#  endif /* HAVE_INTTYPES_H */
#else /* ! HAVE_CONFIG_H */
#  include <sys/types.h>
#  include <unistd.h>
#  include <stdint.h>
#  include <string.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/slurm_xlator.h"
#include "src/slurmctld/slurmctld.h"



#include <ldap.h>
#include <ctype.h>
#include <sys/time.h>
#include <pwd.h>

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  SLURM uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *  <application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "auth" for SLURM authentication) and <method> is a
 * description of how this plugin satisfies that application.  SLURM will
 * only load authentication plugins if the plugin_type string has a prefix
 * of "auth/".
 *
 * plugin_version   - specifies the version number of the plugin.
 * min_plug_version - specifies the minumum version number of incoming
 *                    messages that this plugin can accept
 */
const char plugin_name[]         = "Job submit PLGrid grant";
const char plugin_type[]         = "job_submit/plg_grant";
const uint32_t plugin_version   = 110;
const uint32_t min_plug_version = 100;

/*****************************************************************************\
 * We've provided a simple example of the type of things you can do with this
 * plugin. If you develop another plugin that may be of interest to others
 * please post it to slurm-dev@schedmd.com  Thanks!
\*****************************************************************************/

int init()
{  
  return SLURM_SUCCESS;
}


int fini()
{
  return SLURM_SUCCESS;
}



extern int job_submit(struct job_descriptor *job_desc, uint32_t submit_uid,
          char **err_msg)
{
  struct passwd *pw=NULL;
  char plg_part_prefix[]="plgrid";
  int                 ret          = SLURM_ERROR;
  int                 rc          = 0;
  int                 i          = 0;
  char              *grant_uid = NULL;
  LDAP              *ld_handle   = NULL;
  LDAPMessage       *e=NULL, *result = NULL;
  char              *ldap_uri = "ldap://149.156.10.32:29413";
  //char              *ldap_uri = "ldap://192.168.69.110";
  char              *ldap_base = "dc=icm,dc=plgrid,dc=pl";
  char              *ldap_filtr = NULL;
  char              *ldap_dn = NULL;
  static char       *msg = NULL;
  char         *a = NULL ;
  char        *ldap_attrs[]={"plgridDefaultGrantID","plgridOLAStatus","plgridGrantType",NULL};
  BerElement   *ber = NULL;
  struct berval *val = NULL; 
  struct berval **v = NULL;
  static struct timeval ldap_timeout =
  {
      (time_t) 2,
      (suseconds_t) 0
  };


  info("plg_grant: job_submit: account:%s begin_time:%ld dependency:%s "
       "name:%s partition:%s qos:%s submit_uid:%u time_limit:%u "
       "user_id:%u, mem:%d",
       job_desc->account, (long)job_desc->begin_time,
       job_desc->dependency,
       job_desc->name, job_desc->partition, job_desc->qos,
       submit_uid, job_desc->time_limit, job_desc->user_id,
       job_desc->pn_min_memory  );

  if(job_desc->partition == NULL)  {
    error( "plg_grant: Error No partition");
    return SLURM_ERROR;
  }
  if (strncmp(job_desc->partition,plg_part_prefix,6)!=0)  
    return SLURM_SUCCESS;

  if(submit_uid==0)
    return SLURM_SUCCESS;

  pw = getpwuid (submit_uid);
  if (!pw) {
    error( "plg_grant: Error looking up uid=%u in getpwuid",submit_uid);
    ret=ESLURM_USER_ID_MISSING;
    goto fail_plg;
  }
  //ldap_initialize//
  if (ldap_initialize(&ld_handle, ldap_uri) != LDAP_SUCCESS ) {
    error( "plg_grant: job_submit: no handle to LDAP server  %s ", ldap_uri);
    goto fail_plg;
  }
  int version = LDAP_VERSION3;
  ldap_set_option( ld_handle, LDAP_OPT_PROTOCOL_VERSION, &version );
  rc = ldap_simple_bind_s(ld_handle, NULL, NULL);
  if (rc != LDAP_SUCCESS) {
    error( "plg_grant: Error binding to ldap server with ldap_simple_bind_s(%s,NULL,NULL) ->%s", ldap_uri,ldap_err2string(rc));
    goto fail_plg;
  }
  //Set account from plgridDefaultGrantID//
  if(job_desc->account == NULL)  {
    if ( (ldap_filtr = (char *) xmalloc((strlen("(&(objectClass=plgridOrgPerson)(uid=))") + strlen(pw->pw_name) + 1) * sizeof(char))) == NULL ) {
      error( "plg_grant: Memory could not be allocated for ldap_filtr");
      goto fail_plg;
    }
    sprintf(ldap_filtr,"(&(objectClass=plgridOrgPerson)(uid=%s))",pw->pw_name);

    if(ldap_timeout.tv_sec > 0) {
      rc = ldap_search_st( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, ldap_attrs, 0, &ldap_timeout ,&result );
    } else {
      rc = ldap_search_s( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, ldap_attrs, 0,  &result );
    }
    xfree(ldap_filtr);
    ldap_filtr = NULL;
    if ( rc != LDAP_SUCCESS ) {
      error( "plg_grant: ldap_search_s %s",ldap_err2string(rc));
      goto fail_plg;
    }
    if( (e = ldap_first_entry( ld_handle, result )) != NULL ) {
      if ( (ldap_dn = ldap_get_dn( ld_handle, e )) != NULL ) {
        info("plg_grant: LDAP dn: %s", ldap_dn );
        ldap_memfree( ldap_dn );
      }

      v = ldap_get_values_len(ld_handle, e, "plgridDefaultGrantID");
      if ( v[0]!=NULL ) {
        val=v[0];
        job_desc->account=xstrndup(val->bv_val,val->bv_len);
        info("plg_grant: Set account : %s", job_desc->account );
      }
      ber_bvecfree(v);
      v=NULL;
      
    } else {
      error( "plg_grant: ldap_search_s return 0 entry") ; 
      ret=ESLURM_INVALID_ACCOUNT;
      goto fail_plg;
    }
    if( result ) ldap_msgfree( result );
    result=NULL;
  }  

  //Check account is ACTIVE//
  //Check user is allowed to use account//
  if(job_desc->account != NULL)  {

    if ( (ldap_filtr = (char *) xmalloc((strlen("(&(objectClass=plgridOLA)(plgridGrantID=))") + strlen(job_desc->account) + 1) * sizeof(char))) == NULL ) {
      error( "plg_grant: Memory could not be allocated for ldap_filtr");
      goto fail_plg;
    }
    sprintf(ldap_filtr,"(&(objectClass=plgridOLA)(plgridGrantID=%s))",job_desc->account);

    if(ldap_timeout.tv_sec > 0) {
      rc = ldap_search_st( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, ldap_attrs, 0, &ldap_timeout ,&result );
    } else {
      rc = ldap_search_s( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, ldap_attrs, 0,  &result );
    }
    xfree(ldap_filtr);
    ldap_filtr = NULL;
    if ( rc != LDAP_SUCCESS ) {
      error( "plg_grant: ldap_search_s %s",ldap_err2string(rc));
      goto fail_plg;
    }
    if( (e = ldap_first_entry( ld_handle, result )) != NULL ) {
      if ( (ldap_dn = ldap_get_dn( ld_handle, e )) != NULL ) {
        info("plg_grant: LDAP dn: %s", ldap_dn );
        ldap_memfree( ldap_dn );
      }

      v = ldap_get_values_len(ld_handle, e, "plgridOLAStatus");
      if ( v[0]!=NULL ) {
        val=v[0];
        info("plg_grant: account : %s is %s", job_desc->account, val->bv_val);
        if (strcmp(val->bv_val,"ACTIVE")!=0){
          msg=xstrdup("Grant specified with this ID (ID_grantu) is not yet active or has expired. Job has been rejected.");
          ret=ESLURM_INVALID_ACCOUNT;
          goto fail_plg;
        }
      }

      ber_bvecfree(v);
      v=NULL;
      //Check user is allowed to use account//
      v = ldap_get_values_len(ld_handle, e, "plgridGrantType");

      if( result ) ldap_msgfree( result );
      result =NULL;

      if ( v[0]!=NULL ) {
        val=v[0];
        info("plg_grant: account : %s type is %s", job_desc->account, val->bv_val);
        if (strcmp(val->bv_val,"personal")==0){
          if ( (ldap_dn = ldap_get_dn( ld_handle, e )) != NULL ) {
            info("plg_grant: LDAP dn: %s", ldap_dn );
            if ( (grant_uid = (char *) xmalloc((strlen(",uid=,") + strlen(pw->pw_name) + 1) * sizeof(char))) == NULL ) {
              error( "plg_grant: Memory could not be allocated for grant_uid");
              goto fail_plg;
            }
            sprintf(grant_uid,",uid=%s,",pw->pw_name) ;
            if(strstr(ldap_dn,grant_uid)==NULL) {
              error( "plg_grant: personal grant DN dont match UID");
              xfree(grant_uid);
              grant_uid = NULL;
              ldap_memfree( ldap_dn );
              ldap_dn =NULL;
              msg=xstrdup("You are not a member of group allowed to use this grant ID (ID_grantu). Job has been rejected.'");
              ret=ESLURM_INVALID_ACCOUNT;
              goto fail_plg;
            }
            xfree(grant_uid);
            grant_uid = NULL;
            ldap_memfree( ldap_dn );
            ldap_dn =NULL;
          } else {
             goto fail_plg;
          }
        } else { //Granty Wlaciwe
          if ( (ldap_filtr = (char *) xmalloc((strlen("(&(objectClass=plgridGroup)(plgridGrantID=)(memberUid=))") + strlen(job_desc->account) + strlen(pw->pw_name) + 1) * sizeof(char))) == NULL ) {
            error( "plg_grant: Memory could not be allocated for ldap_filtr");
            goto fail_plg;
          }
          sprintf(ldap_filtr,"(&(objectClass=plgridGroup)(plgridGrantID=%s)(memberUid=%s))",job_desc->account,pw->pw_name);

          if(ldap_timeout.tv_sec > 0) {
            rc = ldap_search_st( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, NULL, 0, &ldap_timeout ,&result );
          } else {
            rc = ldap_search_s( ld_handle, ldap_base, LDAP_SCOPE_SUBTREE, ldap_filtr, NULL , 0,  &result );
          }
          xfree(ldap_filtr);
          ldap_filtr = NULL;
          if ( rc != LDAP_SUCCESS ) {
            error( "plg_grant: ldap_search_s %s",ldap_err2string(rc));
            goto fail_plg;
          }         
          if( ldap_count_entries( ld_handle, result ) < 1 ) {
            error( "plg_grant: Uid no in grant group");
            msg=xstrdup("You are not a member of group allowed to use this grant ID (ID_grantu). Job has been rejected.'");
            ret=ESLURM_INVALID_ACCOUNT;
            goto fail_plg;
          }
        }
      }
    } else {
      error( "plg_grant: 0 entry");
      ret=ESLURM_INVALID_ACCOUNT;
      msg=xstrdup("Grant specified with this ID (ID_grantu) does not exist. Job has been rejected.");
      goto fail_plg;
    }
  }
 
  success_plg:
    ber_bvecfree(v);
    v=NULL;
    if( result ) ldap_msgfree( result );
    result=NULL;
    if (ld_handle) ldap_unbind_s(ld_handle);
    return SLURM_SUCCESS;

  fail_plg:
    ber_bvecfree(v);
    v=NULL;
    if( result ) ldap_msgfree( result );
    result=NULL;
    info( "plg_grant: test %p",v);
    if (ld_handle) ldap_unbind_s(ld_handle);

    if (msg) {
      if (err_msg) {
        *err_msg = msg;
        msg = NULL;
      } else
        xfree(msg);
    }
    return ret;
 
}

extern int job_modify(struct job_descriptor *job_desc,
          struct job_record *job_ptr, uint32_t submit_uid)
{
  /*
	job_modify
  */
  info( "plg_grant: job_modify");
  char plg_part_prefix[]="plgrid";
  if(submit_uid==0) {
    return SLURM_SUCCESS;
  }
  if (strncmp(job_desc->partition,plg_part_prefix,6)!=0 && strncmp( job_ptr->partition,plg_part_prefix,6)!=0) {
    return SLURM_SUCCESS;
  }
  if (strcmp(job_desc->account,job_ptr->account)!=0) {
    return SLURM_ERROR;
  }
  return SLURM_SUCCESS;
}
