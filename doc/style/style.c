/* zero example on code style */

/* include statements [usually] in the beginning of code files */
#include <stddef.h>
#include <string.h>

#define OPT_VERBOSE (1L << 0)
struct styleopt {
    long       flg;      // boolean option bits
    size_t     nfile;    // # of file names in fnametab
    char     **fnametab; // table of file name strings
};

struct styleopt styleopt;

int
stylegetopt(struct styleopt *opt, int argc, char *argv[])
{
    struct stat  statbuf;
    char        *str = argv[1];
    int          ndx = 1;

    while (ndx < argc) {
        if (str[0] == '-') {
            if (str[1] == '-') {
                if (!strcmp(str[2], "verbose")) {
                    opt->flg |= OPT_VERBOSE;
                }
            } else if (str[1] == 'v') {
                opt->flg |= OPT_VERBOSE;
            }
        } else {
            /* check existence and access permissions for files */
            /* stat() files and add buffers for regular ones */
        }
    }
}

int
main(int argc, char *argv)
{
    if (argc) {
        stylegetopt(&styleopt, argc, argv);
	}
}
