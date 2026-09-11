/*
 * 144-serac.c — the ceramic compiler: a map and some C functions in,
 * a program out.
 *
 * What this is: the one executable somebody needs in order to build a
 * program with this engine. It carries the generator, and it carries
 * the engine's own source as text, so the machine it runs on needs a C
 * compiler and nothing else — no copy of the engine, no header, no
 * export list, and no `main` anybody had to write.
 *
 * How it does it, in general terms: it reads the description, resolves
 * every box name against the C files it was handed, emits the
 * construction code the way the build-time generator always has, emits
 * a `main` that takes the program's arguments from the command line
 * and prints its results, and then **builds one piece of text in
 * memory** — the engine's header, the engine's body, the construction
 * code, the `main` — and hands that text to the C compiler down a
 * pipe.
 *
 * **Nothing is written to disk except the program.** That is the point
 * of the concatenation, and it is worth saying why it works. A
 * `#include` is a filesystem lookup: the preprocessor has to open a
 * file of that name, so a header that only exists inside this
 * executable forces a directory to exist somewhere with a copy of it
 * in it, and that directory was where three absolute paths from the
 * machine that built the binary used to come from. Placing the
 * header's text ahead of the body's and deleting the one line that
 * included it puts the same declarations in scope by the same rule,
 * with no file involved — the move the engine already made when eleven
 * sources became one. The linker's export list was the other file, and
 * naming the exported family in a flag does what handing over the file
 * did.
 *
 * The compiler is run without a shell, by handing an argument array
 * straight to the operating system. A compiler is a program whose
 * arguments are other people's file paths, and a path with a space in
 * it is not an error worth having.
 *
 * Usage:
 *   serac program.map boxes.c [more.c ...]   an executable, beside the map
 *   serac --shared program.map boxes.c       a shared object
 *   serac --emit-c program.map boxes.c       the C, and stop
 *   serac --unpack DIR                       cera.c, cera.h, the syms file
 *
 *   -o PATH        where the result lands, instead of beside the map
 *   --main=FILE    a C file carrying its own main, instead of the emitted one
 *   --root=DIR     what box paths are shortened against in generated symbols
 *   --cc=NAME      the C compiler to invoke
 *   --keep-c=PATH  also write out the text handed to the compiler
 *   --results=N    how many values of each result the program can hold
 */
#include "067-genparse.h"
#include "099-mapparse.h"
#include "143-serac.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Which compiler to invoke when nothing on the command line says. The
 * build defines this as the compiler that built `serac` itself, which
 * is the one whose idea of sizeof matches the engine text `serac` is
 * carrying. */
#ifndef SERAC_CC
#define SERAC_CC "cc"
#endif

/* {{{ static void fail() */
/*
 * Every refusal leaves through here, so they all read the same way and
 * so there is one place to look when asking what this program can
 * decline to do. It ends the process: there is no half-built program
 * worth returning, and a caller who wanted to carry on would be
 * carrying on with nothing.
 */
static void fail(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "serac: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(65);
}
/* }}} */

/* {{{ static const char *embedded_or_die() */
static const char *embedded_or_die(const char *name)
{
    const char *text = serac_embedded_text(name);
    if (!text)
        fail("this serac was built without %s inside it, which is a broken "
             "build rather than a missing option", name);
    return text;
}
/* }}} */

/* {{{ static int append_without_engine_include() */
/*
 * **Appends C source with its one include of the engine's header
 * removed**, and says how many it removed.
 *
 * The line being deleted is `#include "cera.h"`, and deleting it is
 * what lets the header's text sit in front of this instead. A line is
 * recognised by being exactly that once leading whitespace is
 * discounted, which is the only spelling anything here produces: the
 * engine carries one, written by hand and unchanged for the life of
 * the file, and the emitted construction code carries one, written by
 * the generator.
 *
 * The count is returned rather than ignored because the caller knows
 * what it should be and a mismatch is worth stopping for. Missing the
 * line would put a second set of declarations into the same
 * translation unit and the compiler would report it hundreds of lines
 * from anything a person can open, naming a file that only ever
 * existed inside this process.
 */
static int append_without_engine_include(buf_t *w, const char *text)
{
    static const char wanted[] = "#include \"cera.h\"";
    int removed = 0;

    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) + 1 : strlen(p);

        const char *scan = p;
        while (scan < p + len && (*scan == ' ' || *scan == '\t'))
            scan++;

        int is_the_line = strncmp(scan, wanted, sizeof wanted - 1) == 0;
        if (is_the_line) {
            /* Only the rest of the line being blank makes it the line
             * rather than something that merely starts like it. */
            const char *rest = scan + sizeof wanted - 1;
            while (rest < p + len && (*rest == ' ' || *rest == '\t' ||
                                      *rest == '\r' || *rest == '\n'))
                rest++;
            is_the_line = rest >= p + len;
        }

        if (is_the_line) {
            /* Replaced by a blank line rather than dropped, so that
             * every line number after it still matches the file this
             * text came from. A compiler error in the engine should
             * name the line it names in `cera.c`. */
            buf_addch(w, '\n');
            removed++;
        } else {
            buf_add(w, p, len);
        }

        if (!eol)
            break;
        p = eol + 1;
    }
    return removed;
}
/* }}} */

/* {{{ static void unpack() */
/*
 * **The engine written back out under the names it had**, for somebody
 * who wants it in a build system of their own. This is not a rescue
 * for people who could not manage `serac`; it is the same two files a
 * consumer has always been able to take, with the taking done for
 * them.
 */
static void unpack(const char *dir)
{
    if (mkdir(dir, 0777) != 0 && errno != EEXIST)
        fail("cannot make %s: %s", dir, strerror(errno));

    for (int i = 0; i < serac_n_embedded; i++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, serac_embedded[i].name);

        FILE *f = fopen(path, "wb");
        if (!f)
            fail("cannot write %s: %s", path, strerror(errno));

        size_t n = strlen(serac_embedded[i].text);
        if (fwrite(serac_embedded[i].text, 1, n, f) != n) {
            fclose(f);
            fail("cannot write %s: %s", path, strerror(errno));
        }
        fclose(f);
        printf("%s\n", path);
    }
}
/* }}} */

/* {{{ static void write_text_file() */
static void write_text_file(const char *path, const char *text, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        fail("cannot write %s: %s", path, strerror(errno));
    if (fwrite(text, 1, n, f) != n) {
        fclose(f);
        fail("cannot write %s: %s", path, strerror(errno));
    }
    fclose(f);
}
/* }}} */

/* {{{ static void run_compiler() */
/*
 * **The C compiler, handed a program on its standard input.**
 *
 * No shell. The arguments are file paths that came from somebody
 * else's command line, and a shell would reinterpret every space,
 * quote and dollar sign in them. Handing the operating system an
 * argument array directly means a path is whatever its bytes are.
 *
 * The text goes down a pipe rather than into a file for the reason the
 * top of this file gives: there is then nothing on disk to decide
 * where to put, how long to keep, or whether yesterday's copy is still
 * there.
 *
 * A compiler that refuses ends this program. What it printed has
 * already reached the terminal, because the child inherits stderr, and
 * a second sentence from here saying it failed would only bury it.
 */
static void run_compiler(char **argv, const char *text, size_t n)
{
    int fds[2];
    if (pipe(fds) != 0)
        fail("cannot make a pipe to the compiler: %s", strerror(errno));

    pid_t pid = fork();
    if (pid < 0)
        fail("cannot start the compiler: %s", strerror(errno));

    if (pid == 0) {
        /* The child reads the program from the pipe as its standard
         * input, which is what `-x c -` on the command line tells the
         * compiler to compile. */
        close(fds[1]);
        if (dup2(fds[0], STDIN_FILENO) < 0)
            _exit(70);
        close(fds[0]);
        execvp(argv[0], argv);
        fprintf(stderr, "serac: cannot run %s: %s\n", argv[0],
                strerror(errno));
        _exit(70);
    }

    close(fds[0]);

    /* A compiler that dies early — on a syntax error in a box, say —
     * closes the pipe while there is still text to send, and the write
     * that finds it closed raises a signal that would kill this
     * process silently. Ignoring it turns that into an ordinary short
     * write, and the child's own exit status is what gets reported. */
    signal(SIGPIPE, SIG_IGN);

    size_t sent = 0;
    while (sent < n) {
        ssize_t k = write(fds[1], text + sent, n - sent);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        sent += (size_t)k;
    }
    close(fds[1]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        exit(WIFEXITED(status) ? WEXITSTATUS(status) : 70);
}
/* }}} */

/* {{{ static char *dir_of() */
/*
 * The directory a path lives in, as a string this program owns. A path
 * with no slash in it lives in the current directory, which is the one
 * case where the answer is not a prefix of the input.
 */
static char *dir_of(arena_t *a, const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
        return arena_strdup(a, ".");
    if (slash == path)
        return arena_strdup(a, "/");
    return arena_strndup(a, path, (size_t)(slash - path));
}
/* }}} */

/* {{{ static char *stem_of() */
/*
 * A file's basename with its last extension removed — `accumulate`
 * from `maps/accumulate.map`. This is the program's name when nobody
 * said otherwise, which is why the map's own name is worth choosing
 * carefully.
 */
static char *stem_of(arena_t *a, const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base)
        return arena_strdup(a, base);
    return arena_strndup(a, base, (size_t)(dot - base));
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    arena_t *a = arena_new();

    const char  *out_path   = NULL;
    const char  *main_file  = NULL;
    const char  *root       = NULL;
    const char  *cc         = SERAC_CC;
    const char  *keep_c     = NULL;
    const char  *map_path   = NULL;
    int          shared     = 0;
    int          emit_c     = 0;
    /*
     * How many values of each result the program will have somewhere to
     * put. A bound has to exist, because the array a result lands in is
     * the caller's memory and the engine never grows it — that is what
     * lets a worker write into it without a lock. Going past it is a
     * refusal naming how many there were, never a quiet truncation, so
     * this number being wrong is something a person finds out about.
     */
    int          results_room = 1024;

    const char **sources  = arena_alloc(a, (size_t)(argc + 1) * sizeof *sources);
    int          n_sources = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Each of these changes what the run produces rather than what
         * it reads, so they are recognised before anything is treated
         * as a file to compile. */
        if (strcmp(arg, "--unpack") == 0) {
            if (i + 1 >= argc)
                fail("--unpack wants a directory to write into");
            unpack(argv[i + 1]);
            arena_free(a);
            return 0;
        }
        /*
         * **Which boxes a description names**, one per line, and
         * nothing else. A running program handed a description has to
         * know what it asks for before it can compile it, because a box
         * it does not hold has to be compiled first. What comes back is
         * exactly what the description said, unresolved: whoever asked
         * knows what they hold and this program does not.
         */
        if (strcmp(arg, "--map-boxes") == 0) {
            if (i + 1 >= argc)
                fail("--map-boxes wants a description to read");
            map_description_t *md = mapfile_parse(argv[i + 1]);
            for (desc_station_t *st = md->stations; st; st = st->next)
                printf("%s\n", st->box);
            mapfile_free(md);
            arena_free(a);
            return 0;
        }
        if (strcmp(arg, "--shared") == 0)          { shared = 1; continue; }
        if (strcmp(arg, "--emit-c") == 0)          { emit_c = 1; continue; }
        if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc)
                fail("-o wants a path to write to");
            out_path = argv[++i];
            continue;
        }
        if (strncmp(arg, "--main=", 7)   == 0) { main_file = arg + 7;  continue; }
        if (strncmp(arg, "--root=", 7)   == 0) { root      = arg + 7;  continue; }
        if (strncmp(arg, "--cc=", 5)     == 0) { cc        = arg + 5;  continue; }
        if (strncmp(arg, "--keep-c=", 9) == 0) { keep_c    = arg + 9;  continue; }
        if (strncmp(arg, "--results=", 10) == 0) {
            results_room = atoi(arg + 10);
            if (results_room <= 0)
                fail("--results wants a count of values greater than zero, "
                     "not '%s'", arg + 10);
            continue;
        }
        if (arg[0] == '-')
            fail("no such option: %s", arg);

        /* Everything left is a file, and the one that is not a C
         * source is the description. Told apart by extension, because
         * that is the one thing about these two files a caller has
         * already decided and this program should not ask twice. */
        size_t len = strlen(arg);
        if (len > 4 && strcmp(arg + len - 4, ".map") == 0) {
            if (map_path)
                fail("two descriptions named, %s and %s — a program is built "
                     "from one", map_path, arg);
            map_path = arg;
        } else {
            sources[n_sources++] = arg;
        }
    }

    /*
     * **A shared object may be boxes and no description.** That is what
     * a program compiling a box while it runs asks for: the shims,
     * placement functions, field tables and comparisons for some C, so
     * that a description arriving afterwards has something to name.
     * Every other form is a program, and a program is a description.
     */
    if (!map_path && !shared)
        fail("no description named — a program is a map file and the C "
             "functions it names");
    if (n_sources <= 0)
        fail("no C sources named — %s names boxes that have to be somewhere",
             map_path ? map_path : "a shared object");

    /* The thing this run is named after: the description when there is
     * one, and otherwise the first source, which is the only other
     * thing a caller named. */
    const char *named_after = map_path ? map_path : sources[0];

    /* Symbols carry the path of the box they came from, shortened
     * against this, so that two machines building the same tree emit
     * the same file. The description's own directory is the default
     * because that is the thing being built: a map and the sources
     * beside it are what somebody hands to somebody else. */
    if (!root)
        root = dir_of(a, named_after);

    /* Where the result lands when nobody said: beside the description,
     * named after it. Not the directory the shell happened to be
     * standing in — a description and the program built from it belong
     * together, which is the same reason a program looks beside its
     * description for the compiler that can extend it. */
    char *chosen = NULL;
    if (!out_path) {
        const char *dir  = dir_of(a, named_after);
        const char *stem = stem_of(a, named_after);
        size_t n = strlen(dir) + strlen(stem) + 8;
        chosen = arena_alloc(a, n);
        snprintf(chosen, n, "%s/%s%s", dir, stem,
                 emit_c ? ".c" : (shared ? ".so" : ""));
        out_path = chosen;
    }

    /*
     * **Never over something it was asked to read.** `--emit-c` over
     * `shapes.map` wants to write `shapes.c`, and a box source called
     * `shapes.c` is an entirely ordinary thing to have beside it — so
     * the default output path can land exactly on an input. Overwriting
     * it destroys the source and then fails to compile it, and the
     * message is about a brace on a line nobody wrote.
     *
     * Compared by the spelling given rather than by resolving each to a
     * real file, which would catch two names for one file and needs the
     * filesystem to answer. This catches the case that actually
     * happens, which is one command line naming the same path twice.
     */
    for (int i = 0; i < n_sources; i++)
        if (strcmp(out_path, sources[i]) == 0)
            fail("this would write over %s, which it was asked to read — "
                 "name somewhere else with -o", sources[i]);
    if (map_path && strcmp(out_path, map_path) == 0)
        fail("this would write over %s, which is the description itself — "
             "name somewhere else with -o", map_path);

    /* --- the description, and the boxes it names -------------------- */
    description_t d;
    gp_init(&d);
    for (int i = 0; i < n_sources; i++)
        gp_parse_file(&d, sources[i]);
    gp_validate(&d);

    const char *maps[1] = { map_path };
    int         n_maps  = map_path ? 1 : 0;

    /*
     * **A description compiled for a program that already holds the
     * boxes carries nothing but build functions.** That is what
     * `--shared` with a description means, and it is why the same flag
     * both makes a shared object and changes what goes in it: a late
     * description binds to the placement functions the running program
     * already published, and a second copy of a box would be a second
     * copy of something a wire is already pointing at.
     *
     * `--shared` with no description is the other half of the same
     * story — boxes arriving before anything names them — and those do
     * have to be defined, because nothing else holds them yet.
     */
    buf_t construction;
    buf_init(&construction);
    ge_build(&d, sources, n_sources, maps, n_maps, root,
             shared && n_maps > 0, &construction);

    /*
     * **A main, unless this is a part of a program rather than one.** A
     * shared object is loaded into something that already has one.
     *
     * Somebody else's `main` goes in the same place the generated one
     * would, so that whichever arrives, what follows is the same: one
     * buffer holding everything except the engine, which is exactly
     * what `--emit-c` hands over.
     */
    if (!shared) {
        if (main_file) {
            size_t n = 0;
            char  *text = gp_read_file(a, main_file, &n);
            /* Theirs may or may not include the header — either is a
             * perfectly ordinary thing to have written — so unlike the
             * two texts this program generates, the count is not
             * checked. */
            append_without_engine_include(&construction, text);
        } else {
            ge_main(&d, map_path, root, results_room, &construction);
        }
    }

    /*
     * **What a person gets when they ask for the C**: everything except
     * the engine, which is the same text that would have been compiled
     * with the engine in front of it. It includes `cera.h` the ordinary
     * way, so it compiles against an unpacked engine with no special
     * knowledge — and running `--emit-c`, `--unpack` and a compiler by
     * hand reproduces what one `serac` does, which is the only way a
     * person can check that claim.
     */
    if (emit_c) {
        write_text_file(out_path, construction.data, construction.len);
        buf_free(&construction);
        gp_free(&d);
        arena_free(a);
        return 0;
    }

    /* --- one piece of text, in this order -------------------------- */
    /*
     * The header first, because everything after it calls what it
     * declares. Then the engine, unless this is a shared object, which
     * binds to the engine already inside the program that will load it
     * rather than carrying a second copy. Then everything above.
     */
    buf_t whole;
    buf_init(&whole);

    buf_addstr(&whole, embedded_or_die("cera.h"));
    buf_addch(&whole, '\n');

    if (!shared) {
        int removed = append_without_engine_include(&whole,
                                                    embedded_or_die("cera.c"));
        if (removed != 1)
            fail("the engine carried inside this serac includes its own "
                 "header %d times, not once — the text and the rule for "
                 "joining it have gone out of step", removed);
    }

    {
        int removed = append_without_engine_include(&whole, construction.data);
        if (removed < 1)
            fail("the generated construction code does not include the "
                 "engine's header, so the emitter and the rule for joining "
                 "it have gone out of step");
    }

    if (keep_c)
        write_text_file(keep_c, whole.data, whole.len);

    /* --- and the compiler ------------------------------------------ */
    /*
     * `-x c -` says the standard input is C. The two linker settings
     * are supplied here because this program is the thing that knows
     * about them: without the export list a program builds and then
     * fails at run time when a box arrives, which looks like success
     * until it is not.
     *
     * No `-Werror`. The text being compiled contains somebody else's
     * box sources, and refusing to build their program over a warning
     * in their code is not this program's call to make.
     */
    char *cmd[24];
    int   n = 0;
    cmd[n++] = (char *)cc;
    cmd[n++] = (char *)"-std=gnu11";
    cmd[n++] = (char *)"-O2";
    cmd[n++] = (char *)"-pthread";
    cmd[n++] = (char *)"-ffunction-sections";
    cmd[n++] = (char *)"-fdata-sections";
    if (shared) {
        cmd[n++] = (char *)"-fPIC";
        cmd[n++] = (char *)"-shared";
    } else {
        cmd[n++] = (char *)"-Wl,--export-dynamic-symbol=cera_*";
        cmd[n++] = (char *)"-Wl,--gc-sections";
    }
    cmd[n++] = (char *)"-x";
    cmd[n++] = (char *)"c";
    cmd[n++] = (char *)"-";
    cmd[n++] = (char *)"-o";
    cmd[n++] = (char *)out_path;
    cmd[n]   = NULL;

    run_compiler(cmd, whole.data, whole.len);

    /*
     * **Silent when it works.** A compiler that announces where it put
     * things is a compiler that cannot be used by a program, and the
     * engine's own path for bringing in new code runs this as a
     * subprocess whose output would otherwise land in the middle of
     * somebody's results. Where the program went is decided by one
     * rule — beside the description, named after it — so saying it is
     * repeating something already known.
     */
    buf_free(&whole);
    buf_free(&construction);
    gp_free(&d);
    arena_free(a);
    return 0;
}
/* }}} */
