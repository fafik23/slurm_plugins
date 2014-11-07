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
const char plugin_name[]         = "Job submit valid mail";
const char plugin_type[]         = "job_submit/valid_mail";
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
/*
Taken from:
Secure Cooking with C and C++, Part 3 - O'Reilly Media
http://www.onjava.com/pub/a/network/excerpt/spcookbook_chap03/index3.html
*/
int _spc_email_isvalid(const char *address) {
  int        count = 0;
  const char *c, *domain;
  static char *rfc822_specials = "()<>@,;:\\\"[]";

  /* first we validate the name portion (name@domain) */
  for (c = address;  *c;  c++) {
    if (*c == '\"' && (c == address || *(c - 1) == '.' || *(c - 1) == 
        '\"')) {
      while (*++c) {
        if (*c == '\"') break;
        if (*c == '\\' && (*++c == ' ')) continue;
        if (*c <= ' ' || *c >= 127) return 0;
      }
      if (!*c++) return 0;
      if (*c == '@') break;
      if (*c != '.') return 0;
      continue;
    }
    if (*c == '@') break;
    if (*c <= ' ' || *c >= 127) return 0;
    if (strchr(rfc822_specials, *c)) return 0;
  }
  if (c == address || *(c - 1) == '.') return 0;

  /* next we validate the domain portion (name@domain) */
  if (!*(domain = ++c)) return 0;
  do {
    if (*c == '.') {
      if (c == domain || *(c - 1) == '.') return 0;
      count++;
    }
    if (*c <= ' ' || *c >= 127) return 0;
    if (strchr(rfc822_specials, *c)) return 0;
  } while (*++c);

  return (count >= 1);
}

int _spc_email_isdefault(const char *address) {
  if(address) {
    if(strncmp(address,"user@email.domain",17)==0)
      return 1;
  }
  return 0;
}
extern int job_submit(struct job_descriptor *job_desc, uint32_t submit_uid,
          char **err_msg)
{
  static char       *msg = NULL;

  info("valid_mail: mail:%s mail_type:%u name:%s partition:%s submit_uid:%u",
       job_desc->mail_user, job_desc->mail_type, job_desc->name, job_desc->partition, submit_uid );

  if (job_desc->mail_type == 0 || job_desc->mail_user == NULL) {
    debug( "valid_mail: No mail spec or mail_type == 0");
    return SLURM_SUCCESS;
  }
  if (job_desc->mail_user){
    if (_spc_email_isdefault(job_desc->mail_user)) {
      debug( "valid_mail:  mail == user@email.domain is default - Job has been rejected");
      if (err_msg) {
        msg=xstrdup("You specify wrong email address. Job has been rejected.'");
        *err_msg = msg;
      }
      return SLURM_ERROR;
    }
    if ( !( _spc_email_isvalid(job_desc->mail_user))) {
      debug( "valid_mail:  mail is invalid - Job has been rejected");
      if (err_msg) {
        msg=xstrdup("You specify wrong email address. Job has been rejected.'");
        *err_msg = msg;
      }
      return SLURM_ERROR;
    }
  }

  return SLURM_SUCCESS;
 
}

extern int job_modify(struct job_descriptor *job_desc,
          struct job_record *job_ptr, uint32_t submit_uid)
{
  /*
	job_modify
  */
  debug3( "valid_mail: job_modify");
  return SLURM_SUCCESS;
}
