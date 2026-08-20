/*
 * 074-latebox.c — a box arriving after the program started, from inside.
 *
 * What this is: the five steps that turn C source into a placeable
 * box while a program runs — save it, generate, compile, load, add —
 * and the growable half of the table stations are placed from.
 *
 * How it does it, in general terms: by running the same two programs
 * the build runs, as programs. The generator turns a box source into
 * a registry source; the compiler turns that into a shared object;
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
 * What is deliberately absent: unloading. A shared object is never
 * closed, so a box added at run time stays for the life of the
 * process. Doing it safely means waiting until no worker is inside
 * the code being freed, which is the retire-sweep-free mechanism
 * issue 214 builds for destination arrays and issue 216 needs for
 * stations. It should be built once and shared by all three rather
 * than three times; until then this leaks a library per compile,
 * which is bounded by how often somebody adds code and is stated
 * rather than hidden.
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
    const box_info_t  *boxes;
    int                n_boxes;
    void              *handle;
} late_block_t;

static late_block_t *late_head;   /* newest first */
static int           late_total;
static int           late_serial; /* names the scratch files apart */
/* }}} */

/* {{{ registry_late_source_dir() / late_library_dir() */
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
const char *registry_late_source_dir(void)
{
    return SORA_RAM_SHARED "/late-boxes";
}

static const char *late_library_dir(void)
{
    return SORA_RAM_EXEC "/late-boxes";
}
/* }}} */

/* {{{ registry_late_count() / registry_late_box() */
int registry_late_count(void)
{
    return late_total;
}

const box_info_t *registry_late_box(int i)
{
    /* Blocks are newest first, so walking them in order and counting
     * down gives the caller oldest-first, which is the order boxes
     * were added and the only order that means anything. */
    int remaining = late_total - i;
    for (late_block_t *b = late_head; b; b = b->next) {
        if (remaining <= b->n_boxes)
            return &b->boxes[remaining - 1];
        remaining -= b->n_boxes;
    }
    return NULL;
}
/* }}} */

/* {{{ registry_late_find() */
/*
 * Called by registry_find after it has walked the generated rows.
 * Newest first, so a name added twice resolves to the newer one —
 * which is the only useful answer, since the older row's code is
 * still loaded and still callable by anything already placed.
 */
const box_info_t *registry_late_find(const char *name);

const box_info_t *registry_late_find(const char *name)
{
    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_boxes; i++)
            if (strcmp(b->boxes[i].name, name) == 0)
                return &b->boxes[i];
    return NULL;
}
/* }}} */

/* {{{ registry_late_name_for_shim() */
const char *registry_late_name_for_shim(task_call_t shim);

const char *registry_late_name_for_shim(task_call_t shim)
{
    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_boxes; i++)
            if (b->boxes[i].shim == shim)
                return b->boxes[i].name;
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

/* {{{ registry_unload_box() */
int registry_unload_box(map_t *m, const char *name)
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
        for (int i = 0; i < (*link)->n_boxes; i++)
            if (strcmp((*link)->boxes[i].name, name) == 0) {
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
        station_t *s = &m->stations[i];
        if (!s->call)
            continue;
        for (int b = 0; b < found->n_boxes; b++)
            if (s->call == found->boxes[b].shim) {
                fprintf(stderr,
                        "latebox: station %d places '%s', so its code cannot "
                        "be unloaded — remove the station first\n",
                        i, found->boxes[b].name);
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
    late_total -= found->n_boxes;
    *link = found->next;
    void *handle = found->handle;
    free(found);
    map_retire(m, handle, close_library);
    return 0;
}
/* }}} */

/* {{{ registry_recover_box() */
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
const box_info_t *registry_recover_box(const char *name);

const box_info_t *registry_recover_box(const char *name)
{
    if (!name || !*name)
        return NULL;

    char path[512];
    snprintf(path, sizeof path, "%s/%s.c", registry_late_source_dir(), name);

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

    int added = registry_compile_source(text);
    free(text);
    if (added < 0) {
        fprintf(stderr, "latebox: '%s' could not be recovered from its own "
                        "saved source\n", name);
        return NULL;
    }
    return registry_find(name);
}
/* }}} */

/* {{{ registry_compile_source() */
int registry_compile_source(const char *c_source)
{
    if (!c_source || !*c_source) {
        fprintf(stderr, "latebox: asked to compile nothing\n");
        return -1;
    }

    const char *dir = registry_late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(SORA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return -1;
    if (ensure_dir(SORA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
        return -1;

    int serial = late_serial++;
    char box_path[512], gen_path[512], lib_path[512], cmd[2048];
    snprintf(box_path, sizeof box_path, "%s/box-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(gen_path, sizeof gen_path, "%s/registry-%d-%d.c",
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
                        "registry for %s\n", box_path);
        return -1;
    }

    void *handle = dlopen(lib_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "latebox: cannot load %s: %s\n", lib_path, dlerror());
        return -1;
    }

    /* The generated file defines exactly the two symbols the build's
     * own registry defines, so the loaded object hands back its rows
     * the same way the compiled-in ones are reached. */
    const box_info_t *boxes = dlsym(handle, "registry_boxes");
    const int *count = dlsym(handle, "registry_n_boxes");
    if (!boxes || !count) {
        fprintf(stderr, "latebox: %s defines no registry — the generator "
                        "emitted something unexpected\n", lib_path);
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
    block->boxes   = boxes;
    block->n_boxes = *count;
    block->handle  = handle;

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
        snprintf(by_name, sizeof by_name, "%s/%s.c", dir, boxes[i].name);
        if (write_text(by_name, c_source) != 0)
            fprintf(stderr, "latebox: '%s' is loaded but its source could "
                            "not be filed under its own name; a dump taken "
                            "now will not reload in a fresh process\n",
                    boxes[i].name);
    }

    return *count;
}
/* }}} */
