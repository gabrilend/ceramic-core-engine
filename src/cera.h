/*
 * cera.h — everything a program built with this engine may call.
 *
 * The same components as cera.c, in the same order, and within each one
 * the calls in the order the body defines them. Nothing else is public.
 *
 * What each call does is in cera.info.md.
 */
#ifndef CERA_H
#define CERA_H
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>

/* {{{ 012 — the pool */
typedef struct task cera_task_t;
typedef void (*cera_task_call_t)(cera_task_t *t);

struct task {
    cera_task_call_t call;
    void       *owner;
    int32_t     station;
    int32_t     port;
    int32_t     n_in;
    void      **in;
    void       *out;
    long        box_ns;
};

typedef struct pool cera_pool_t;
typedef void (*cera_pool_finish_t)(void *ctx, cera_task_t *t);
void cera_pool_push(cera_pool_t *p, cera_task_t *t);
cera_task_t *cera_pool_pop(cera_pool_t *p);
int cera_pool_worker_index(void);
uint64_t cera_pool_worker_epoch(cera_pool_t *p, int worker);
void cera_pool_signal_when_finished(cera_pool_t *p, int signo);
void cera_pool_stop(cera_pool_t *p);
int  cera_pool_finished(cera_pool_t *p);
int  cera_pool_queued(cera_pool_t *p);
int  cera_pool_worker_station(cera_pool_t *p, int worker);
cera_pool_t *cera_pool_create(int n_workers, cera_pool_finish_t finish, void *finish_ctx);
void cera_pool_release(cera_pool_t *p);
void cera_pool_join(cera_pool_t *p);
void cera_pool_submitter_register(cera_pool_t *p);
void cera_pool_submitter_unregister(cera_pool_t *p);
void cera_pool_destroy(cera_pool_t *p);
int cera_pool_worker_count(cera_pool_t *p);
void cera_pool_queue_stats(cera_pool_t *p, int *capacity, int *high_water, int *growths);
/* }}} */

/* {{{ 019 — the station table */
enum in_port_kind {
    CERA_IN_PORT_RING    = 0,
    CERA_IN_PORT_STATIC  = 1,
    CERA_IN_PORT_NONE    = 2,
    CERA_IN_PORT_KIND_COUNT
};

#define CERA_IN_PORT_DEFAULT_CAPACITY 10

enum slot_state {
    CERA_SLOT_EMPTY    = 0,
    CERA_SLOT_RESERVED = 1,
    CERA_SLOT_READY    = 2,
    CERA_SLOT_CLAIMED  = 3,
};

/*
 * **A door is a port, not a station** (issues 213a, 209a).
 *
 * A port carries the number of the argument or result it is, or this
 * when it is neither — which is most of them. The number is a name the
 * author chose that happens to sort: reordering every line in a file
 * changes nothing, and a gap or a duplicate is refused at bring-up.
 *
 * It means the same thing whoever supplies the value — a shell, a C
 * caller, or an enclosing map — the way a C function's first parameter
 * does not care who called it. That is what lets a map stand where a
 * box stands.
 */
#define CERA_NOT_A_DOOR (-1)

enum station_kind {
    CERA_STATION_PLAIN      = 0,
    CERA_STATION_COMPARATOR = 1,
    CERA_STATION_ITERATOR   = 2,
    CERA_STATION_KIND_COUNT
};

typedef struct in_port_page {
    _Atomic(struct in_port_page *) next;
    unsigned char                 slots[];
} cera_in_port_page_t;

typedef struct in_port {
    _Atomic unsigned char kind;
    int   elem_size;
    cera_in_port_page_t *pages;
    int   page_slots;
    _Atomic int capacity;
    int   stride;
    int   read_hint;
    int   write_hint;
    _Atomic int held;
    void *constant;
    char *constant_string;
    int   constant_set;
    const char *type_name;
    const struct struct_text *text;
    int growths;
    int high_water;
    /* Which of the map's arguments this port is, or CERA_NOT_A_DOOR.
     * Being an argument and being fed by a wire are independent: a port
     * that is both is fed both ways, and simply is not an argv slot. */
    int argument;
} cera_in_port_t;

typedef struct destination {
    int32_t station;
    int32_t port;
} cera_destination_t;

typedef struct dest_set {
    int           n;
    cera_destination_t items[];
} cera_dest_set_t;

typedef struct out_port {
    _Atomic(cera_dest_set_t *) dests;
    struct out_port      *next;
    /* Which of the map's results this port is, or CERA_NOT_A_DOOR. */
    int    result;
    /*
     * **Where an embedding caller wants these values put** (issue
     * 209a), and nothing until it says. A marked port with no
     * receptacle discards like any other unwired output, so a program
     * nobody is collecting from grows nothing.
     *
     * The memory is the caller's. `taken` is claimed with one atomic
     * add, and a worker handed a slot at or past `room` writes
     * nothing — the bound is the reservation, never the winding down,
     * because workers are still inside boxes when the array fills.
     *
     * No slot state machine: a ring slot needs one because it is
     * reused and a reader must know what it is looking at, and one of
     * these is written once and read by nobody until the caller looks.
     */
    void  *into;
    int    room;
    int    elem_size;
    _Atomic int taken;
} cera_out_port_t;

typedef int (*cera_station_compare_t)(const void *a, const void *b);

typedef struct station {
    pthread_mutex_t mutex;
    cera_task_call_t     call;
    unsigned char   kind;
    cera_in_port_t      *in_ports;
    int             n_in_ports;
    cera_out_port_t     *out_ports;
    int             n_out_ports;
    int             cursor;
    _Atomic unsigned char removed;
    int             out_size;
    const char     *box_name;
    unsigned char   seeded;
    cera_station_compare_t compare;
    _Atomic long runs;
    _Atomic long produced;
    _Atomic long box_ns;
    _Atomic long mutex_wait_ns;
} cera_station_t;

#define CERA_STATIONS_PER_SHELF 64
#define CERA_STATION_SHELF_SHIFT 6
#define CERA_STATION_SHELF_MASK  (CERA_STATIONS_PER_SHELF - 1)

typedef struct map {
    cera_station_t **shelves;
    int         n_shelves;
    _Atomic int n_stations;
    cera_pool_t    *pool;
    int seeded;
    char **station_names;
    int    n_named;
    _Atomic int closing;
    int    pool_is_borrowed;
    pthread_mutex_t rewire_mutex;
    pthread_mutex_t   scrap_mutex;
    struct scrap_item *scrap_head;
    pthread_t observer;
    int       observer_running;
    char     *observer_path;
    int       observer_interval_ms;
    struct cera_watch *watch;
} cera_map_t;

static inline cera_station_t *cera_map_station(cera_map_t *m, int n)
{
    return &m->shelves[n >> CERA_STATION_SHELF_SHIFT][n & CERA_STATION_SHELF_MASK];
}

cera_map_t *cera_map_create_empty(void);
cera_map_t *cera_map_create(int n_stations);
int cera_map_add_station(cera_map_t *m);

void cera_map_place(cera_map_t *m, int station, cera_task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size);

const char *cera_map_in_port_start_depth(cera_map_t *m, int station, int port,
                                    int slots);

void cera_map_in_port_convert(cera_map_t *m, int station, int port, int kind);

const char *cera_map_configure_port(cera_map_t *m, int station, int port,
                               int source, const char *text);

const char *cera_map_check_sources(cera_map_t *m);
const char *cera_map_name_station(cera_map_t *m, int station, const char *name);
const char *cera_map_station_set_cursor(cera_map_t *m, int station, int at);
const char *cera_map_designate_result(cera_map_t *m, int station, int port,
                                      int nth);
cera_map_t *cera_map_start_beside(cera_map_t *parent);
const char *cera_map_designate_argument(cera_map_t *m, int station, int port,
                                        int nth);

int cera_map_argument_at(cera_map_t *m, int nth, int *station, int *port);
int cera_map_result_at(cera_map_t *m, int nth, int *station, int *port);

const char *cera_map_deliver_argument(cera_map_t *m, int station, int port,
                                 const void *value, int size);

const char *cera_map_collect(cera_map_t *m, int station, int port,
                             void *into, int room, int elem_size);

int cera_map_collected(cera_map_t *m, int station, int port);
const char *cera_map_bring_up(cera_map_t *m);

void cera_map_connect(cera_map_t *m, int from_station, int port,
                 int to_station, int to_port);

void cera_map_start(cera_map_t *m, int n_workers);
int cera_map_in_port_depth(cera_map_t *m, int station, int port);
void cera_map_destroy(cera_map_t *m);
/* }}} */

/* {{{ 020 — delivery, readiness, routing */
int cera_map_station_start_after(cera_map_t *m, int station,
                            void (*while_locked)(void *), void *ctx);

int cera_map_station_try_start(cera_map_t *m, int station);
int cera_map_station_keep_starting(cera_map_t *m, int station);
int cera_map_station_start_while_ready(cera_map_t *m, int station);
int cera_map_deliver_value(cera_map_t *m, int station, int port, const void *value);
/* }}} */

/* {{{ 027 — support for generated code */
typedef int (*cera_compare_fn_t)(const void *a, const void *b);

enum field_kind {
    CERA_FIELD_INT = 0,     /* signed integers of any width */
    CERA_FIELD_UINT,        /* unsigned integers of any width */
    CERA_FIELD_FLOAT,       /* float or double */
    CERA_FIELD_STRING,      /* a char array, fixed length, text inside */
    CERA_FIELD_STRUCT,      /* another struct, by its own field table */
};

typedef struct struct_info cera_struct_info_t;

typedef struct field_info {
    const char           *name;
    int                   offset;
    int                   size;
    unsigned char         kind;
    const cera_struct_info_t  *nested;
    int                   array_len;
} cera_field_info_t;

struct struct_info {
    const char          *name;
    int                  size;
    int                  n_fields;
    const cera_field_info_t  *fields;
};

typedef struct box_place {
    const char *name;
    const char *address;
    void      (*place)(cera_map_t *m, int station, int kind);
} cera_box_place_t;

extern const cera_box_place_t    box_places[];
extern const int            n_box_places;
extern const cera_struct_info_t  struct_layouts[];
extern const int            n_struct_layouts;

typedef struct cera_where {
    int station;
    int port;
} cera_where_t;

typedef struct cera_textbuf {
    char *out;
    int   room;
    int   used;
} cera_textbuf_t;

typedef struct struct_text {
    const char *name;
    int         size;
    const char *(*read)(const char *p, void *out, const cera_where_t *w);
    void        (*write)(const void *bytes, cera_textbuf_t *tb);
} cera_struct_text_t;

extern const cera_struct_text_t struct_texts[];
extern const int           n_struct_texts;

typedef struct box_source {
    const char *path;
    const char *text;
} cera_box_source_t;

extern const cera_box_source_t cera_box_sources[];
extern const int          cera_n_box_sources;

typedef struct map_build {
    const char *path;
    int         n_stations;
    int       (*build)(cera_map_t *m, int *landed, int cap);
} cera_map_build_t;

extern const cera_map_build_t cera_map_builds[];
extern const int         cera_n_map_builds;
const cera_box_place_t *cera_box_place_find(const char *name);
const cera_struct_text_t *cera_struct_text_find(const char *type_name);
const cera_struct_info_t *cera_struct_find(const char *type_name);
void cera_emitted_print(FILE *out);
const char *cera_box_source_text(const char *path);
const cera_map_build_t *cera_map_build_find(const char *path);
void cera_map_place_box(cera_map_t *m, int station, const char *box_name, int kind);
/* }}} */

/* {{{ 033 — constants, and values from text */
const char *cera_text_expect(const char *p, char c, const cera_where_t *w,
                             const char *what);

const char *cera_text_signed(const char *p, void *out, int size,
                             const cera_where_t *w, const char *field);

const char *cera_text_unsigned(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);

const char *cera_text_floating(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);

const char *cera_text_chars(const char *p, char *out, int room,
                            const cera_where_t *w, const char *field);

void cera_text_put(cera_textbuf_t *tb, const char *literal);
void cera_text_put_signed(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_unsigned(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_floating(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_chars(cera_textbuf_t *tb, const char *chars, int room);

void cera_map_in_port_static_text(cera_map_t *m, int station, int port,
                             const char *text);

const char *cera_map_deliver_argument_text(cera_map_t *m, int station, int port,
                                      const char *text);

const char *cera_map_deliver_command_line(cera_map_t *m, int argc, char **argv);

const char *cera_map_in_port_queue_text(cera_map_t *m, int station, int port,
                                   const char *text);

void cera_map_in_port_static_write(cera_map_t *m, int station, int port,
                           const void *bytes, int size);
/* }}} */

/* {{{ 042 — reading a description */
typedef struct map_instance {
    int *station;
    int  count;
} cera_map_instance_t;

typedef struct map_part {
    int entrance;
    int result;
} cera_map_part_t;

cera_map_t *cera_map_load_file(const char *path, int n_workers);
cera_map_t *cera_map_load_salvage(const char *path, int n_workers);
cera_map_instance_t cera_map_instantiate_file(cera_map_t *m, const char *path);
int  cera_map_instance_entrance(cera_map_t *m, const cera_map_instance_t *in, int nth);
int  cera_map_instance_result(cera_map_t *m, const cera_map_instance_t *in, int nth);
void cera_map_instance_free(cera_map_instance_t *in);
const char *cera_map_add_part(cera_map_t *m, const char *what, cera_map_part_t *out);

const char *cera_map_connect_parts(cera_map_t *m, cera_map_part_t from, int from_port,
                              cera_map_part_t to, int to_port);

int cera_map_seed_count(cera_map_t *m);
/* }}} */

/* {{{ 050 — reports and the observer */
enum report_order {
    CERA_REPORT_BY_TIME = 0,
    CERA_REPORT_BY_CONTENTION,
    CERA_REPORT_BY_COUNT,
    CERA_REPORT_ORDER_COUNT
};

void cera_map_report_buffers(cera_map_t *m, FILE *out);
void cera_map_report_stations(cera_map_t *m, FILE *out, int order);
void cera_stats_box_time(cera_task_t *t, long ns);
void cera_map_observe_start(cera_map_t *m, const char *path, int interval_ms);
void cera_map_observe_stop(cera_map_t *m);
void cera_map_report_shutdown(cera_map_t *m);
/* }}} */

/* {{{ 051 — a live map written back out */
void cera_map_dump(cera_map_t *m, FILE *out);
/* }}} */

/* {{{ 052 — changing a running program */
const char *cera_map_wire(cera_map_t *m, int from_station, int port,
                     int to_station, int to_port);

const char *cera_map_unwire(cera_map_t *m, int from_station, int port,
                       int to_station, int to_port);

void        cera_map_disconnect(cera_map_t *m, int from_station, int port,
                           int to_station, int to_port);

const char *cera_map_remove_station(cera_map_t *m, int station);

const char *cera_map_remove_stations(cera_map_t *m, const int *stations,
                                     int count);
/* }}} */

/* {{{ 074 — boxes and maps compiled at run time */
const char *cera_late_source_dir(void);
int                cera_late_box_count(void);
const cera_box_place_t *cera_late_box_at(int i);
const char *cera_late_source_text(const char *path);
int cera_late_unload_box(cera_map_t *m, const char *name);
int cera_late_spill_sources(const char *dir);
const cera_map_build_t *cera_late_compile_map(const char *map_text);
int cera_late_compile_source(const char *c_source);
/* }}} */

/* {{{ 092 — signals, capture, and the end */
#define CERA_EXIT_FINISHED     0    /* ran out of work, or was asked to stop */
#define CERA_EXIT_BAD_FILE     65   /* a map file the engine refused        */
#define CERA_EXIT_BAD_CALL     70   /* an invalid construction call         */
#define CERA_EXIT_NO_RESOURCE  71   /* out of memory; no edit fixes it      */
#define CERA_EXIT_INTERRUPTED  130  /* a person interrupted it              */
#define CERA_EXIT_BUG          134  /* an engine fault; aborts, leaving a core */
typedef void (*cera_error_fn)(const char *message, int exit_code);
void cera_on_error(cera_error_fn fn);
void cera_prepare(const char *report_path);
const char *cera_report_path(void);
int cera_wait(cera_map_t *m);
int cera_capture(cera_map_t *m, const char *path);
int cera_capture_whole(cera_map_t *m, const char *dir);
int cera_capture_now(cera_map_t *m, const char *path);
void cera_stop_now(cera_map_t *m, int exit_code, const char *why);
/* }}} */

/* {{{ 118 — watching a running program */
typedef struct cera_watch cera_watch_t;
typedef struct cera_watch_reader cera_watch_reader_t;

enum cera_watch_kind {
    CERA_WATCH_UP,        /* the program came up: a=stations, b=workers   */
    CERA_WATCH_DUE,       /* a task became due: a=station                 */
    CERA_WATCH_RAN,       /* a station ran: a=station, ns=how long        */
    CERA_WATCH_MOVED,     /* a value was delivered: a.b -> c.d            */
    CERA_WATCH_GREW,      /* a buffer grew: a=station, b=port, c=slots    */
    CERA_WATCH_ADDED,     /* a station was placed: a=station              */
    CERA_WATCH_REMOVED,   /* a station was removed: a=station             */
    CERA_WATCH_WIRED,     /* a wire was drawn: a.b -> c.d                 */
    CERA_WATCH_UNWIRED,   /* a wire was cut: a.b -> c.d                   */
    CERA_WATCH_DONE,      /* the program finished: a=tasks in total       */
    CERA_WATCH_KIND_COUNT
};

typedef struct cera_watch_event {
    uint64_t seq;
    uint64_t ns;
    uint32_t kind;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t pad;
} cera_watch_event_t;

int cera_watch_compiled_in(void);
const char *cera_watch_kind_name(int kind);
const char *cera_watch_open(cera_map_t *m, const char *path);
void cera_watch_close(cera_map_t *m);
cera_watch_reader_t *cera_watch_attach(const char *path);
int cera_watch_next(cera_watch_reader_t *r, cera_watch_event_t *into, uint64_t *lost);
void cera_watch_joined_at(cera_watch_reader_t *r, uint64_t *first, uint64_t *before);
int cera_watch_writer_alive(cera_watch_reader_t *r);
void cera_watch_detach(cera_watch_reader_t *r);
/* }}} */

#endif /* CERA_H */
