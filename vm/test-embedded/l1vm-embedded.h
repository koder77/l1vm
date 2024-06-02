// l1vm-embedded.h
// headers for L1VM enmbedded protos

int l1vm_run_program (char *program_name, int ac, char *av[]);
long long int l1vm_get_global_data_size (void);
unsigned char *l1vm_get_global_data (void );
void l1vm_cleanup (void);
