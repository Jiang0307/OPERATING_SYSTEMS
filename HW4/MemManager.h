#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct REPLACEMENT
{
    char process;
    int VPN;
    int reference;
    struct REPLACEMENT* next;
} REPLACEMENT;

typedef struct PAGE
{
    int PFN_DBI;
    int present;
    REPLACEMENT *ptr;
} PAGE;

typedef struct TLB
{
    int VPN;
    int PFN;
} TLB;