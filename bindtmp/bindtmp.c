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
SPANK_PLUGIN(bindtmp, 1)
static char *lls_mountpoint_prefix;


/* 
 * Called from both srun and slurmd.
 */
int slurm_spank_init (spank_t sp, int ac, char **av)
{
    uint32_t jobid;
    struct passwd *pw;
    uid_t uid;
    char buf [1024];
    int n;

    if (ac!=1) {
        slurm_error ("bindtmp: Error module need argument ' lls_mountpoint_prefix ' ");
        return (-1);
    }
    if (!spank_remote (sp))
        return (0);

    slurm_verbose("bindtmp: av[0] == %s ",av[0] );
    lls_mountpoint_prefix=av[0];

    spank_get_item (sp, S_JOB_UID, &uid);
    pw = getpwuid (uid);
    if (!pw) {
        slurm_error ("bindtmp: Error looking up uid in /etc/passwd");
        return (-1);
    }

    if (unshare (CLONE_NEWNS) < 0) {
        slurm_error ("bindtmp: Error unshare CLONE_NEWNS: %m");
        return (-1);
    }

    if ( spank_get_item (sp, S_JOB_ID, &jobid) != ESPANK_SUCCESS ) {
        slurm_error ("bindtmp: Error unable to get jobid");
        return (-1);
    }
    n = snprintf (buf, sizeof (buf), "%s%u",lls_mountpoint_prefix,  jobid);
    if ((n < 0) || (n > sizeof (buf) - 1)) {
        slurm_error ("bindtmp: Error sprintf ");
        return (-1);
    }
    if (mount(buf, "/tmp", NULL, MS_BIND , NULL)!= 0) {
        slurm_error ("bindtmp: Could not bind %s to /tmp",buf);
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
