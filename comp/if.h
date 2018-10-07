#define MAXJUMPLIST 1024
#define MAXJUMPNAME 64
#define MAXIF 1024
#define EMPTY -1
#define NOTDEF -1
#define IF_FINISHED 2

struct if_comp
{
    U1 used;
    S4 else_pos;
    S4 endif_pos;
    U1 else_name[MAXJUMPNAME + 1];
    U1 endif_name[MAXJUMPNAME + 1];
};

struct jumplist
{
    U1 islabel;
    U1 lab[MAXJUMPNAME + 1];
    S4 pos;                                 /* position in exelist */
    S4 source_pos;                          /* position in sourcecode */
};
