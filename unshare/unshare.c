#include <sys/types.h>
#include <sys/mount.h>
#include <string.h>
#include <pwd.h>
#include <sched.h>

#include <slurm/spank.h>
#include <slurm/slurm.h>


/*
 * All spank plugins must define this macro for the SLURM plugin loader.
 */
SPANK_PLUGIN(unshare, 1)
static char *fs_licenses;
static char *fs_mountpoint;


/* 
 * Called from both srun and slurmd.
 */
int slurm_spank_init (spank_t sp, int ac, char **av)
{
    char *licenses=NULL;
    char *coma=NULL;
    uint32_t jobid;
    job_info_msg_t * job_buffer_ptr;
    struct passwd *pw;
    uid_t uid;

    if (ac!=2) {
        slurm_error ("unshare: Error module need two arguments ' licenses mountpoint ' ");
        return (-1);
    }

    if (!spank_remote (sp))
        return (0);

    slurm_verbose("unshare: av[0] == %s ",av[0] );
    fs_licenses=av[0];
    slurm_verbose("unshare: av[1] == %s ",av[1] );
    fs_mountpoint=av[1];

    spank_get_item (sp, S_JOB_UID, &uid);
    pw = getpwuid (uid);
    if (!pw) {
        slurm_error ("unshare: Error looking up uid in /etc/passwd");
        return (-1);
    }
    if (uid == 0 ) {
        slurm_verbose("unshare: User Root Skip" );
        return (0);
    }

    /* get job id */

    if (unshare (CLONE_NEWNS) < 0) {
        slurm_error ("unshare: Error unshare CLONE_NEWNS: %m");
        return (-1);
    }

    if ( spank_get_item (sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS ) {
        slurm_error ("unshare: Error unable to get jobid");
        return (-1);
    }
    if ( slurm_load_job(&job_buffer_ptr,jobid,SHOW_ALL) != 0 ) {
        slurm_error ("unshare: Error unable slurm_load_job");
        return (-1);
    }

    licenses=job_buffer_ptr->job_array->licenses;
    slurm_verbose("unshare: licenses  = %s ", licenses );
    if ( licenses ) 
        coma= strchr(licenses, ',' );
    
    while( coma  || licenses) {
       if(coma) {
            if (strncmp(fs_licenses, licenses, coma - licenses) == 0 && coma - licenses == strlen(fs_licenses)){
                slurm_free_job_info_msg(job_buffer_ptr);
                return 0;     
            }
       } else if (strcmp(fs_licenses, licenses) == 0){
           slurm_free_job_info_msg(job_buffer_ptr);
           return 0;     
       }
       if ( (licenses = coma)) {
           licenses ++;
           coma= strchr(licenses, ',' );
       }
    }
    slurm_free_job_info_msg(job_buffer_ptr);

    if (umount(fs_mountpoint) != 0) {
        slurm_error ("unshare: Could not unmount %s",fs_mountpoint);
        return (-1);
    }

    return (0);
}

/* 
 * Called from both srun and slurmd.
 */
int slurm_spank_exit (spank_t sp, int ac, char **av)
{
    return (0);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
