slurm_plugins
=============
gcc job_submit_plg_grant.c -o job_submit_plg_grant.so -shared  -I ../../../../ -DHAVE_STDBOOL_H -DLDAP_DEPRECATED -DPIC -fPIC -lldap;  cp -f job_submit_plg_grant.so /usr/local/slurm/14.03.3-2/lib/slurm
