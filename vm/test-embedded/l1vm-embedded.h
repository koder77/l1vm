// l1vm-embedded.h
// headers for L1VM enmbedded protos

int run_program (char *program_name, int ac, char *av[]);
long long int get_global_data_size (void);
unsigned char *get_global_data (void );
void cleanup (void);
