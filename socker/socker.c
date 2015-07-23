#include <sys/types.h>
#include <sys/mount.h>
#include <string.h>
#include <pwd.h>
#include <sched.h>
#include <unistd.h>

#include <slurm/spank.h>
#include <slurm/slurm.h>


/*
 * All spank plugins must define this macro for the SLURM plugin loader.
 */
SPANK_PLUGIN(socker, 1)


/* 
 * Called from both srun and slurmd.
 */
int slurm_spank_init (spank_t sp, int ac, char **av)
{
    struct passwd *pw;
    uid_t uid;


    if (!spank_remote (sp))
        return (0);

    spank_get_item (sp, S_JOB_UID, &uid);
    pw = getpwuid (uid);
    if (!pw) {
        slurm_error ("socker: Error looking up uid in /etc/passwd");
        return (-1);
    }
    if (uid != 4794 ) {
        slurm_verbose("socker: User No bart Skip" );
        return (0);
    }

    if (unshare (CLONE_NEWNS|CLONE_NEWPID) < 0) {
        slurm_error ("socker: Error unshare CLONE_NEWNS: %m");
        return (-1);
    }

    if (chroot("/icm/hydra/home/admins/bart/X") < 0) {
        slurm_error ("socker: Error unshare CLONE_NEWNS: %m");
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
