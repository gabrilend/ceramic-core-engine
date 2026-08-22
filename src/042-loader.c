/*
 * 042-loader.c — the moment the two halves of a program meet.
 *
 * What this is: the loader (issues 602–605). The binary holds a
 * list of boxes and no map; the file holds a map and no code.
 * This file walks the parsed description twice — create everything,
 * then connect everything — checks every wire against the emitted sizes
 * at the first moment both ends are known, validates what only the
 * whole map can show, and seeds the first tasks.
 *
 * How it does it, in general terms: declaration order must not
 * matter (an arrow may point at a station declared further down),
 * which is the entire reason there are two passes. The name lookup
 * table lives only as long as loading. Validation failures are
 * collected and printed together before stopping — someone fixing a
 * new map wants the whole list — and the error messages carry
 * station names and type names, because they are the surface a
 * person actually touches.
 */
#include "040-mapfile.h"
#include "026-emitted.h"
/* A description on disk becomes a program by being compiled, which
 * is the same door a box source goes through (issue 311d). */
#include "073-latebox.h"
#include "091-stopping.h"

/* A box added while some earlier process ran; see 073-latebox.h. It
 * is declared here rather than included, because the loader needs one
 * function from that file and nothing else it offers. */
const box_place_t *late_recover_box(const char *name);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ die_load() */
static void die_load(const char *path, int line, const char *station,
                     const char *what)
{
    /*
     * **Exit 65, meaning the input was malformed** (issue 106), rather
     * than aborting. A core dump says nothing about a mistyped box
     * name, and a shell script that wants to tell "the file was
     * wrong" from "the machine ran out of memory" could not, because
     * both arrived as the same abnormal death.
     */
    char said[768];
    if (station)
        snprintf(said, sizeof said, "map %s:%d: station '%s': %s",
                 path, line, station, what);
    else
        snprintf(said, sizeof said, "map %s:%d: %s", path, line, what);
    sora_stop_now(NULL, SORA_EXIT_BAD_FILE, said);
}
/* }}} */




/* {{{ static char *read_whole_file() */
/*
 * A description, as text. Small by nature — a description names
 * stations and wires, and a program with a thousand of either is
 * still a few tens of kilobytes — so it is read whole rather than
 * streamed, and the caller frees it.
 */
static char *read_whole_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        die_load(path, 0, NULL, "cannot be opened");

    if (fseek(f, 0, SEEK_END) != 0)
        die_load(path, 0, NULL, "cannot be measured");
    long n = ftell(f);
    if (n < 0)
        die_load(path, 0, NULL, "cannot be measured");
    rewind(f);

    char *text = malloc((size_t)n + 1);
    if (!text)
        die_load(path, 0, NULL, "out of memory reading a description");
    size_t got = fread(text, 1, (size_t)n, f);
    text[got] = '\0';
    fclose(f);
    return text;
}
/* }}} */

/* {{{ static int build_from_file() */
/*
 * **A description on disk becomes the calls it describes, and then
 * those calls are made** (issue 311d).
 *
 * Nothing here reads the description. It is handed to the compiler —
 * the same generator and the same C compiler the build used — which
 * turns it into a function that builds it, and that function is
 * called. So there is one way a description becomes a program, and
 * this is a caller of it rather than a second implementation.
 *
 * **What that costs is a compiler invocation**, roughly a tenth of a
 * second, where walking the description cost nothing. It is paid
 * deliberately: the alternative was keeping a second way to turn a
 * description into a program, which is exactly the thing this family
 * of changes exists to remove. A program that never reads a
 * description at run time never pays it, and a program built from its
 * own descriptions never reads one.
 *
 * **And the boxes are not compiled.** They are already here; what is
 * compiled is the description, and the code that comes back binds to
 * the station-builders this program published.
 */
static void build_from_file(map_t *m, const char *path, map_instance_t *out)
{
    char *text = read_whole_file(path);
    const map_build_t *built = late_compile_map(text);
    free(text);

    if (!built)
        die_load(path, 0, NULL, "could not be compiled into this program");

    if (!out) {
        built->build(m, NULL, 0);
        return;
    }

    /*
     * **The row says how many stations before anything is built**,
     * which is what lets the table be the right size on the first and
     * only build. Asking the built function would mean building, and
     * building twice would make two copies of the description.
     */
    out->count = built->n_stations;
    out->station = calloc((size_t)(out->count > 0 ? out->count : 1),
                          sizeof *out->station);
    if (!out->station)
        die_load(path, 0, NULL, "out of memory instantiating a map");

    built->build(m, out->station, out->count);
}
/* }}} */

/* {{{ map_load_file() */
/* {{{ static int marked_incomplete() */
/*
 * **An artifact that says it lost work** (issue 712). A capture taken
 * while workers were still inside boxes never got their results, and
 * it says so at the top rather than leaving it to be noticed.
 *
 * Read from the text here rather than from the description, because
 * the marker is a comment — the parser drops comments on the floor, as
 * it should, since a comment is by definition not part of what a file
 * says about a program. This is a fact about the *file*, and the file
 * is what asks it.
 */
static int marked_incomplete(const char *text)
{
    return strstr(text, "# INCOMPLETE CAPTURE") != NULL;
}
/* }}} */

static map_t *load_file(const char *path, int n_workers, int salvaging);

/* {{{ map_load_file() / map_load_salvage() */
/*
 * **Reading a description back is refused when it says it lost work**,
 * unless the caller asks for salvage (issue 712). A program picked up
 * from an incomplete capture is quietly missing results somebody
 * computed, and quietly is the part this engine refuses everywhere: a
 * fallback is a warning and a warning is an error.
 *
 * Salvaging is a different act, and having a different name for it is
 * the point — whoever calls it has said out loud that they know what
 * is missing.
 */
map_t *map_load_file(const char *path, int n_workers)
{
    return load_file(path, n_workers, 0);
}

map_t *map_load_salvage(const char *path, int n_workers)
{
    return load_file(path, n_workers, 1);
}

static map_t *load_file(const char *path, int n_workers, int salvaging)
{
    /*
     * An empty table, grown one station at a time as the description
     * is built (issue 211). It used to count the station lines and
     * allocate exactly that many, which meant reading a map was a
     * different act from adding a station to a running program — and
     * under one construction surface it should not be.
     */
    /* Asked of the file before anything is built from it, so a
     * refusal costs no compiler invocation and leaves nothing behind
     * to clean up. */
    if (!salvaging) {
        char *text = read_whole_file(path);
        int lossy = marked_incomplete(text);
        free(text);
        if (lossy)
            die_load(path, 0, NULL,
                     "says at the top that it is an incomplete capture — "
                     "work was still running when it was written and its "
                     "results were never delivered. Read it with the "
                     "salvage door if that is understood and wanted");
    }

    map_t *m = map_create_empty();

    build_from_file(m, path, NULL);

    /* The pool exists before the seed so the seed has somewhere to
     * push, but its workers stay parked until the caller releases —
     * the seeding window issue 102 built. */
    map_start(m, n_workers);

    /*
     * **Reading a file no longer validates or seeds; it asks for the
     * program to be brought up, the same as anybody else would**
     * (issue 212). The checks and the seed were the last thing the
     * loader could do that nothing else could, and with them moved
     * there is no state called *still loading* left for it to be in.
     */
    const char *no = map_bring_up(m);
    if (no)
        die_load(path, 0, NULL, no);

    /*
     * **Whether starting nothing is a fault is the caller's to say**,
     * and this caller says yes.
     *
     * Bringing a program up is repeatable, so seeding nothing is
     * perfectly ordinary — a program brought up, grown by one
     * station, and brought up again seeds nothing the second time and
     * should not be scolded for it. But a *file* somebody asked to be
     * run is a different promise: if no station can start without
     * waiting for a value, and no value can arrive because nothing is
     * running to send one, then the program does nothing at all, and
     * saying so is more use than starting it.
     *
     * **Unless the program has a declared entrance**, in which case
     * waiting is exactly what it is supposed to do (issue 213). This
     * refusal means "nothing can start and nothing can arrive, so
     * this program will do nothing at all" — and a declared entrance
     * is a station something outside delivers to, which makes the
     * second half of that false.
     */
    int has_entrance = 0;
    for (int i = 0; i < m->n_stations; i++)
        if (map_station(m, i)->door == DOOR_IN)
            has_entrance = 1;

    if (map_seed_count(m) == 0 && !has_entrance)
        die_load(path, 0, NULL,
                 "nothing to seed — every station waits for a buffered "
                 "value, so the map cannot ever start");

    return m;
}
/* }}} */

/* {{{ map_instantiate_file() */
/*
 * **Bring a description inside a program that already exists** (issue
 * 217) — the operation this whole file turns out to have been, with
 * the program fixed at "a fresh empty one".
 *
 * **It is instantiating a template, not merging two programs.** There
 * is no second running program being picked up and carried, no handle
 * that becomes invalid, no table stitched onto another table. There
 * is a *description* and there is a table with some number of
 * stations in it; this builds new stations for the description's
 * stations and wires them the way the description says. One
 * description can be instantiated as many times into one program as
 * anybody likes, with nothing shared between the copies — separate
 * stations, separate buffers, separate constants.
 *
 * **So no wire is rewritten.** The description says its third station
 * feeds its fifth; that becomes wherever the third landed feeding
 * wherever the fifth landed. Nothing that already exists is
 * renumbered, so the invariant this engine rests on — an index means
 * what it meant — is not approached, let alone bent.
 *
 * **Legal at any moment**, because every operation it is made of is:
 * adding a station, naming one, placing a box, configuring a port,
 * drawing a wire. A program with workers in flight gains a subgraph
 * the same way it gains a station.
 *
 * The caller gets a handle it can find the instance's doors through,
 * and **the doors are all it should want**. A parent wiring into an
 * interior station of an instance is reaching inside, which is the
 * thing the marks exist to stop happening by accident.
 */
map_instance_t map_instantiate_file(map_t *m, const char *path)
{
    /*
     * **Where the stations landed comes back from the built function
     * itself**, because nothing else can know. Adding a station hands
     * back a freed place before it grows the table, so a program that
     * has had removals gets whatever holes exist in whatever order,
     * and the parent wants the doors in the order the description
     * declared them rather than in table order.
     */
    map_instance_t in;
    build_from_file(m, path, &in);
    return in;
}
/* }}} */

/* {{{ map_instance_door() / map_instance_free() */
/*
 * **The nth station of this instance facing that way**, or -1.
 *
 * This is the whole of what a parent is entitled to know about
 * something it brought inside itself. It could reach any of the
 * instance's stations through the handle — the translation table is
 * right there — and doing so would be reaching inside a thing whose
 * author may rename or restructure anything that is not a door.
 *
 * A program may have several of each, so the nth rather than the
 * only. They come back in the order the description declared them,
 * which is the one order a description can be said to have.
 */
static int map_instance_door(map_t *m, const map_instance_t *in,
                             int facing, int nth)
{
    int seen = 0;
    for (int i = 0; i < in->count; i++) {
        station_t *s = map_station(m, in->station[i]);
        if (s->call && s->door == facing && seen++ == nth)
            return in->station[i];
    }
    return -1;
}

int map_instance_entrance(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_IN, nth);
}

int map_instance_result(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_OUT, nth);
}

/*
 * The handle goes; the stations stay. Nothing in the running program
 * refers to this — it was the reader's note to itself about where
 * things landed, and a parent keeps it only for as long as it is
 * still deciding what to wire.
 */
void map_instance_free(map_instance_t *in)
{
    free(in->station);
    in->station = NULL;
    in->count = 0;
}
/* }}} */

/* {{{ map_add_part() */
/*
 * **Adding a box and adding a map are one operation** (issue 217).
 *
 * A map is a list of boxes and the wiring between them; a box is a
 * list of one. That is the whole difference, and once it is said that
 * way the two stop being different acts — adding a map means walking
 * its list, instantiating each of its boxes and then connecting them
 * the way it says, and adding a box means walking a list of length
 * one and connecting nothing.
 *
 * **What comes back is the same kind of thing either way.** A part is
 * where values go in and where they come out. For a map those are the
 * stations it declared as doors. **For a single box they are the same
 * station**, because a box's own input ports are its way in and its
 * own output port is its way out — a box is a map of one station
 * whose doors are itself.
 *
 * That is what makes a handle safe to hand around without ever taking
 * it apart: everything that consumes one takes it whole.
 *
 * **Which kind it is, is resolved rather than guessed.** A box lives
 * in the binary and a description lives on disk, so both are looked
 * for. Finding both is refused as ambiguous rather than settled by an
 * order nobody can see; finding neither is refused naming both places
 * that were searched.
 */
const char *map_add_part(map_t *m, const char *what, map_part_t *out)
{
    static _Thread_local char said[512];

    if (!what || !*what)
        return "adding a part with no name";

    const box_place_t *box = box_place_find(what);
    FILE *described = fopen(what, "r");
    if (described)
        fclose(described);

    if (box && described) {
        snprintf(said, sizeof said,
                 "'%s' is both a box in this binary and a description on "
                 "disk — say which by using a path that is not also a box "
                 "name", what);
        return said;
    }

    if (box) {
        /* A list of one. Its doors are itself. */
        int at = map_add_station(m);
        if (at < 0)
            return "the station table would not grow";
        map_place_box(m, at, what, STATION_PLAIN);
        out->entrance = at;
        out->result = at;
        return NULL;
    }

    if (described) {
        map_instance_t in = map_instantiate_file(m, what);
        out->entrance = map_instance_entrance(m, &in, 0);
        out->result = map_instance_result(m, &in, 0);
        map_instance_free(&in);
        if (out->result < 0) {
            snprintf(said, sizeof said,
                     "'%s' declares no way out, so nothing can be taken "
                     "from it", what);
            return said;
        }
        return NULL;
    }

    snprintf(said, sizeof said,
             "'%s' is neither a box compiled into this program nor a "
             "description that can be read from disk", what);
    return said;
}
/* }}} */

/* {{{ map_connect_parts() */
/*
 * **A wire from one part's way out to another part's way in**, which
 * is the only wire a composing caller ever needs to draw (issue 217).
 *
 * For two single boxes this is the ordinary wire, because a box's
 * doors are itself. For two maps it crosses what used to be a seam
 * and there is nothing there to cross — after instantiation there are
 * stations with indices, the way there always were.
 *
 * The port numbers are the ones a wire has always had: which output
 * port of the producing station, and which input port of the
 * receiving one. A comparator's three outcomes are reachable this way
 * exactly as before.
 */
const char *map_connect_parts(map_t *m, map_part_t from, int from_port,
                              map_part_t to, int to_port)
{
    static _Thread_local char said[256];

    if (from.result < 0) {
        snprintf(said, sizeof said,
                 "wiring out of a part that has no way out");
        return said;
    }
    if (to.entrance < 0) {
        snprintf(said, sizeof said,
                 "wiring into a part that declares no way in — a program "
                 "that takes no arguments cannot be fed");
        return said;
    }
    return map_wire(m, from.result, from_port, to.entrance, to_port);
}
/* }}} */

/* {{{ map_seed_count() */
int map_seed_count(map_t *m)
{
    return m->seeded;
}
/* }}} */
