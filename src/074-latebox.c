/*
 * 074-latebox.c — a box arriving after the program started, from inside.
 *
 * What this is: the five steps that turn C source into a placeable
 * box while a program runs — save it, generate, compile, load, add —
 * and the growable half of the table stations are placed from.
 *
 * How it does it, in general terms: by running the same two programs
 * the build runs, as programs. The generator turns a box source into
 * a source for the generator; the compiler turns that into a shared object;
 * the dynamic linker loads it and hands back the arrays it defines.
 * Nothing here re-implements any of that, which is the point — a box
 * added late goes through the identical path a box added early did,
 * so there is one way for a box to come into existence rather than
 * two that must agree.
 *
 * **The table grows by adding a block and never moves a row.** The
 * generated array is the first block and is const; rows added later
 * live in blocks of their own. That is the same shape everything else
 * growable in this engine uses, for the same reason: a reader
 * resolving a row is never disturbed, because nothing it is looking
 * at moves.
 *
 * Unloading exists but is asked for rather than automatic: a caller
 * names a box and its library is closed, after checking that no
 * station in the map is still placed from it. Nothing sweeps or
 * refcounts, so a library nobody asks about stays for the life of the
 * process — bounded by how often somebody adds code, and stated rather
 * than hidden. Doing it automatically means waiting until no worker is
 * inside the code being freed, which is the retire-sweep-free
 * mechanism issue 214 builds for destination arrays and issue 216
 * needs for stations; it should be built once and shared by all three
 * rather than three times.
 *
 * **Libraries are opened globally** (issue 311d), so a box arriving
 * later can bind to one that arrived earlier instead of carrying its
 * own copy. That is what makes this an iterative compiler rather than
 * a sequence of unrelated compilations, and it is why the previous
 * paragraph matters more than it used to: something bound to may still
 * be bound to.
 */
#include "073-latebox.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* {{{ the build's own answers, baked in */
/*
 * Which compiler built this binary, where the generator is, and where
 * the headers generated code includes live. Defaults exist only so
 * this file compiles outside the project's Makefile; a real build
 * always defines all four.
 */
#ifndef SORA_CC
#define SORA_CC "cc"
#endif
#ifndef SORA_GENERATOR
#define SORA_GENERATOR "generate"
#endif
#ifndef SORA_INCLUDE
#define SORA_INCLUDE "."
#endif
#ifndef SORA_INCLUDE_LIBS
#define SORA_INCLUDE_LIBS "."
#endif
#ifndef SORA_RAM_SHARED
#define SORA_RAM_SHARED "/dev/shm/minimal-soramech"
#endif
#ifndef SORA_RAM_EXEC
#define SORA_RAM_EXEC "/tmp/minimal-soramech"
#endif
/* }}} */

/* {{{ struct late_block */
/*
 * One dlopen's worth of rows. The arrays belong to the loaded object
 * and live as long as it does, which is forever — see the note about
 * unloading at the top of this file.
 */
typedef struct late_block {
    struct late_block *next;
    /* The placement functions this object brought with it (issue
     * 311b). A box compiled while the program runs has to be
     * placeable the same way as one compiled into it, which means the
     * same generated function doing the writing. */
    const box_place_t *places;
    int                n_places;
    /* And the source it was compiled from, as text (issue 311d). The
     * generator emits this for every object it writes, so a loaded
     * one carries its own C exactly as the program's own generated
     * file does. Nothing is copied and nothing is allocated: these
     * point into the loaded object and live as long as it does.
     *
     * Two things read it. A person or a capture wanting to write out
     * what a grown program is now made of, which cannot be answered
     * from the build alone once boxes have arrived. And the check that
     * refuses to compile the same source twice — same path, same
     * bytes, already here. */
    const box_source_t *sources;
    int                 n_sources;
    void              *handle;
} late_block_t;

static late_block_t *late_head;   /* newest first */
static int           late_total;
static int           late_serial; /* names the scratch files apart */
/* }}} */

/* {{{ late_source_dir() / late_library_dir() */
/*
 * **Two tiers, and which goes where is not arbitrary.** The project
 * keeps RAM-backed scratch in two places: `/dev/shm` for artifacts
 * that are read — logs, text, anything a person or a reloader looks
 * at — and `/tmp` for anything that gets executed. `/dev/shm` is
 * commonly mounted so that nothing on it may be executed at all, so a
 * shared object written there compiles fine and then cannot be
 * loaded, failing with a message about mapping a segment that says
 * nothing about the real cause.
 *
 * So the source text goes to the read tier and the compiled library
 * goes to the execute tier. Found by putting them both in the wrong
 * one, which is the useful kind of mistake: the rule existed and the
 * reason for it had to be rediscovered.
 */
const char *late_source_dir(void)
{
    return SORA_RAM_SHARED "/late-boxes";
}

static const char *late_library_dir(void)
{
    return SORA_RAM_EXEC "/late-boxes";
}
/* }}} */

/* {{{ late_box_count() / late_box_at() */
int late_box_count(void)
{
    return late_total;
}

const box_place_t *late_box_at(int i)
{
    /* Blocks are newest first, so walking them in order and counting
     * down gives the caller oldest-first, which is the order boxes
     * were added and the only order that means anything. */
    int remaining = late_total - i;
    for (late_block_t *b = late_head; b; b = b->next) {
        if (remaining <= b->n_places)
            return &b->places[remaining - 1];
        remaining -= b->n_places;
    }
    return NULL;
}
/* }}} */

/* {{{ late_place_find() */
/*
 * The placement function for a box that arrived after the program
 * started. Newest first, for the same reason the box lookup is: a
 * name added twice resolves to the newer one, and the older code is
 * still loaded and still callable by anything already placed.
 */
const box_place_t *late_place_find(const char *name);

const box_place_t *late_place_find(const char *name)
{
    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_places; i++)
            /* The same rule the compiled-in rows are searched by
             * (issue 311a), so a bare name, a basename and a path all
             * mean here what they mean there. */
            if (box_place_matches(&b->places[i], name))
                return &b->places[i];
    return NULL;
}
/* }}} */

/* {{{ late_source_text() */
/*
 * The C a late-arriving source was compiled from, by the path it was
 * compiled under. Newest first, for the same reason the box lookup is:
 * a path compiled twice reports the newer text, which is what somebody
 * asking "what is running now" means by the question.
 *
 * Full path first and basename second, matching how a box is
 * addressed, so that a person can type what they can see.
 */
const char *late_source_text(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_sources; i++)
            if (strcmp(b->sources[i].path, path) == 0)
                return b->sources[i].text;

    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_sources; i++) {
            const char *slash = strrchr(b->sources[i].path, '/');
            const char *base = slash ? slash + 1 : b->sources[i].path;
            if (strcmp(base, path) == 0)
                return b->sources[i].text;
        }
    return NULL;
}
/* }}} */


/* {{{ static int ensure_dir() */
static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;
    fprintf(stderr, "latebox: cannot create %s: %s\n", path, strerror(errno));
    return -1;
}
/* }}} */

/* {{{ static int write_text() */
static int write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "latebox: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok) {
        fprintf(stderr, "latebox: cannot write %s: %s\n",
                path, strerror(errno));
        remove(path);
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ static int run() */
/*
 * Runs a command and reports whether it succeeded. The child's own
 * output goes wherever ours goes, deliberately: when a box source
 * does not compile, the message worth reading is the compiler's,
 * with its line numbers and its carets, not a summary this file
 * invented.
 */
static int run(const char *command)
{
    int rc = system(command);
    if (rc == 0)
        return 0;
    if (rc < 0)
        fprintf(stderr, "latebox: cannot run '%s': %s\n",
                command, strerror(errno));
    return -1;
}
/* }}} */

/* {{{ static void close_library() */
static void close_library(void *handle)
{
    dlclose(handle);
}
/* }}} */

/* {{{ late_unload_box() */
int late_unload_box(map_t *m, const char *name)
{
    if (!m || !name || !*name) {
        fprintf(stderr, "latebox: asked to unload nothing\n");
        return -1;
    }

    /* Find the block holding it, and refuse outright if the name
     * belongs to a box the program was built with — that code is part
     * of the binary and there is nothing to close. */
    late_block_t **link = &late_head;
    late_block_t *found = NULL;
    for (; *link; link = &(*link)->next) {
        for (int i = 0; i < (*link)->n_places; i++)
            if (strcmp((*link)->places[i].name, name) == 0) {
                found = *link;
                break;
            }
        if (found)
            break;
    }
    if (!found) {
        fprintf(stderr, "latebox: '%s' was not added while this program ran, "
                        "so there is nothing to unload\n", name);
        return -1;
    }

    /*
     * Refused while any station places any box in this block. The
     * block is the unit that gets closed, so one placed box in it
     * keeps the whole thing — which is right, because they arrived in
     * one library and leave in one.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        /*
         * **By the name the station was placed as** (issue 311b),
         * rather than by comparing shim pointers. The record that
         * held those pointers is gone; a station carries the name
         * literal its own placement function wrote, which is the same
         * fact arrived at from the other side.
         *
         * It is conservative in exactly one direction, and that
         * direction is the safe one: two blocks holding a box of the
         * same name would each refuse to unload while the other's
         * station stands. Refusing an unload that could have gone
         * ahead costs a library staying loaded; allowing one that
         * could not is the crash this check exists to prevent.
         */
        for (int b = 0; b < found->n_places; b++)
            if (s->box_name && strcmp(s->box_name,
                                      found->places[b].address) == 0) {
                fprintf(stderr,
                        "latebox: station %d places '%s', so its code cannot "
                        "be unloaded — remove the station first\n",
                        i, found->places[b].address);
                return -1;
            }
    }

    /*
     * Unlinked first, so nothing can find it by name from here on and
     * no station can be placed from it after this point. Then the
     * library goes to the scrapyard: a worker may be *inside* this
     * code right now, and the counter that answers that is the same
     * one a replaced destination set uses (issue 214).
     */
    late_total -= found->n_places;
    *link = found->next;
    void *handle = found->handle;
    free(found);
    map_retire(m, handle, close_library);
    return 0;
}
/* }}} */

/* {{{ late_recover_box() */
/*
 * Compile a box back into existence from the source it left behind.
 *
 * A box added while a program ran saved its source under its own
 * name, so a **fresh process** loading a dump of that program can
 * find it. Without this, a dump taken after somebody added code
 * describes a program that cannot be rebuilt — which would make the
 * dump a record of something unreproducible, and the whole value of a
 * dump is that it says what is actually there.
 *
 * **It announces itself.** Recovering is doing something the caller
 * did not ask for, on the strength of a file found lying about, and
 * this project treats a silent fallback as an error. So it says which
 * box it is recovering and where the source came from, every time.
 *
 * Returns the row, or null when there is no source to recover from —
 * which is the ordinary case of a genuinely misspelled name, and the
 * caller's message for that is the best one in the program.
 */
const box_place_t *late_recover_box(const char *name);

const box_place_t *late_recover_box(const char *name)
{
    if (!name || !*name)
        return NULL;

    char path[512];
    snprintf(path, sizeof path, "%s/%s.c", late_source_dir(), name);

    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);

    fprintf(stderr, "latebox: '%s' was not built in; recovering it from %s\n",
            name, path);

    int added = late_compile_source(text);
    free(text);
    if (added < 0) {
        fprintf(stderr, "latebox: '%s' could not be recovered from its own "
                        "saved source\n", name);
        return NULL;
    }
    return box_place_find(name);
}
/* }}} */

/* {{{ static int ensure_path_dirs() */
/*
 * Create every directory leading to a file path, the way `mkdir -p`
 * does. Needed because the sources a program carries are filed under
 * the paths the build knew them by — `src/boxes/029-demo-boxes.c` —
 * and writing them back out under those same paths is what lets the
 * generator resolve a description's box names exactly as it did at
 * build time. Flattening them would work until two directories held a
 * file of the same name, which is the case the addressing rules exist
 * for in the first place.
 */
static int ensure_path_dirs(const char *path)
{
    char work[1024];
    size_t n = strlen(path);
    if (n >= sizeof work) {
        fprintf(stderr, "latebox: path too long: %s\n", path);
        return -1;
    }
    memcpy(work, path, n + 1);

    for (char *p = work + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(work, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "latebox: cannot create %s: %s\n",
                    work, strerror(errno));
            return -1;
        }
        *p = '/';
    }
    return 0;
}
/* }}} */

/* {{{ static int spill_sources() */
/*
 * Write every source this program is made of into a directory, under
 * the path it was compiled as. The generator is then pointed at that
 * directory as its root, so what it reads is byte for byte what this
 * program was built from and every name resolves the way it did then.
 *
 * **This is a copy of text, not of code.** Nothing here is compiled;
 * the sources exist so the generator can answer "which box does this
 * line mean" with the same answer it gave at build time, and the
 * emitted file that results carries none of them.
 *
 * Both halves of what a program is made of go out — what the build
 * compiled in and what has arrived since — because a description may
 * name either, and the distinction is not one a person writing a
 * description should have to know about.
 *
 * Returns how many were written, or -1.
 */
static int spill_sources(const char *dir, const char **paths, int cap)
{
    int n = 0;
    char full[1024];

    for (int i = 0; i < sora_n_box_sources; i++) {
        if (n >= cap)
            break;
        if (snprintf(full, sizeof full, "%s/%s",
                     dir, sora_box_sources[i].path) >= (int)sizeof full) {
            fprintf(stderr, "latebox: path too long: %s\n",
                    sora_box_sources[i].path);
            return -1;
        }
        if (ensure_path_dirs(full) != 0)
            return -1;
        if (write_text(full, sora_box_sources[i].text) != 0)
            return -1;
        paths[n] = strdup(full);
        if (!paths[n]) {
            fprintf(stderr, "latebox: out of memory\n");
            return -1;
        }
        n++;
    }

    /* And what has arrived since. Oldest blocks last in this walk, so
     * a path compiled twice is written by the newest first and then
     * overwritten by the older — which would be backwards, so the
     * newest wins by being written last. Walking the list in reverse
     * is not worth the bookkeeping: skipping a path already written is
     * the same answer and reads as what it is. */
    for (late_block_t *b = late_head; b; b = b->next) {
        for (int i = 0; i < b->n_sources; i++) {
            if (n >= cap)
                break;
            int already = 0;
            for (int k = 0; k < n && !already; k++)
                if (strstr(paths[k], b->sources[i].path))
                    already = 1;
            if (already)
                continue;
            if (snprintf(full, sizeof full, "%s/%s",
                         dir, b->sources[i].path) >= (int)sizeof full) {
                fprintf(stderr, "latebox: path too long: %s\n",
                        b->sources[i].path);
                return -1;
            }
            if (ensure_path_dirs(full) != 0)
                return -1;
            if (write_text(full, b->sources[i].text) != 0)
                return -1;
            paths[n] = strdup(full);
            if (!paths[n]) {
                fprintf(stderr, "latebox: out of memory\n");
                return -1;
            }
            n++;
        }
    }
    return n;
}
/* }}} */

/* {{{ late_compile_map() */
void (*late_compile_map(const char *map_text))(map_t *m)
{
    if (!map_text || !*map_text) {
        fprintf(stderr, "latebox: an empty description describes nothing\n");
        return NULL;
    }

    const char *dir = late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(SORA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return NULL;
    if (ensure_dir(SORA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
        return NULL;

    int serial = late_serial++;
    char map_path[512], src_root[512], gen_path[512], lib_path[512];
    char cmd[8192];
    snprintf(map_path, sizeof map_path, "%s/map-%d-%d.map",
             dir, (int)getpid(), serial);
    snprintf(src_root, sizeof src_root, "%s/sources-%d-%d",
             dir, (int)getpid(), serial);
    snprintf(gen_path, sizeof gen_path, "%s/built-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(lib_path, sizeof lib_path, "%s/built-%d-%d.so",
             libdir, (int)getpid(), serial);

    /* Saved before anything is done with it, for the same reason a box
     * source is: a dump may be taken at any moment, including while
     * something is going wrong, and a failure path is the worst
     * possible time to discover something needed saving. */
    if (write_text(map_path, map_text) != 0)
        return NULL;
    if (ensure_dir(src_root) != 0)
        return NULL;

    enum { MAX_SPILLED = 256 };
    const char *spilled[MAX_SPILLED];
    int n_spilled = spill_sources(src_root, spilled, MAX_SPILLED);
    if (n_spilled <= 0) {
        fprintf(stderr, "latebox: this program carries no source text, so a "
                        "description's box names cannot be resolved\n");
        return NULL;
    }

    /*
     * **--external-boxes is the whole difference from compiling a
     * box.** It says the boxes are already in the process that will
     * load this, so the emitted file declares the functions that build
     * their stations rather than defining them, and carries no second
     * copy of anything.
     */
    int at = snprintf(cmd, sizeof cmd,
                      "%s %s --root=%s --map=%s --external-boxes",
                      SORA_GENERATOR, gen_path, src_root, map_path);
    for (int i = 0; i < n_spilled && at < (int)sizeof cmd; i++)
        at += snprintf(cmd + at, sizeof cmd - (size_t)at, " %s", spilled[i]);
    if (at >= (int)sizeof cmd) {
        fprintf(stderr, "latebox: too many sources to name on one command "
                        "line\n");
        return NULL;
    }
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the generator refused %s\n", map_path);
        return NULL;
    }

    snprintf(cmd, sizeof cmd,
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -I%s -o %s %s",
             SORA_CC, SORA_INCLUDE, SORA_INCLUDE_LIBS, lib_path, gen_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the compiler refused the code generated "
                        "for %s\n", map_path);
        return NULL;
    }

    /* Globally, like a box, so a description compiled after this one
     * can bind to anything this one brought. */
    void *handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        /* The failure worth naming: a description naming a box this
         * program does not hold arrives here as an unresolved symbol,
         * and the message names it. */
        fprintf(stderr, "latebox: cannot load %s: %s\n", lib_path, dlerror());
        return NULL;
    }

    const map_build_t *builds = dlsym(handle, "sora_map_builds");
    const int *count = dlsym(handle, "sora_n_map_builds");
    if (!builds || !count || *count <= 0) {
        fprintf(stderr, "latebox: %s builds no description — the generator "
                        "emitted something unexpected\n", lib_path);
        dlclose(handle);
        return NULL;
    }
    return builds[0].build;
}
/* }}} */

/* {{{ late_compile_source() */
int late_compile_source(const char *c_source)
{
    if (!c_source || !*c_source) {
        fprintf(stderr, "latebox: asked to compile nothing\n");
        return -1;
    }

    const char *dir = late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(SORA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return -1;
    if (ensure_dir(SORA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
        return -1;

    int serial = late_serial++;
    char box_path[512], gen_path[512], lib_path[512], cmd[2048];
    snprintf(box_path, sizeof box_path, "%s/box-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(gen_path, sizeof gen_path, "%s/emitted-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(lib_path, sizeof lib_path, "%s/box-%d-%d.so",
             libdir, (int)getpid(), serial);

    /* The source is saved **before** anything is done with it, and
     * that ordering is the decision rather than an accident: a dump
     * may be taken at any moment, including while something is going
     * wrong, and a failure path is the worst possible time to
     * discover that something needed saving. The artifact exists
     * before anybody needs it. */
    if (write_text(box_path, c_source) != 0)
        return -1;

    snprintf(cmd, sizeof cmd, "%s %s %s", SORA_GENERATOR, gen_path, box_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the generator refused %s\n", box_path);
        return -1;
    }

    /* Position-independent and shared, with the engine's headers
     * reachable because generated code includes them. The compiler is
     * the one that built this binary, which is what makes its answer
     * to sizeof the same answer. */
    snprintf(cmd, sizeof cmd,
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -I%s -o %s %s",
             SORA_CC, SORA_INCLUDE, SORA_INCLUDE_LIBS, lib_path, gen_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the compiler refused the generated "
                        "generated source for %s\n", box_path);
        return -1;
    }

    /*
     * **Opened globally, so that the next arrival can bind to this
     * one** (issue 311d step 7). Privately was the old setting, and it
     * meant every arrival was an island: a second one naming a
     * function the first had already compiled had to carry its own
     * copy, because it could not see the first one's.
     *
     * Global costs nothing here and needs no table. When a later
     * shared object names a function it does not define, the dynamic
     * linker binds it against what is already loaded — which is a
     * lookup by name that the operating system already maintains for
     * every process, that this project does not have to write, test,
     * or keep in step with anything.
     *
     * Measured before it was relied on: a second object naming a
     * function it does not define binds straight to the first object's
     * copy, with this process uninvolved.
     *
     * What it means for names is worth stating plainly, because global
     * scope is usually where somebody gets hurt: two arrivals defining
     * the same symbol resolve to the first. That is correct here
     * rather than dangerous, because generated symbols carry the box's
     * full path, so two boxes only collide when they are the same box
     * from the same file — and a box is forbidden to remember anything
     * between calls, so two copies of one box are indistinguishable.
     */
    void *handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "latebox: cannot load %s: %s\n", lib_path, dlerror());
        return -1;
    }

    /*
     * **One pair of symbols now, where there were two** (issue 311b).
     * The generated file used to define a table of box records beside
     * the placement functions, and this fetched both — the records to
     * learn what the new box was, the placements to be able to put one
     * anywhere. The records are gone: every number they held is
     * written straight onto a station by the placement function, from
     * a `sizeof` the compiler folded, so there was nothing in them
     * anybody read twice.
     */
    const box_place_t *places = dlsym(handle, "box_places");
    const int *count = dlsym(handle, "n_box_places");
    if (!places || !count) {
        fprintf(stderr, "latebox: %s defines no placement functions — the "
                        "generator emitted something unexpected\n", lib_path);
        dlclose(handle);
        return -1;
    }
    if (*count <= 0) {
        fprintf(stderr, "latebox: %s defines no boxes; a source with only "
                        "static helpers has nothing to place\n", box_path);
        dlclose(handle);
        return -1;
    }

    late_block_t *block = calloc(1, sizeof *block);
    if (!block) {
        fprintf(stderr, "latebox: out of memory\n");
        dlclose(handle);
        return -1;
    }
    block->places   = places;
    block->n_places = *count;
    block->handle   = handle;

    /* The source text the object carries (issue 311d). Absent is not
     * an error the way absent placement functions are: an object built
     * by an older generator has boxes but no text, and refusing to
     * load it would trade a working box for a missing document. What
     * it costs is that this source cannot be written back out, and the
     * lookup answers NULL rather than pretending. */
    const box_source_t *sources = dlsym(handle, "sora_box_sources");
    const int *n_sources = dlsym(handle, "sora_n_box_sources");
    if (sources && n_sources && *n_sources > 0) {
        block->sources   = sources;
        block->n_sources = *n_sources;
    }

    /* Published last, and by one write, so a reader walking the list
     * either sees this block complete or does not see it at all. */
    block->next = late_head;
    late_head = block;
    late_total += *count;

    /*
     * A copy per box, named for the box, so a **later process** can
     * find the source from a name alone — which is what makes a dump
     * taken after this reloadable. The serial-numbered file above is
     * what was handed over; these are how it is found again.
     *
     * One source may define several boxes, so each gets its own copy
     * of the whole thing. Copies rather than links because a link into
     * the scratch tier is one more thing that can be half there.
     */
    for (int i = 0; i < *count; i++) {
        char by_name[512];
        snprintf(by_name, sizeof by_name, "%s/%s.c", dir, places[i].name);
        if (write_text(by_name, c_source) != 0)
            fprintf(stderr, "latebox: '%s' is loaded but its source could "
                            "not be filed under its own name; a dump taken "
                            "now will not reload in a fresh process\n",
                    places[i].name);
    }

    return *count;
}
/* }}} */
