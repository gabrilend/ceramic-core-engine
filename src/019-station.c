/*
 * 019-station.c — building and dismantling the station table.
 *
 * What this is: the structural half of the engine. It allocates the
 * table of stations, hangs input and output ports off them, and tears
 * it all down. Nothing in this file moves a value; motion lives in
 * the delivery file. Data structures here, dataflow there — an error
 * in one is then findable without reading the other.
 *
 * How it does it, in general terms: one allocation for the table,
 * one per station's port array, one per ring buffer, ports and
 * destinations as small linked nodes created as wires are declared.
 * Every cross-reference is an index, so nothing here ever needs
 * fixing up when storage grows elsewhere.
 */
#include "018-station.h"
#include "091-stopping.h"   /* an invalid operation ends the program (issue 106) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ring buffers start small on purpose: growth is cheap, proven, and
 * worth seeing in the demo; a generous initial size would only hide
 * the mechanism. The depth itself is IN_PORT_DEFAULT_CAPACITY, declared
 * beside the record in the header because the port's contract is
 * where a reader looks for it — and because issue 210b gave a port a
 * way to ask for a different one, which means two places now have to
 * agree on what "unless somebody says otherwise" is.
 */

/* {{{ fail() */
/*
 * Construction errors are author errors: the map being described is
 * wrong, and building on top of a wrong description helps nobody.
 * Say what, where, and stop.
 */
static void fail(const char *what)
{
    /*
     * **An invalid operation ends the program** (issue 106), with an
     * exit code that says which kind of fault it was: one a caller
     * can correct and retry. It used to abort, which left a core and
     * told a shell script nothing.
     *
     * There is no map to hand over here, because this helper is
     * reached from places that hold one and places that do not, and a
     * report about the wrong program is worse than no report. What
     * dies with a report is the path that knows which program it was
     * editing.
     */
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    sora_stop_now(NULL, SORA_EXIT_BAD_CALL, said);
}

/* {{{ fail_resource() */
/*
 * The other kind, and the distinction is the point (issue 106): out
 * of memory is not something a caller can correct and retry, so
 * anything that retries on failure has to be able to tell the two
 * apart in code rather than in prose.
 */
static void fail_resource(const char *what)
{
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    sora_stop_now(NULL, SORA_EXIT_NO_RESOURCE, said);
}
/* }}} */
/* }}} */

/* {{{ slot_stride() */
/*
 * How many bytes one slot occupies: its value, then its state, then
 * enough padding that the next slot's value is aligned too.
 *
 * The alignment is inferred rather than known, and the inference is
 * the only subtle line in this file. The registry carries every
 * type's *size* and no type's *alignment* — nothing has needed the
 * latter before, because a plain array of values strided by their own
 * size is aligned for free. Adding a state byte per slot breaks that
 * for the first time.
 *
 * What rescues it is a rule the C standard guarantees: a type's
 * alignment always divides its size. So the largest power of two
 * dividing elem_size is *at least* the alignment the type wants, and
 * rounding the stride up to it is safe without knowing what the type
 * actually is. Sixteen is the ceiling because no ordinary C type
 * needs more than max_align_t, and rounding past it would only waste
 * memory.
 *
 * The cost is worth stating plainly. A port of four-byte integers
 * goes from four bytes per slot to eight — the state needs a byte and
 * the alignment rounds it to four. A port of two-hundred-byte structs
 * goes from two hundred to two hundred and eight. So the overhead is
 * large in proportion exactly where it is small in absolute terms,
 * and negligible where the values are big, which is the case this
 * whole line of work is about.
 */
static int slot_stride(int elem_size)
{
    int align = 1;
    while (align < 16 && elem_size % (align * 2) == 0)
        align *= 2;
    int total = elem_size + (int)sizeof(_Atomic unsigned char);
    return (total + align - 1) / align * align;
}
/* }}} */

/* {{{ in_port_page_at() */
/*
 * The page holding a given page number, counted from the first.
 *
 * A walk down a short list, and the cost paging charges (issue 210e).
 * It is bounded by how many pages a port has, which is one until a
 * consumer falls behind its producer — and a port deep enough for
 * this walk to matter is one phase 7's buffer report is already
 * shouting about, so the long chain is a symptom of a problem the
 * engine is supposed to be complaining about rather than absorbing.
 *
 * The scan does not use this per candidate. It resolves a page once
 * and then follows `next`, which is what keeps a sweep to one pointer
 * hop per page boundary instead of a walk per slot.
 */
static in_port_page_t *in_port_page_at(const in_port_t *sl, int page)
{
    in_port_page_t *pg = sl->pages;
    while (page-- > 0 && pg)
        pg = atomic_load_explicit(&pg->next, memory_order_acquire);
    return pg;
}
/* }}} */

/* {{{ in_port_slot() */
void *in_port_slot(const in_port_t *sl, int index)
{
    in_port_page_t *pg = in_port_page_at(sl, index / sl->page_slots);
    if (!pg) {
        /* An ordinal past the last page means the caller computed a
         * position the port does not have. Nothing should be able to:
         * the scan bounds itself by the capacity, and the capacity is
         * the sum of the pages. Stopping is right because continuing
         * would read whatever follows the list. */
        fprintf(stderr, "station: slot %d asked for on a port that has "
                        "%d\n", index, sl->capacity);
        abort();
    }
    return pg->slots + (size_t)(index % sl->page_slots) * (size_t)sl->stride;
}
/* }}} */

/* {{{ in_port_add_page() */
/*
 * One more page on the end. Used both to give a port its first page
 * and to grow it, because they are the same act (issue 210e) — which
 * is the shape the station table already uses one level up.
 */
in_port_page_t *in_port_add_page(in_port_t *sl)
{
    /* Zeroed rather than merely allocated, because a slot's state is
     * part of it and empty is zero (issue 210c) — a fresh page has to
     * be a page of *empty* slots, or the first reader to reach it
     * would find whatever the allocator left behind and believe it. */
    in_port_page_t *pg = calloc(1, sizeof *pg
                                + (size_t)sl->page_slots * (size_t)sl->stride);
    if (!pg) fail_resource("out of memory for a page of a ring buffer");

    if (!sl->pages) {
        sl->pages = pg;
    } else {
        in_port_page_t *last = sl->pages;
        in_port_page_t *next;
        while ((next = atomic_load_explicit(&last->next,
                                            memory_order_relaxed)) != NULL)
            last = next;
        /* Release, so a scanner that follows this link sees a page of
         * fully-zeroed slots rather than whatever calloc had not yet
         * made visible. Only growth ever writes a link, and growth
         * holds the station's mutex, so this is the one writer. */
        atomic_store_explicit(&last->next, pg, memory_order_release);
    }
    /* **After** the page is linked, never before.** Seeing the larger
     * capacity is what tells a scanner the slots exist; publishing the
     * number first would invite a sweep into slots that are not
     * reachable yet. */
    atomic_fetch_add_explicit(&sl->capacity, sl->page_slots,
                              memory_order_release);
    return pg;
}
/* }}} */

/* {{{ in_port_free_pages() */
void in_port_free_pages(in_port_t *sl)
{
    in_port_page_t *pg = sl->pages;
    while (pg) {
        in_port_page_t *next = atomic_load_explicit(&pg->next,
                                                    memory_order_relaxed);
        free(pg);
        pg = next;
    }
    sl->pages = NULL;
    atomic_store_explicit(&sl->capacity, 0, memory_order_relaxed);
}
/* }}} */

/* {{{ slot_move_at() */
/*
 * The state machine, on a slot the caller has already located.
 *
 * Split out from the index-taking form because the scan walks pages
 * and therefore already holds the address (issue 210e). Going back
 * through an ordinal would make it resolve a page per candidate,
 * turning a sweep into a walk down the page list for every slot it
 * looks at — quadratic in the number of pages, on the hot path,
 * to recompute something it just had.
 */
int slot_move_at(void *slot, int elem_size, int from, int to)
{
    /* The state sits immediately after the value bytes. Reached
     * through a byte pointer and an explicit offset rather than a
     * struct member, because a slot's size is not known until the
     * port exists — the value in the middle of it is as wide as the
     * parameter this port feeds. */
    _Atomic unsigned char *state = (_Atomic unsigned char *)
        ((unsigned char *)slot + elem_size);

    unsigned char expected = (unsigned char)from;
    /* Acquire-release on success: a reader that wins ready-to-claimed
     * must see every byte the writer copied before it published, and
     * a writer that wins claimed-to-empty must not have its next copy
     * hoisted above the release. Acquire on failure, because a caller
     * that lost still read the state and will decide what to do from
     * it. */
    return atomic_compare_exchange_strong_explicit(
        state, &expected, (unsigned char)to,
        memory_order_acq_rel, memory_order_acquire);
}
/* }}} */

/* {{{ slot_state_at() / slot_take_at() */
/*
 * The claim side's transition, without a compare-and-swap (issue
 * 210d, step 6).
 *
 * **A ready slot has exactly one possible mover, and under the
 * station's mutex that mover is us.** Other claimers are excluded by
 * the lock. A writer never touches a ready slot: it moves a slot
 * empty → reserved → ready, and its search for an empty one
 * compare-and-swaps from *empty*, which simply fails against a ready
 * slot without writing anything. So nothing can change this byte
 * between reading it and writing it, and a read-modify-write would be
 * paying for an exclusion that the lock has already bought.
 *
 * That matters because the *search* asks this question of every
 * candidate it walks past. A compare-and-swap per candidate is what
 * made small values measurably slower when the scan replaced index
 * arithmetic; a load per candidate does not.
 *
 * The acquire is still needed and is the whole reason these are not
 * plain memory accesses: it is what makes the writer's copied bytes
 * visible to the worker that takes the slot from ready.
 */
int slot_state_at(const void *slot, int elem_size)
{
    const _Atomic unsigned char *state = (const _Atomic unsigned char *)
        ((const unsigned char *)slot + elem_size);
    return atomic_load_explicit(state, memory_order_acquire);
}

void slot_set_at(void *slot, int elem_size, int to)
{
    _Atomic unsigned char *state = (_Atomic unsigned char *)
        ((unsigned char *)slot + elem_size);
    /* Release, so that whoever next acquires this slot sees everything
     * this thread did to its bytes beforehand. */
    atomic_store_explicit(state, (unsigned char)to, memory_order_release);
}
/* }}} */

/* {{{ in_port_slot_move() */
/*
 * The same transition, named by ordinal rather than by address, for
 * every caller that has an index in hand and no page to walk from.
 */
int in_port_slot_move(const in_port_t *sl, int index, int from, int to)
{
    return slot_move_at(in_port_slot(sl, index), sl->elem_size, from, to);
}
/* }}} */

/* {{{ static int add_shelf() */
/*
 * One more shelf, and its pointer written into the short array that
 * names them. That array holds addresses rather than mutexes, so
 * growing it by reallocation is safe — the same kind of copy the
 * pool's ring already does. No station record is ever copied.
 */
static int add_shelf(map_t *m)
{
    station_t *shelf = calloc(STATIONS_PER_SHELF, sizeof *shelf);
    if (!shelf)
        return -1;
    station_t **shelves = realloc(m->shelves,
                                  (size_t)(m->n_shelves + 1) * sizeof *shelves);
    if (!shelves) {
        free(shelf);
        return -1;
    }
    shelves[m->n_shelves] = shelf;
    m->shelves = shelves;
    m->n_shelves++;
    return 0;
}
/* }}} */

/* {{{ map_create() */
map_t *map_create_empty(void)
{
    map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");
    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);
    return m;
}
/* }}} */

/* {{{ map_create() */
map_t *map_create(int n_stations)
{
    if (n_stations <= 0)
        fail("a map needs at least one station");

    map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");

    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);

    /*
     * Shelves enough for what was asked for (issue 211). Asking for a
     * count up front is now a convenience rather than a commitment:
     * the table grows a shelf at a time afterwards, and nothing
     * already placed ever moves.
     *
     * Reserved directly rather than by calling map_add_station in a
     * loop, because that call hands back the first place nobody has
     * filled — which is the same place every time until somebody
     * fills it. Reserving N places and filling them is a different
     * act from asking for somewhere to put one thing.
     */
    while (n_stations > m->n_shelves * STATIONS_PER_SHELF)
        if (add_shelf(m) < 0)
            fail_resource("out of memory for the station table");
    atomic_store_explicit(&m->n_stations, n_stations, memory_order_release);

    return m;
}
/* }}} */

/* {{{ map_add_station() */
int map_add_station(map_t *m)
{
    /* Exclusive, under the lock every other structural change already
     * takes (issue 704). Two threads each finding the same free place,
     * or each deciding the shelves are full, would otherwise hand two
     * callers one index. The hand-raising ring the note asked for is
     * not built and is moot: adding a shelf is one allocation and one
     * pointer write, so there is no long stretch for anybody to raise a
     * hand during. `strategems/raise-your-hand.md` keeps the pattern
     * and the lesson that displaced it. */
    pthread_mutex_lock(&m->rewire_mutex);

    /*
     * A freed place first. Removing a station is what removes the
     * wires to it (issue 216), so a place whose shim is clear holds
     * nothing stale and can simply be taken. A program that adds and
     * removes forever therefore reaches a steady size rather than
     * climbing.
     */
    int count = atomic_load_explicit(&m->n_stations, memory_order_acquire);
    for (int i = 0; i < count; i++) {
        station_t *s = map_station(m, i);
        if (!s->call && !atomic_load_explicit(&s->removed,
                                              memory_order_acquire)) {
            pthread_mutex_unlock(&m->rewire_mutex);
            return i;
        }
    }

    if (count >= m->n_shelves * STATIONS_PER_SHELF && add_shelf(m) < 0) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return -1;
    }

    /*
     * Published last, and by one write. Everything about the record is
     * already zeroed by the shelf's own allocation, so a reader that
     * sees this count sees a station that is complete — an empty one,
     * which is exactly what a station is before anything is placed in
     * it.
     */
    atomic_store_explicit(&m->n_stations, count + 1, memory_order_release);
    pthread_mutex_unlock(&m->rewire_mutex);
    return count;
}
/* }}} */

/* {{{ map_place() */
void map_place(map_t *m, int station, task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size)
{
    if (station < 0 || station >= m->n_stations)
        fail("placing a box at a station index outside the table");
    if (kind < 0 || kind >= STATION_KIND_COUNT)
        fail("placing a box of a kind that does not exist");
    if (n_in_ports < 0)
        fail("a station cannot have a negative number of slots");

    station_t *s = map_station(m, station);
    if (s->call)
        fail("placing a box at a station already occupied");

    pthread_mutex_init(&s->mutex, NULL);
    s->call = shim;
    s->kind = (unsigned char)kind;
    s->out_size = out_size;
    s->cursor = 0;

    s->n_in_ports = n_in_ports;
    s->in_ports = NULL;
    if (n_in_ports > 0) {
        s->in_ports = calloc((size_t)n_in_ports, sizeof *s->in_ports);
        if (!s->in_ports) fail_resource("out of memory for a port array");
    }

    for (int i = 0; i < n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (elem_sizes[i] <= 0)
            fail("a port's element size must be positive");
        /* Every port starts life as a ring buffer — the default the
         * map format also assumes (issue 601). Becoming a static, or
         * having its source taken away, is a conversion applied
         * afterwards; neither one frees what is allocated here
         * (issue 210b). */
        sl->kind = IN_PORT_RING;
        sl->elem_size = elem_sizes[i];
        sl->stride = slot_stride(sl->elem_size);
        /* The first page, which is the same act as growing (issue
         * 210e): a port with one page and a port with nine differ
         * only in how many times this has happened. */
        sl->pages = NULL;
        sl->capacity = 0;
        sl->page_slots = IN_PORT_DEFAULT_CAPACITY;
        in_port_add_page(sl);
        sl->read_hint = 0;
        sl->write_hint = 0;
        sl->held = 0;

        /* The other storage, allocated at the same moment and for the
         * same reason (issues 210b, 401): a port has room for both a
         * buffer and a constant whatever it is currently for, so
         * changing which one is in effect is a field write and never
         * an allocation. Zeroed, so a port whose constant has not been
         * set holds zeroes rather than whatever was there — though
         * nothing reads it until constant_set says somebody wrote it. */
        sl->constant = calloc(1, (size_t)sl->elem_size);
        if (!sl->constant) fail_resource("out of memory for a port's constant");
        sl->constant_string = NULL;
        sl->constant_set = 0;
    }
}
/* }}} */

/* {{{ static void station_label_into() */
/*
 * The name a map file gave a station, or its index when nothing gave
 * it one. A program built by calling the surface has no names, and a
 * complaint that says "?" about it is one nobody can act on.
 */
static void station_label_into(map_t *m, int i, char *out, size_t room)
{
    if (m->station_names && i < m->n_named && m->station_names[i])
        snprintf(out, room, "%s", m->station_names[i]);
    else
        snprintf(out, room, "%d", i);
}
/* }}} */

/* {{{ static void no_such_port_into() */
/*
 * **One sentence for "that port does not exist", written once**
 * (issue 210g).
 *
 * The loader used to compose its own, and it was the better of the
 * two: it named the box, counted its ports, and remembered that a
 * comparator carries one more than its parameter list shows. The
 * surface's said only which numbers disagreed. Two spellings of one
 * refusal is the small version of the same fault two construction
 * paths were — so the good one moved down here, where every caller
 * reaches it, and the loader kept nothing of its own but the file and
 * the line it prefixes this with.
 *
 * The threshold clause is why this cannot be a format string at each
 * call site: a comparator's last port is not one of the box's
 * parameters, so a reader counting parameters in the box source finds
 * one fewer than the refusal names and concludes the engine is
 * confused.
 */
static void no_such_port_into(map_t *m, int station, int port,
                              char *out, size_t room)
{
    station_t *s = map_station(m, station);
    char who[64];
    station_label_into(m, station, who, sizeof who);
    snprintf(out, room,
             "station '%s' has no port %d — '%s' has %d port%s (its "
             "parameters%s)",
             who, port, s->box_name ? s->box_name : "?", s->n_in_ports,
             s->n_in_ports == 1 ? "" : "s",
             s->kind == STATION_COMPARATOR ? ", plus the threshold" : "");
}
/* }}} */

/* {{{ map_in_port_start_depth() */
/*
 * **A refusal travels rather than stopping here** (issue 210g).
 *
 * This used to die on the spot, which was the last port operation
 * that did. A caller reading a file collects every mistake in it and
 * presents them together, and it cannot collect what killed the
 * process — so a depth on a port that does not exist would have been
 * the one fault in a map file that hid every fault after it.
 *
 * Existing callers that pass sound arguments are unaffected: a
 * function that returned nothing now returns nothing they have to
 * look at, and the only difference is that the ones who *do* look can
 * say where the trouble was.
 */
const char *map_in_port_start_depth(map_t *m, int station, int port, int slots)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said,
                 "station %d is outside the table", station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    /* One slot is a legitimate depth. It used to take two, because a
     * spare was held back so that head meeting tail could mean empty
     * rather than full; a slot that carries its own state needs no
     * such stand-in, and every slot is usable (issue 210c). */
    if (slots < 1) {
        snprintf(said, sizeof said,
                 "a ring buffer needs at least one slot, and %d was asked for",
                 slots);
        return said;
    }

    in_port_t *sl = &s->in_ports[port];
    if (sl->held != 0) {
        char who[64];
        station_label_into(m, station, who, sizeof who);
        snprintf(said, sizeof said,
                 "%s.%d already holds values — this is a starting depth, and "
                 "the start has been and gone", who, port);
        return said;
    }

    /* The starting depth sets the **page size**, not merely the first
     * page's size (issue 210e). Every page a port ever adds is this
     * big, so asking for deep buffers gets large pages everywhere
     * rather than a long chain of small ones — the same lever pointed
     * at the same problem, which is what keeps the page walk short
     * for a program that knew it would need depth.
     *
     * The port is empty, which the check above proved, so there is
     * nothing to carry across: drop the pages it has and give it one
     * of the new size. */
    in_port_free_pages(sl);
    sl->page_slots = slots;
    in_port_add_page(sl);
    sl->read_hint = 0;
    sl->write_hint = 0;
    return NULL;
}
/* }}} */

/* {{{ map_in_port_convert() */
void map_in_port_convert(map_t *m, int station, int port, int kind)
{
    /* A case of the one configuration operation (issue 210g), kept as
     * a name because "convert this port" is what callers already say.
     * Passing no text means *the value it had before*, which is why
     * becoming a static again works and becoming one for the first
     * time is refused here. */
    const char *no = map_configure_port(m, station, port, kind, NULL);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ map_configure_port() */
/*
 * **The one operation that says where a port's values come from**
 * (issue 210g): a station, a port, a source, and — when the source is
 * a value — the value itself, written as text.
 *
 * Binding a constant, taking a source away, and giving a port back to
 * the arrows were three calls with three shapes, some dying on
 * refusal and some returning a code, and the difference between them
 * was history rather than meaning. They are one thing now and the
 * other two are cases of it. What that buys is not tidiness: it is
 * that there is one description of what it means to give a port a
 * source, and it is executable — the loader calls it while reading a
 * file, and a debugger or a workbench calls the same one on a running
 * program.
 *
 * **Text distinguishes the two ways to become a constant.** Given
 * text, the port takes that value. Given none, it goes back to the
 * value it held before, which is legitimate because a constant
 * survives being converted away exactly as waiting values do (issue
 * 210f) — and which is refused when there is no such value, because
 * the tag would then be in effect over storage nobody ever wrote.
 *
 * **Returns a refusal rather than stopping the program**, so that a
 * caller reading a file can collect every mistake in it and present
 * them together instead of one per run. That is the policy issue 212
 * settles and this is the surface it applies to.
 *
 * One thing still stops the program: text that does not parse. The
 * reader dies where the malformed value is, naming the station, the
 * port and the field, and moving that onto this return path belongs
 * with the rest of the refusal policy rather than being half done
 * here.
 */
const char *map_configure_port(map_t *m, int station, int port,
                               int source, const char *text)
{
    /* Per thread, because two threads may be editing two different
     * maps and a shared buffer would let one overwrite the other's
     * complaint. Valid until this thread's next refusal. */
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said,
                 "station %d is outside the table", station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        /* The loader's wording, which named the box and remembered
         * the comparator's threshold, moved down here so that every
         * caller gets it and nobody keeps a second copy (issue 210g). */
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    if (source < 0 || source >= IN_PORT_KIND_COUNT) {
        snprintf(said, sizeof said,
                 "there is no such source for a port");
        return said;
    }

    if (source == IN_PORT_STATIC) {
        if (text) {
            /* Binding parses the text and sets the tag together, so a
             * value that will not parse never leaves the port in a
             * state that claims to hold one. */
            map_in_port_static_text(m, station, port, text);
            return NULL;
        }
        if (!s->in_ports[port].constant_set) {
            snprintf(said, sizeof said,
                     "station %d port %d has never held a value, so there is "
                     "none to go back to — give it one as text",
                     station, port);
            return said;
        }
    }

    /* Under the station's mutex, as one of the rare structural
     * operations, so no readiness walk sees a port mid-change.
     * Nothing is freed and nothing is cleared: the whole of the change
     * is the tag, which is what makes it cheap and lossless. */
    pthread_mutex_lock(&s->mutex);
    s->in_ports[port].kind = (unsigned char)source;
    pthread_mutex_unlock(&s->mutex);
    return NULL;
}
/* }}} */

/* {{{ map_check_sources() */
/*
 * **Every parameter needs somewhere to get a value** (issue 210g).
 *
 * A port with no source is not an error while a program is being
 * assembled — it is the ordinary state of a station that exists
 * before anybody has finished wiring it, and being able to exist that
 * way is what lets a program be built a piece at a time. It becomes
 * an error at the moment somebody says the program is finished.
 *
 * Caught here rather than at the first task, because here it can name
 * the station and the port while the person who mis-wired them is
 * still looking. A station whose port has no source simply never
 * becomes ready, which is correct behaviour and a terrible way to
 * find out: the symptom is a program that runs and quietly does less
 * than it was asked to.
 *
 * **Every one of them, collected**, rather than the first — somebody
 * fixing a new program wants the whole list, not one per run. The
 * count is reported even when the list is trimmed, so a long one
 * never reads as a short one.
 *
 * **The check has no exceptions and never will.** A parameter a box
 * could do without was proposed and refused (issue 210h), because it
 * would have been the only exemption to the rule that a station runs
 * when every one of its slots holds a value. So this is unqualified:
 * a port with no source is an error, full stop.
 */
const char *map_check_sources(map_t *m)
{
    static _Thread_local char said[512];
    int used = 0, found = 0;

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        /* An empty place in the table is not a station (issue 216). */
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) != IN_PORT_NONE)
                continue;
            found++;
            if (used < (int)sizeof said - 64) {
                /* The name a map file gave it, or its index when
                 * nothing gave it one — a program built by calling
                 * this surface has no names, and a complaint that
                 * says "?" about it is a complaint nobody can act
                 * on. Both spellings read the same way: which
                 * station, then which port.
                 *
                 * Through the shared speller, which is the third
                 * copy of these four lines being retired. The copies
                 * had drifted in a way that mattered: this one read
                 * the names array by station index without asking
                 * how long it is, and stations added after the last
                 * naming call leave it shorter than the table. */
                char who[64];
                station_label_into(m, i, who, sizeof who);
                used += snprintf(said + used, sizeof said - (size_t)used,
                                 "%s%s.%d has no source",
                                 used ? "; " : "", who, j);
            }
        }
    }

    if (!found)
        return NULL;
    if (found > 1 && used < (int)sizeof said - 32)
        snprintf(said + used, sizeof said - (size_t)used,
                 " (%d ports in all)", found);
    return said;
}
/* }}} */

/* {{{ map_name_station() */
/*
 * **What to call a station** (issue 212), which is the sixth thing
 * construction has to be able to say.
 *
 * The engine never reads these — every wire is an index, and that is
 * deliberate. They exist so a program can be *written back out* as a
 * file that reads in again, and so a person watching a live view sees
 * something other than numbers. A program with no names still runs
 * perfectly; it simply cannot be described on disk.
 *
 * It had to become an operation rather than staying a thing only the
 * loader did. The loader used to copy every name onto the map in one
 * go once its own lookup table had served, which works exactly while
 * the only way to build a program is to read a file. A program built
 * by calling this surface would otherwise dump as a row of indices —
 * and then a file-built program and a surface-built one could not be
 * compared, which is the proof that there is one construction path
 * rather than two that agree by coincidence.
 *
 * The array grows with the table, because stations are added one at a
 * time now rather than counted in advance.
 */
const char *map_name_station(map_t *m, int station, const char *name)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    if (!name || !*name)
        return "a station's name cannot be empty";

    if (m->n_named < m->n_stations) {
        char **grown = realloc(m->station_names,
                               (size_t)m->n_stations * sizeof *grown);
        if (!grown)
            return "out of memory naming a station";
        for (int i = m->n_named; i < m->n_stations; i++)
            grown[i] = NULL;
        m->station_names = grown;
        m->n_named = m->n_stations;
    }

    free(m->station_names[station]);
    m->station_names[station] = strdup(name);
    if (!m->station_names[station])
        return "out of memory naming a station";
    return NULL;
}
/* }}} */

/* {{{ map_designate_output() */
/*
 * **Say that this station is a place the program's results come
 * from** (issue 209).
 *
 * It stays an ordinary station: same shape, same readiness, running
 * whatever box it was placed with or none. The designation adds
 * exactly one rule — when its output port is wired nowhere, values
 * are held rather than discarded — and one meaning, which is that a
 * parent composing this program has somewhere to wire from and a
 * person reading it can tell which station is the point.
 *
 * A program may have **several**, each with its own input ports and
 * its one output port, because a box returns one value and so a
 * station has one output port and so a program output is one station.
 * The alternative — one station whose output ports each owned a
 * subset of its inputs — needs a box returning several values, which
 * C does not have, and would be the only thing in the engine with
 * several readiness checks over subsets of its ports.
 */
const char *map_designate_output(map_t *m, int station)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (s->out_size == 0) {
        /* A station whose box returns nothing has no output port, so
         * there is nothing for a parent to wire from and nothing to
         * hold. Refused rather than accepted-and-useless, because the
         * mistake is almost certainly the wrong station. */
        snprintf(said, sizeof said,
                 "station %d returns nothing, so it has no results to be "
                 "the source of", station);
        return said;
    }
    s->door = DOOR_OUT;
    return NULL;
}
/* }}} */

/* {{{ map_start_beside() */
map_t *map_start_beside(map_t *parent)
{
    if (!parent->pool)
        fail("starting a program beside one that has not started itself");

    map_t *m = map_create_empty();
    /*
     * The same workers, and nothing else shared. A task now says
     * which program it belongs to, so a worker finishing one does not
     * need to know whose pool it is running on — which is what makes
     * this a field assignment rather than a mechanism.
     */
    m->pool = parent->pool;
    m->pool_is_borrowed = 1;
    return m;
}
/* }}} */

/* {{{ map_designate_input() */
/*
 * **Say that this station is where the outside delivers** (issue
 * 213), which is the other door and the same design.
 *
 * Without it, a value gets into a running program exactly one way:
 * somebody holding the program calls the delivery entry naming a
 * station and a port. That works and it is what every test does, and
 * it means **the caller has to know the program's insides**. Rename
 * an interior station and every caller breaks. That is not
 * encapsulation — the program has no surface, only internals that
 * happen to be reachable.
 *
 * The mark says which ports the outside is allowed to deliver to.
 * Everything after that is an ordinary delivery down an ordinary
 * wire, which is why this needs no new mechanism in the delivery
 * path at all.
 *
 * **One output port, so one station per argument group.** A box
 * returns one value, so a station has one output port, so a program
 * taking several unrelated arguments has several input stations. The
 * alternative wants a C function returning several values, and faking
 * it with a struct something downstream takes apart means a function
 * written to satisfy the engine — which is the thing this design will
 * not ask anybody for.
 *
 * Fan-out is a different thing and was always free: one input
 * station's output port may feed as many interior stations as it is
 * wired to.
 */
const char *map_designate_input(map_t *m, int station)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (s->door == DOOR_OUT) {
        /* A program whose entrance is its exit is not a program with
         * two doors; it is somebody having designated the wrong
         * station. Refused rather than quietly overwritten. */
        snprintf(said, sizeof said,
                 "station %d is already where results come from — a station "
                 "cannot be both doors", station);
        return said;
    }
    s->door = DOOR_IN;
    return NULL;
}
/* }}} */

/* {{{ map_deliver_argument() */
/*
 * **Deliver a value from outside the program** (issue 213).
 *
 * The difference between this and the ordinary delivery entry is not
 * mechanical — underneath it is the same call — it is *who may use
 * it*. This one refuses any station that is not a declared door, and
 * that refusal is the whole of what gives a program a surface. A
 * caller reaching an interior station is reaching inside, and the
 * point of the designation is that reaching inside stops being
 * possible by accident.
 *
 * The size is checked against the port, because a caller from outside
 * is exactly the caller least likely to be right about it — inside
 * the graph a wire was checked when it was drawn, and here there is
 * no wire, so this is the only moment.
 */
const char *map_deliver_argument(map_t *m, int station, int port,
                                 const void *value, int size)
{
    static _Thread_local char said[224];

    /*
     * **Shut, because somebody asked this program to wind down**
     * (issue 106). The entrance is the only way anything outside puts
     * work into a program, so refusing here is the whole of what
     * "stop accepting new work" can mean — and it is what lets the
     * queue drain and the ordinary ending fire.
     *
     * Asked first, before the station is even looked at, because a
     * closing program has nothing useful to say about which of its
     * doors somebody was aiming at.
     */
    if (atomic_load_explicit(&m->closing, memory_order_acquire)) {
        snprintf(said, sizeof said,
                 "this program is winding down and is not accepting new "
                 "work");
        return said;
    }

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (s->door != DOOR_IN) {
        snprintf(said, sizeof said,
                 "station %d is not a declared entrance — the outside may "
                 "only deliver to a program's input stations", station);
        return said;
    }
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said,
                 "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    if (size != s->in_ports[port].elem_size) {
        snprintf(said, sizeof said,
                 "that port takes %d bytes and %d were offered",
                 s->in_ports[port].elem_size, size);
        return said;
    }

    map_deliver_value(m, station, port, value);
    return NULL;
}
/* }}} */

/* {{{ map_output_waiting() / map_output_take() */
/*
 * The two halves of collecting a program's results from outside,
 * mirroring the call that writes a constant in (issue 209).
 *
 * **Both, because one is not usable without the other.** A caller
 * asked to drain results needs to know whether there are any, and
 * asking by taking and checking for failure makes "none waiting"
 * indistinguishable from "not an output station" without a second
 * question anyway.
 *
 * Under the station's own mutex, which is the same lock a worker
 * finishing a box takes to put a result there — unlike a slot, which
 * belongs to one worker, a held result belongs to the station until
 * somebody takes it.
 */
int map_output_waiting(map_t *m, int station)
{
    if (station < 0 || station >= m->n_stations)
        return 0;
    station_t *s = map_station(m, station);
    pthread_mutex_lock(&s->mutex);
    int n = s->n_held;
    pthread_mutex_unlock(&s->mutex);
    return n;
}

int map_output_take(map_t *m, int station, void *into, int size)
{
    if (station < 0 || station >= m->n_stations)
        return 0;
    station_t *s = map_station(m, station);
    if (size != s->out_size)
        fail("taking a result into something the wrong size for it");

    pthread_mutex_lock(&s->mutex);
    if (s->n_held == 0) {
        pthread_mutex_unlock(&s->mutex);
        return 0;
    }
    /* Oldest first, and the shuffle is deliberate over the
     * alternative. Results are taken far less often than they are
     * produced, and a caller draining them wants them in the order
     * the program produced them — which is the one ordering this
     * engine can still honestly offer, because a single station
     * produced them all in sequence. */
    memcpy(into, s->held, (size_t)size);
    s->n_held--;
    if (s->n_held > 0)
        memmove(s->held, (unsigned char *)s->held + size,
                (size_t)s->n_held * (size_t)size);
    pthread_mutex_unlock(&s->mutex);
    return 1;
}
/* }}} */

/* {{{ map_bring_up() */
const char *map_bring_up(map_t *m)
{
    static _Thread_local char said[768];
    int used = 0, faults = 0;

    /*
     * **A port with no source is said out loud and is not a fault**,
     * and which of those it is took resolving between two issues that
     * disagreed.
     *
     * Issue 210g asked for it to be a configuration error, caught
     * while somebody is still looking rather than on the first task
     * built minutes into a run. Issue 210b then made a port with no
     * source a state the map file can *spell*, so that a half-built
     * program could be written down and read back — and this issue
     * says plainly that a station may hold such a port indefinitely,
     * because that is what lets a program be assembled from nothing
     * and wired one arrow at a time.
     *
     * The later two win, and they are right: nothing breaks. Such a
     * station simply never becomes ready, which is the same outcome
     * as a buffered input nothing feeds. Refusing it would make
     * "add a station now, wire it in a moment" impossible to express,
     * which is the sequence this whole surface exists to make
     * ordinary.
     *
     * So it is a warning, and a loud one, because a station that
     * silently never runs is the hardest thing to notice from
     * outside.
     */
    const char *unsourced = map_check_sources(m);
    if (unsourced)
        fprintf(stderr, "map: WARNING: %s — %s will not run until %s\n",
                unsourced,
                strchr(unsourced, ';') ? "those stations" : "that station",
                strchr(unsourced, ';') ? "they are finished"
                                       : "it is finished");

    /*
     * **Which ports arrows land on, worked out once for the whole
     * program** (issue 212).
     *
     * This used to be asked per station: for each one, walk every
     * *other* station's destinations looking for arrows that land
     * here. That is the table walked once per station — quadratic in
     * the number of stations, which nobody minded at a dozen of them.
     *
     * Composing is what changes the number. A program brought into
     * another produces one table holding both, and a table that holds
     * several programs' worth of boxes is exactly how a dozen
     * stations stops being a dozen. Turning it round costs one array
     * and answers the same question: walk every destination in the
     * program once, marking where each one lands. The work becomes
     * proportional to the stations plus the wires, which is the size
     * of the thing being described rather than its square.
     *
     * One flat array of flags, indexed by a station's first port plus
     * the port number, so there is one allocation rather than one per
     * station. An empty place contributes no ports and is skipped,
     * which keeps the offsets honest without a special case.
     */
    int *first_port = calloc((size_t)m->n_stations + 1, sizeof *first_port);
    if (!first_port)
        return "out of memory checking a program";
    int total_ports = 0;
    for (int i = 0; i < m->n_stations; i++) {
        first_port[i] = total_ports;
        station_t *s = map_station(m, i);
        total_ports += s->call ? s->n_in_ports : 0;
    }
    first_port[m->n_stations] = total_ports;

    unsigned char *landed = calloc((size_t)(total_ports > 0 ? total_ports : 1),
                                   sizeof *landed);
    if (!landed) {
        free(first_port);
        return "out of memory checking a program";
    }
    for (int k = 0; k < m->n_stations; k++) {
        station_t *other = map_station(m, k);
        for (out_port_t *p = other->out_ports; p; p = p->next) {
            dest_set_t *set = out_port_dests(p);
            for (int di = 0; set && di < set->n; di++) {
                int at = set->items[di].station;
                int port = set->items[di].port;
                if (at < 0 || at >= m->n_stations)
                    continue;
                station_t *dest = map_station(m, at);
                if (!dest->call || port < 0 || port >= dest->n_in_ports)
                    continue;
                landed[first_port[at] + port] = 1;
            }
        }
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;   /* an empty place is not a station (issue 216) */

        char who[64];
        station_label_into(m, i, who, sizeof who);

        const unsigned char *landed_on = landed + first_port[i];

        int has_ring = 0, any_arrow = 0;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == IN_PORT_RING)
                has_ring = 1;
            any_arrow |= landed_on[j];
        }

        /* An arrow landing on a port with no source would have
         * nowhere to put its value at all — nothing to queue into and
         * nothing to overwrite.
         *
         * **A static destination stopped being one of these** (issue
         * 405). A value arriving there overwrites the constant, which
         * is how a constant gets computed at startup rather than
         * written down, and is a property of the wire rather than of
         * the box — so it is visible in the map file instead of
         * happening invisibly inside C. */
        for (int j = 0; j < s->n_in_ports; j++) {
            unsigned char k = atomic_load_explicit(&s->in_ports[j].kind,
                                                   memory_order_relaxed);
            if (landed_on[j] && k != IN_PORT_RING && k != IN_PORT_STATIC) {
                faults++;
                if (used < (int)sizeof said - 128)
                    used += snprintf(said + used, sizeof said - (size_t)used,
                                     "%san arrow lands on %s.%d, but that "
                                     "port is %s, not a buffer",
                                     used ? "; " : "", who, j,
                                     in_port_kind_name(k));
            }
        }

        /* Buffered inputs nothing feeds. Loud but not a fault: a
         * program under construction has these, and so does one fed
         * from outside by a test or a control surface. Silently never
         * running is the hardest thing to notice from outside, which
         * is why it is said at all. */
        /*
         * Buffered inputs nothing feeds. Loud but not a fault: a
         * program under construction has these, and so does one fed
         * from outside. Silently never running is the hardest thing
         * to notice from outside, which is why it is said at all.
         *
         * **A declared entrance is exempt, and that is not a special
         * case being carved out — it is the warning's own escape
         * clause becoming checkable.** The sentence has always ended
         * "unless something outside delivers into it"; a station
         * marked as a door is precisely one that something outside
         * delivers into (issue 213). Warning about it would be
         * telling somebody that the thing they just declared might
         * not happen.
         */
        if (has_ring && !any_arrow && s->door != DOOR_IN)
            fprintf(stderr,
                    "map: WARNING: station %s has buffered inputs that no "
                    "arrow feeds — unless something outside delivers into "
                    "it, it will never run\n", who);
    }

    free(landed);
    free(first_port);

    /*
     * **A program says where its results come from, or it is not
     * finished** (issue 209).
     *
     * Bringing a program up is a caller declaring it finished, and a
     * finished program that has never said what it produces has not
     * said what it is for. The parallel is a C function returning
     * void: it still declares its return, and the declaration is what
     * a caller reads.
     *
     * What the requirement buys is that a program's interface is
     * **total**. Without it there are two different ways to produce
     * nothing — no result station at all, and a result station nobody
     * wired anything into — and only the second is legible. The
     * engine cannot tell a program that deliberately does all its work
     * by side effect from one whose author forgot the results, because
     * a box is a C function and nothing about it says whether it
     * touches the world. Requiring the declaration moves that from
     * something the engine would have to guess into something the
     * program states, and a program that writes to disk and returns
     * nothing then declares exactly that.
     *
     * **A station with nothing wired into it satisfies this**, which
     * is the whole point: the declaration is the interface, and what
     * flows through it is a separate matter.
     *
     * It is asked here rather than while a program is being built,
     * because a program under construction legitimately has no result
     * station yet — the same reason the other whole-program checks
     * live here. A caller that never says it is finished is never
     * asked.
     */
    int has_result = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (s->call && s->door == DOOR_OUT)
            has_result = 1;
    }
    if (!has_result) {
        faults++;
        if (used < (int)sizeof said - 128)
            used += snprintf(said + used, sizeof said - (size_t)used,
                             "%sthis program never says where its results "
                             "come from — one station has to be marked as "
                             "the way out, even when nothing is wired into "
                             "it", used ? "; " : "");
    }

    if (faults) {
        if (used < (int)sizeof said - 48)
            snprintf(said + used, sizeof said - (size_t)used,
                     " — nothing was started");
        return said;
    }

    /*
     * The one place anything walks the station table looking for
     * work. From here on every station is reached by index, through a
     * wire; the engine never scans, and this is the single exception.
     *
     * Every station with no buffered input can run now, because
     * nothing has to arrive first. A station with an unfinished port
     * is skipped rather than refused — the check above already
     * refused it, so reaching here means there are none.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call || s->seeded)
            continue;

        int has_ring = 0;
        for (int j = 0; j < s->n_in_ports; j++)
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == IN_PORT_RING)
                has_ring = 1;
        if (has_ring)
            continue;

        /* Through the same door delivery uses — one way a task comes
         * into existence, not two. */
        s->seeded = 1;
        if (map_station_try_start(m, i))
            m->seeded++;
    }
    return NULL;
}
/* }}} */

/* {{{ in_port_kind_name() */
const char *in_port_kind_name(unsigned char kind)
{
    static const char *const names[IN_PORT_KIND_COUNT] = {
        [IN_PORT_RING]   = "a buffer",
        [IN_PORT_STATIC] = "a static value",
        [IN_PORT_NONE]   = "a port with no source yet",
    };
    return kind < IN_PORT_KIND_COUNT ? names[kind]
                                    : "a port of an unknown kind";
}
/* }}} */

/* {{{ station_out_port() */
/*
 * The port at a given index, walking the list. Ports are few — one
 * for plain, three for a comparator — so the walk is cheaper than
 * any cleverness. Returns null when the port was never created,
 * which delivery reads as "discard".
 */
out_port_t *station_out_port(station_t *s, int index)
{
    out_port_t *p = s->out_ports;
    for (int i = 0; p && i < index; i++)
        p = p->next;
    return p;
}
/* }}} */

/* {{{ out_port_dests() */
dest_set_t *out_port_dests(const out_port_t *p)
{
    if (!p)
        return NULL;
    /* Acquire, so everything the writer put in the set before
     * publishing the pointer is visible to whoever follows it. */
    return atomic_load_explicit(&p->dests, memory_order_acquire);
}
/* }}} */

/* {{{ dest_set_build() */
/*
 * A new set from an old one, plus one wire or minus one. Never edits
 * what it was given — that set may have walkers inside it right now,
 * and the whole design rests on nothing it holds ever changing.
 *
 * Passing -1 as a station means "add nothing" or "drop nothing". A
 * drop removes **one** matching pair, not every match, because a wire
 * drawn twice is two wires and removing one should leave the other.
 */
dest_set_t *dest_set_build(const dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port)
{
    int old_n = from ? from->n : 0;
    int n = old_n + (add_station >= 0 ? 1 : 0);
    dest_set_t *set = calloc(1, sizeof *set + (size_t)(n > 0 ? n : 1)
                                              * sizeof(destination_t));
    if (!set)
        fail_resource("out of memory for a destination set");

    int out = 0;
    int dropped = 0;
    for (int i = 0; i < old_n; i++) {
        if (!dropped && drop_station >= 0
            && from->items[i].station == drop_station
            && from->items[i].port == drop_port) {
            dropped = 1;
            continue;
        }
        set->items[out++] = from->items[i];
    }
    if (add_station >= 0) {
        /* Appended, so fan-out visits destinations in the order the
         * wires were drawn. Nothing in the engine depends on that
         * order — a delivery visits all of them and the order values
         * arrive elsewhere was never promised — but the dump writes
         * them in array order, so keeping it means dump, load, dump
         * produces the same text without anybody arranging it. */
        set->items[out].station = add_station;
        set->items[out].port = add_port;
        out++;
    }
    set->n = out;
    return set;
}
/* }}} */

/* {{{ struct scrap_item */
/*
 * One thing waiting to be freed, and the photograph of every worker's
 * epoch taken when it was filed.
 *
 * A worker whose epoch is now **even** is not inside a task, and one
 * whose epoch **differs from the snapshot** has finished the task it
 * was in. Either way it cannot still be using what this holds.
 */
struct scrap_item {
    struct scrap_item *next;
    void              *p;
    void             (*free_fn)(void *);
    uint64_t          *snapshot;
    int                n_snapshot;
};
/* }}} */

/* {{{ static int nobody_can_hold() */
/*
 * True when no worker can still be inside the task it was in when
 * this was filed.
 *
 * Nothing waits and nothing spins. An idle worker is asleep and
 * therefore even, so it passes without ever having to move — which is
 * what would otherwise deadlock a sweep against a quiet pool.
 *
 * Sixty-four bits, so a counter cannot wrap all the way back to its
 * snapshot in any run this engine will ever have and read as
 * unchanged when it is not.
 */
static int nobody_can_hold(map_t *m, const struct scrap_item *it)
{
    /*
     * Filed when there were no workers at all — during construction,
     * before the pool exists. Nobody can be inside something that was
     * already replaced before anyone could reach it, so it is free to
     * go the first time anybody sweeps.
     */
    if (it->n_snapshot == 0)
        return 1;
    for (int i = 0; i < it->n_snapshot; i++) {
        uint64_t now = pool_worker_epoch(m->pool, i);
        if ((now % 2) == 0)
            continue;                       /* not in a task */
        if (now != it->snapshot[i])
            continue;                       /* a different task since */
        return 0;                           /* might be inside this one */
    }
    return 1;
}
/* }}} */

/* {{{ map_scrap_sweep() */
void map_scrap_sweep(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    struct scrap_item **link = &m->scrap_head;
    while (*link) {
        struct scrap_item *it = *link;
        if (nobody_can_hold(m, it)) {
            /* Unfiled first, freed under the same hold: a second
             * toucher arriving afterwards does not find it, so there
             * is nothing for it to free twice. That is the whole of
             * what this lock is for. */
            *link = it->next;
            it->free_fn(it->p);
            free(it->snapshot);
            free(it);
        } else {
            link = &it->next;
        }
    }
    pthread_mutex_unlock(&m->scrap_mutex);
}
/* }}} */

/* {{{ map_retire() */
void map_retire(map_t *m, void *p, void (*free_fn)(void *))
{
    if (!p)
        return;

    /* Sweep before filing, so the work happens exactly where the need
     * is created and a program that changes shape forever reclaims as
     * it goes. */
    map_scrap_sweep(m);

    int workers = m->pool ? pool_worker_count(m->pool) : 0;
    uint64_t *snapshot = NULL;
    if (workers > 0) {
        snapshot = calloc((size_t)workers, sizeof *snapshot);
        if (!snapshot)
            fail_resource("out of memory retiring something");
        for (int i = 0; i < workers; i++)
            snapshot[i] = pool_worker_epoch(m->pool, i);
    }

    struct scrap_item *it = calloc(1, sizeof *it);
    if (!it)
        fail_resource("out of memory retiring something");
    it->p = p;
    it->free_fn = free_fn;
    it->snapshot = snapshot;
    it->n_snapshot = workers;

    pthread_mutex_lock(&m->scrap_mutex);
    it->next = m->scrap_head;
    m->scrap_head = it;
    pthread_mutex_unlock(&m->scrap_mutex);

    /*
     * A map with no pool has no workers, so there are no epochs to
     * snapshot and nothing that could be inside anything. Those items
     * are freeable the first time anybody sweeps — which is the
     * construction case, where every wire drawn replaces the set the
     * one before it made.
     */
}
/* }}} */

/* {{{ map_scrap_count() */
int map_scrap_count(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    int n = 0;
    for (struct scrap_item *it = m->scrap_head; it; it = it->next)
        n++;
    pthread_mutex_unlock(&m->scrap_mutex);
    return n;
}
/* }}} */

/* {{{ map_scrap_free_all() */
/*
 * Empties the scrapyard. Called at teardown, when every worker has
 * been collected and nothing can be using anything.
 */
void map_scrap_free_all(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    struct scrap_item *it = m->scrap_head;
    m->scrap_head = NULL;
    while (it) {
        struct scrap_item *next = it->next;
        it->free_fn(it->p);
        free(it->snapshot);
        free(it);
        it = next;
    }
    pthread_mutex_unlock(&m->scrap_mutex);
}
/* }}} */

/* {{{ map_connect() */
void map_connect(map_t *m, int from_station, int port,
                 int to_station, int to_port)
{
    /*
     * A face on the one wiring operation (issue 212), for a caller
     * that wants a refusal to stop the program.
     *
     * This used to be a second implementation with its own rules, and
     * they were quietly *weaker*: it never asked whether the
     * destination was a buffer, and it never compared the widths. So
     * a program could be built by hand that the same program read
     * from a file would have been refused — two sets of rules meant
     * to agree, with one of them missing two.
     */
    const char *no = map_wire(m, from_station, port, to_station, to_port);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ map_start() */
void map_start(map_t *m, int n_workers)
{
    if (m->pool)
        fail("the map was already started");
    /* Delivery rides the pool's finish hook: after a worker runs a
     * task, the map decides where its output goes. This is the whole
     * of the pool's knowledge of the engine — one function pointer. */
    m->pool = pool_create(n_workers, map_deliver, m);

    /* A process-wide "active map" pointer used to be set here, so that
     * a box — which receives only values and has no handle to anything
     * — could reach the statics table's write call. **That pointer was
     * the singleton**: it is what made a process able to run only one
     * map. Issue 405 removed the reason for it by giving a static's
     * write a real address, a station and a port, reachable from
     * outside the graph where every other configuration change already
     * comes from. Nothing here holds process-wide state now, so two
     * maps can run side by side and not see each other. */
}
/* }}} */

/* {{{ map_in_port_depth() */
int map_in_port_depth(map_t *m, int station, int port)
{
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        fail("asking the depth of a port that does not exist");
    in_port_t *sl = &s->in_ports[port];

    /* A maintained count rather than index arithmetic (issue 210d):
     * with values claimed wherever they sit, the distance between two
     * indices stopped describing how many are waiting.
     *
     * A port that is not a buffer still answers, and answers honestly.
     * A static reports whatever its slots were carrying when it
     * stopped being a buffer, which is the truth — those values are
     * waiting, and will be served if it becomes a buffer again. */
    pthread_mutex_lock(&s->mutex);
    int depth = sl->held;
    pthread_mutex_unlock(&s->mutex);
    return depth;
}
/* }}} */

/* {{{ map_destroy() */
/* Phase 7 joints, implemented in the observe module; declared here
 * narrowly so teardown can call them without the whole header. */
void map_observe_stop(map_t *m);
void map_report_shutdown(map_t *m);

void map_destroy(map_t *m)
{
    map_observe_stop(m);
    /* A borrowed pool belongs to the program that made it, and other
     * programs may still be running on it (issue 212). */
    if (m->pool && !m->pool_is_borrowed)
        pool_destroy(m->pool);
    map_report_shutdown(m);
    if (m->station_names) {
        /* Over what the array actually holds, not over the station
         * count: stations are added one at a time and the names grow
         * behind them, so the two are not always equal (issue 212). */
        for (int i = 0; i < m->n_named; i++)
            free(m->station_names[i]);
        free(m->station_names);
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call) {
            pthread_mutex_destroy(&s->mutex);
            continue;
        }
        /* Results nobody took. Freed rather than reported, because
         * the pile-up was already shouted about from the first
         * doubling — saying it twice at teardown would be the same
         * fault wearing a different hat (issue 209). */
        free(s->held);
        s->held = NULL;

        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_free_pages(&s->in_ports[j]);
            /* Both storages, because a port carries both whatever it
             * was being used for (issue 401). */
            in_port_constant_free(&s->in_ports[j]);
        }
        free(s->in_ports);
        out_port_t *p = s->out_ports;
        while (p) {
            free(out_port_dests(p));
            out_port_t *next = p->next;
            free(p);
            p = next;
        }
        pthread_mutex_destroy(&s->mutex);
    }
    /* Everything a rewire replaced and left filed. By now the pool is
     * gone, so nothing can be walking any of it (issue 214). */
    map_scrap_free_all(m);
    pthread_mutex_destroy(&m->scrap_mutex);
    pthread_mutex_destroy(&m->rewire_mutex);
    for (int i = 0; i < m->n_shelves; i++)
        free(m->shelves[i]);
    free(m->shelves);
    free(m);
}
/* }}} */
