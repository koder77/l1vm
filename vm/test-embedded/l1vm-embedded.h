// l1vm-embedded.h
// headers for L1VM enmbedded protos

int l1vm_run_program (char *program_name, int ac, char *av[]);
long long int l1vm_get_global_data_size (void);
unsigned char *l1vm_get_global_data (void );
void l1vm_cleanup (void);

S2 memory_bounds (S8 start, S8 offset_access);

// mem.c
// read
S2 read_data_byte (S8 addr, U1 *ret);
S2 read_data_int16 (S8 addr, S2 *ret);
S2 read_data_int32 (S8 addr, S4 *ret);
S8 read_data_int64 (S8 addr, S4 *ret);
S2 read_data_double (S8 addr, F8 *ret);

// write
S2 write_data_byte (S8 addr, U1 data);
S2 write_data_int16 (S8 addr, S2 *data);
S2 write_data_int32 (S8 addr, S4 *data);
S2 write_data_int64 (S8 addr, S8 *data);
S2 write_data_double (S8 addr, F8 *data);
