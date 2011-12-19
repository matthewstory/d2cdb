/* @author: Jacob Yuan <jacob@tablethotels.com>

   Based on Matthew Story's d2cdb (Bourne shell) */

#include <cdb.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fts.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

/* To build: gcc -Wall d2cdb.c -I/usr/local/include -L/usr/local/lib -lcdb -std=c99 -o <executable>
   For debug output, append: -DDEBUG */

const char *usage = "usage: %s [-amorth] cdb_file tmp_file directory [directory ...]";
const char *long_usage =
"usage: %s [-amorth] cdb_file tmp_file directory [directory ...]\n"
"  a : include directory entries whose names begin with a dot (`.')\n"
"  m : remove the file name from the cdb key\n"
"  o : replace earlier cdb entries if the keys are identical\n"
"  r : reverse the directory traversal order (implies `-o')\n"
"  t : unused option for compatibility\n"
"  h : show this help";

typedef struct {
    char *cdbfile;
    char *tmpfile;
    char **dirs;
    int includedot;
    int multikey;
    int override;
    int reverse_override;
    int dirs_argv_idx;
} t_argopts;
t_argopts args = {.cdbfile = NULL, .tmpfile = NULL, .dirs = NULL, \
                  .includedot = 0, .multikey = 0, .override = 0, \
                  .reverse_override = 0, .dirs_argv_idx = 0};

int fd = 0;
FTS *tree = NULL;
int src_dir_len = 0;

#ifndef EXIT_SUCC
#define EXIT_SUCC 0
#endif
/* #ifndef EXIT_TEMP
#define EXIT_TEMP 111
#endif */
#ifndef EXIT_FAIL
#define EXIT_FAIL 100
#endif

int parse_args(const int, char **, t_argopts *);
int cdb_add(struct cdb_make *, const FTSENT *, const int *, const int *);
int get_inode(const char *);
void print_varargs(const char *, ...);
void err_exit(const int, const char *, ...);
void print_err(const int, const char *, va_list);
int fts_cmp(const FTSENT * const *, const FTSENT * const *);

int parse_args(const int argc, char **argv, t_argopts *args) {
    const int min_args = 4;
    int c = 0;

    while((c = getopt(argc, argv, "amorth")) != -1) {
        switch(c) {
            case 'a':
                args->includedot = 1;
                break;
            case 'm':
                args->multikey = 1;
                break;
            case 'r':
                args->reverse_override = 1;
            case 'o':
                args->override = 1;
                break;
            case 't':
                break;
            case 'h':
                return -1;
            case '?':
                return 1;
            default:
                #ifdef DEBUG
                fprintf(stderr, "DEBUG: getopt option error: 0x%x\n", optopt);
                #endif
                return 1; // break
        }
    }

    if((argc < min_args) || (argc - optind < min_args - 1))
        return 1;

    for(int i = 2; argv[optind + i] != NULL; i++) {
        struct stat sbuf;
        if(stat(argv[optind + i], &sbuf) == -1)
            err_exit(1, "fatal: %s stat error", argv[optind + i]);
        if(!S_ISDIR(sbuf.st_mode))
            return 1;
    }

    args->cdbfile = argv[optind];
    args->tmpfile = argv[optind + 1];
    args->dirs = argv + optind + 2;
    args->dirs_argv_idx = optind + 2;
    return 0;
}

int cdb_add(struct cdb_make *cdbm, const FTSENT *node, const int *multikeys, const int *override) {
    char *key = NULL;
    int klen = 0;

    int fsize = node->fts_statp->st_size;
    char *fbuf = malloc((fsize + 1) * sizeof(char));
    if(!fbuf) {
        err_exit(1, "fatal: malloc error (size: %i)", (fsize + 1) * sizeof(char));
    }

    FILE *fp = NULL;
    if((fp = fopen(node->fts_path, "r")) == NULL) {
        free(fbuf);
        err_exit(1, "fatal: %s fopen error", node->fts_path);
    }

    int nread = fread(fbuf, sizeof(char), fsize, fp);
    if(nread != fsize) {
        fclose(fp);
        free(fbuf);
        err_exit(0, "fatal: %s fread error", node->fts_path);
    }
    fbuf[fsize] = '\0';

    if(fclose(fp)) {
        free(fbuf);
        err_exit(1, "fatal: %s fclose error", node->fts_path);
    }

    // remove the source directory and the trailing path separator for the key
    key = node->fts_path + src_dir_len + 1;
    klen = node->fts_pathlen - (src_dir_len + 1);

    if(*multikeys && node->fts_level > 1) {
        // remove the file name and path separator
        *(key + klen - (node->fts_namelen + 1)) = '\0';
        klen -= (node->fts_namelen + 1);
    }

    int ret = 0;
    if(*override)
        ret = cdb_make_put(cdbm, key, klen, fbuf, fsize, CDB_PUT_REPLACE);
    else
        ret = cdb_make_add(cdbm, key, klen, fbuf, fsize);

    free(fbuf);
    return ret;
}

int get_inode(const char *fn) {
    struct stat sbuf;
    if(stat(fn, &sbuf))
        err_exit(1, "fatal: %s stat error", fn);
    return sbuf.st_ino;
}

void print_varargs(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    return;
}

void err_exit(const int perr, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    print_err(perr, fmt, ap);
    va_end(ap);

    if(tree)
        fts_close(tree);
    if(fd)
        close(fd);
    if(args.tmpfile && unlink(args.tmpfile))
        fprintf(stderr, "fatal: %s unlink error: %s\n", args.tmpfile, strerror(errno));

    exit(EXIT_FAIL);
}

void print_err(const int perr, const char *fmt, va_list ap) {
    char s[LINE_MAX + 1];
    vsnprintf(s, LINE_MAX + 1, fmt, ap);
    if(perr)
        snprintf(s + strlen(s), LINE_MAX + 1 - strlen(s), ": %s", strerror(errno));

    fflush(stdout);
    fputs(s, stderr);
    fputs("\n", stderr);
    fflush(NULL);
    return;
}

int fts_cmp(const FTSENT * const *a, const FTSENT * const *b) {
    /* int path_cmp = strcmp((*a)->fts_path, (*b)->fts_path);
    if(path_cmp != 0)
        return path_cmp; */
    return strcmp((*a)->fts_name, (*b)->fts_name);
}

int main(int argc, char **argv) {
    int r = parse_args(argc, argv, &args);
    if(r == 1) {
        print_varargs("%s", usage, argv[0]);
        exit(EXIT_SUCC);
    } else if(r == -1) {
        print_varargs("%s", long_usage, argv[0]);
        exit(EXIT_SUCC);
    }

    if(args.reverse_override) {
        int ndirs = argc - args.dirs_argv_idx;
        for(int i = 0; i < ndirs / 2; i++) {
            char *tmp = argv[args.dirs_argv_idx + i];
            argv[args.dirs_argv_idx + i] = argv[argc - 1 - i];
            argv[argc - 1 - i] = tmp;
        }
    }

    struct cdb_make cdbm = {};
    if((fd = open(args.tmpfile, (O_RDWR | O_CREAT))) == -1)
        err_exit(1, "fatal: %s open error", args.tmpfile);
    if(cdb_make_start(&cdbm, fd))
        err_exit(0, "fatal: cdb_make_start error");

    const int cdbtemp_ino = get_inode(args.tmpfile);
    for(int i = 0; args.dirs[i] != NULL; i++) {
        char path_buf[PATH_MAX + 1];
        if(realpath(args.dirs[i], path_buf) == NULL)
            err_exit(0, "fatal: %s realpath error", args.dirs[i]);

        char *src_dir[] = {path_buf, NULL};
        src_dir_len = strlen(src_dir[0]);

        // note: fts_open returns a non-NULL for an invalid dir
        int (*cmp_func)(const FTSENT * const *, const FTSENT * const *) = &fts_cmp;
        // tree = fts_open(src_dir, FTS_LOGICAL, cmp_func);
        tree = fts_open(src_dir, (FTS_LOGICAL | FTS_NOCHDIR), cmp_func);
        if(!tree)
            err_exit(1, "fatal: %s fts_open error", src_dir[0]);

        FTSENT *node = NULL;
        while((node = fts_read(tree))) {
            if((node->fts_info & (FTS_DNR | FTS_ERR | FTS_NS)) && node->fts_errno)
                err_exit(1, "fatal: %s fts_read error", node->fts_path);

            if(!args.includedot && node->fts_level > 0 && node->fts_name[0] == '.') {
                #ifdef DEBUG
                fprintf(stderr, "DEBUG: skipping %s at level %li\n", \
                        node->fts_name, node->fts_level);
                #endif
                if(fts_set(tree, node, FTS_SKIP))
                    err_exit(0, "fatal: %s fts_set error", node->fts_path);
            } else if(node->fts_info & FTS_F) {
                if(get_inode(node->fts_path) == cdbtemp_ino) {
                    #ifdef DEBUG
                    fprintf(stderr, "DEBUG: skipping %s\n", node->fts_path);
                    #endif
                    if(fts_set(tree, node, FTS_SKIP))
                        err_exit(0, "fatal: %s fts_set error", node->fts_path);

                    continue;
                }

                int ret = cdb_add(&cdbm, node, &args.multikey, &args.override);
                if(ret && ret != 1)
                    err_exit(0, "fatal: %s cdb_add error", node->fts_path);
            }
        }
        if(errno)
            err_exit(1, "fatal: fts_read error");

        if(fts_close(tree))
            err_exit(1, "fatal: %s fts_close error", src_dir[0]);
        tree = NULL;
    }

    if(cdb_make_finish(&cdbm))
        err_exit(0, "fatal: cdb_make_finish error");
    if(close(fd))
        err_exit(1, "fatal: %s close error", args.tmpfile);
    fd = 0;

    if(rename(args.tmpfile, args.cdbfile))
        err_exit(1, "fatal: %s rename error", args.tmpfile);
    args.tmpfile = NULL;

    if(chmod(args.cdbfile, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)))
        err_exit(1, "fatal: %s chmod error", args.cdbfile);

    exit(EXIT_SUCC);
}
