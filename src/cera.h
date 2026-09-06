/*
 * cera.h — everything a program built with this engine may call.
 *
 * The same components as cera.c, in the same order, and within each one
 * the calls in the order the body defines them. Nothing else is public.
 *
 * What each call does is in cera.h.info.md.
 */
#ifndef CERA_H
#define CERA_H

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>

/* {{{ 012 — the pool */
/* ==================================================================
 *
 * 012 — the pool
 * ================================================================== */

/* {{{ cera_task_t */
typedef struct task cera_task_t;
/* }}} */

/* {{{ cera_task_call_t */
typedef void (*cera_task_call_t)(cera_task_t *t);
/* }}} */

/* {{{ struct task */
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
/* }}} */

/* {{{ cera_pool_t */
typedef struct pool cera_pool_t;
/* }}} */

/* {{{ cera_pool_finish_t */
typedef void (*cera_pool_finish_t)(void *ctx, cera_task_t *t);
/* }}} */

/* {{{ cera_pool_push() */
void cera_pool_push(cera_pool_t *p, cera_task_t *t);
/* }}} */

/* {{{ cera_pool_pop() */
cera_task_t *cera_pool_pop(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_worker_index() */
int cera_pool_worker_index(void);
/* }}} */

/* {{{ cera_pool_worker_epoch() */
uint64_t cera_pool_worker_epoch(cera_pool_t *p, int worker);
/* }}} */

/* {{{ cera_pool_signal_when_finished() */
void cera_pool_signal_when_finished(cera_pool_t *p, int signo);
/* }}} */

/* {{{ cera_pool_stop() */
void cera_pool_stop(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_finished() */
int  cera_pool_finished(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_queued() */
int  cera_pool_queued(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_worker_station() */
int  cera_pool_worker_station(cera_pool_t *p, int worker);
/* }}} */

/* {{{ cera_pool_create() */
cera_pool_t *cera_pool_create(int n_workers, cera_pool_finish_t finish, void *finish_ctx);
/* }}} */

/* {{{ cera_pool_release() */
void cera_pool_release(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_join() */
void cera_pool_join(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_submitter_register() */
void cera_pool_submitter_register(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_submitter_unregister() */
void cera_pool_submitter_unregister(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_destroy() */
void cera_pool_destroy(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_worker_count() */
int cera_pool_worker_count(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_queue_stats() */
void cera_pool_queue_stats(cera_pool_t *p, int *capacity, int *high_water, int *growths);
/* }}} */
/* }}} */

/* {{{ 019 — the station table */
/* ==================================================================
 *
 * 019 — the station table
 * ================================================================== */

/* {{{ enum in_port_kind */
enum in_port_kind {
    CERA_IN_PORT_RING    = 0,
    CERA_IN_PORT_STATIC  = 1,
    CERA_IN_PORT_NONE    = 2,
    CERA_IN_PORT_KIND_COUNT
};
/* }}} */

/* {{{ CERA_IN_PORT_DEFAULT_CAPACITY */
#define CERA_IN_PORT_DEFAULT_CAPACITY 10
/* }}} */

/* {{{ enum slot_state */
enum slot_state {
    CERA_SLOT_EMPTY    = 0,
    CERA_SLOT_RESERVED = 1,
    CERA_SLOT_READY    = 2,
    CERA_SLOT_CLAIMED  = 3,
};
/* }}} */

/* {{{ enum station_door */
enum station_door {
    CERA_DOOR_NONE = 0,   /* an interior station, which is most of them */
    CERA_DOOR_IN   = 1,   /* the outside may deliver here */
    CERA_DOOR_OUT  = 2,   /* results wait here to be taken */
};
/* }}} */

/* {{{ enum station_kind */
enum station_kind {
    CERA_STATION_PLAIN      = 0,
    CERA_STATION_COMPARATOR = 1,
    CERA_STATION_ITERATOR   = 2,
    CERA_STATION_KIND_COUNT
};
/* }}} */

/* {{{ struct in_port_page / struct in_port */
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
} cera_in_port_t;
/* }}} */

/* {{{ struct destination / struct out_port */
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
} cera_out_port_t;
/* }}} */

/* {{{ cera_station_compare_t */
typedef int (*cera_station_compare_t)(const void *a, const void *b);
/* }}} */

/* {{{ struct station */
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
    unsigned char   door;
    void           *held;
    int             n_held;
    int             held_room;
    int             held_growths;
    cera_station_compare_t compare;
    _Atomic long runs;
    _Atomic long produced;
    _Atomic long box_ns;
    _Atomic long mutex_wait_ns;
} cera_station_t;
/* }}} */

/* {{{ struct map */
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
} cera_map_t;
/* }}} */

/* {{{ static inline cera_station_t *cera_map_s */
static inline cera_station_t *cera_map_station(cera_map_t *m, int n)
{
    return &m->shelves[n >> CERA_STATION_SHELF_SHIFT][n & CERA_STATION_SHELF_MASK];
}
/* }}} */

/* {{{ cera_map_create_empty() */
cera_map_t *cera_map_create_empty(void);
/* }}} */

/* {{{ cera_map_create() */
cera_map_t *cera_map_create(int n_stations);
/* }}} */

/* {{{ cera_map_add_station() */
int cera_map_add_station(cera_map_t *m);
/* }}} */

/* {{{ cera_map_place() */
void cera_map_place(cera_map_t *m, int station, cera_task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size);
/* }}} */

/* {{{ cera_map_in_port_start_depth() */
const char *cera_map_in_port_start_depth(cera_map_t *m, int station, int port,
                                    int slots);
/* }}} */

/* {{{ cera_map_in_port_convert() */
void cera_map_in_port_convert(cera_map_t *m, int station, int port, int kind);
/* }}} */

/* {{{ cera_map_configure_port() */
const char *cera_map_configure_port(cera_map_t *m, int station, int port,
                               int source, const char *text);
/* }}} */

/* {{{ cera_map_check_sources() */
const char *cera_map_check_sources(cera_map_t *m);
/* }}} */

/* {{{ cera_map_name_station() */
const char *cera_map_name_station(cera_map_t *m, int station, const char *name);
/* }}} */

/* {{{ cera_map_station_set_cursor() */
const char *cera_map_station_set_cursor(cera_map_t *m, int station, int at);
/* }}} */

/* {{{ cera_map_designate_output() */
const char *cera_map_designate_output(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_start_beside() */
cera_map_t *cera_map_start_beside(cera_map_t *parent);
/* }}} */

/* {{{ cera_map_designate_input() */
const char *cera_map_designate_input(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_deliver_argument() */
const char *cera_map_deliver_argument(cera_map_t *m, int station, int port,
                                 const void *value, int size);
/* }}} */

/* {{{ cera_map_output_waiting() */
int cera_map_output_waiting(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_output_take() */
int cera_map_output_take(cera_map_t *m, int station, void *into, int size);
/* }}} */

/* {{{ cera_map_bring_up() */
const char *cera_map_bring_up(cera_map_t *m);
/* }}} */

/* {{{ cera_map_connect() */
void cera_map_connect(cera_map_t *m, int from_station, int port,
                 int to_station, int to_port);
/* }}} */

/* {{{ cera_map_start() */
void cera_map_start(cera_map_t *m, int n_workers);
/* }}} */

/* {{{ cera_map_in_port_depth() */
int cera_map_in_port_depth(cera_map_t *m, int station, int port);
/* }}} */

/* {{{ cera_map_destroy() */
void cera_map_destroy(cera_map_t *m);
/* }}} */
/* }}} */

/* {{{ 020 — delivery, readiness, routing */
/* ==================================================================
 *
 * 020 — delivery, readiness, routing
 * ================================================================== */

/* {{{ cera_map_station_start_after() */
int cera_map_station_start_after(cera_map_t *m, int station,
                            void (*while_locked)(void *), void *ctx);
/* }}} */

/* {{{ cera_map_station_try_start() */
int cera_map_station_try_start(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_station_keep_starting() */
int cera_map_station_keep_starting(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_station_start_while_ready() */
int cera_map_station_start_while_ready(cera_map_t *m, int station);
/* }}} */

/* {{{ cera_map_deliver_value() */
int cera_map_deliver_value(cera_map_t *m, int station, int port, const void *value);
/* }}} */
/* }}} */

/* {{{ 027 — support for generated code */
/* ==================================================================
 *
 * 027 — support for generated code
 * ================================================================== */

/* {{{ cera_compare_fn_t */
typedef int (*cera_compare_fn_t)(const void *a, const void *b);
/* }}} */

/* {{{ enum field_kind */
enum field_kind {
    CERA_FIELD_INT = 0,     /* signed integers of any width */
    CERA_FIELD_UINT,        /* unsigned integers of any width */
    CERA_FIELD_FLOAT,       /* float or double */
    CERA_FIELD_STRING,      /* a char array, fixed length, text inside */
    CERA_FIELD_STRUCT,      /* another struct, by its own field table */
};
/* }}} */

/* {{{ struct field_info / struct_info */
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
/* }}} */

/* {{{ struct box_place */
typedef struct box_place {
    const char *name;
    const char *address;
    void      (*place)(cera_map_t *m, int station, int kind);
} cera_box_place_t;
extern const cera_box_place_t    box_places[];
extern const int            n_box_places;
extern const cera_struct_info_t  struct_layouts[];
extern const int            n_struct_layouts;
/* }}} */

/* {{{ writing a value down and reading it back */
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
/* }}} */

/* {{{ box sources, as text */
typedef struct box_source {
    const char *path;
    const char *text;
} cera_box_source_t;
extern const cera_box_source_t cera_box_sources[];
extern const int          cera_n_box_sources;
/* }}} */

/* {{{ maps compiled into code */
typedef struct map_build {
    const char *path;
    int         n_stations;
    int       (*build)(cera_map_t *m, int *landed, int cap);
} cera_map_build_t;
extern const cera_map_build_t cera_map_builds[];
extern const int         cera_n_map_builds;
/* }}} */

/* {{{ cera_box_place_find() */
const cera_box_place_t *cera_box_place_find(const char *name);
/* }}} */

/* {{{ cera_struct_text_find() */
const cera_struct_text_t *cera_struct_text_find(const char *type_name);
/* }}} */

/* {{{ cera_struct_find() */
const cera_struct_info_t *cera_struct_find(const char *type_name);
/* }}} */

/* {{{ cera_emitted_print() */
void cera_emitted_print(FILE *out);
/* }}} */

/* {{{ cera_box_source_text() */
const char *cera_box_source_text(const char *path);
/* }}} */

/* {{{ cera_map_build_find() */
const cera_map_build_t *cera_map_build_find(const char *path);
/* }}} */

/* {{{ cera_map_place_box() */
void cera_map_place_box(cera_map_t *m, int station, const char *box_name, int kind);
/* }}} */
/* }}} */

/* {{{ 033 — constants, and values from text */
/* ==================================================================
 *
 * 033 — constants, and values from text
 * ================================================================== */

/* {{{ cera_text_expect() */
const char *cera_text_expect(const char *p, char c, const cera_where_t *w,
                             const char *what);
/* }}} */

/* {{{ cera_text_signed() */
const char *cera_text_signed(const char *p, void *out, int size,
                             const cera_where_t *w, const char *field);
/* }}} */

/* {{{ cera_text_unsigned() */
const char *cera_text_unsigned(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);
/* }}} */

/* {{{ cera_text_floating() */
const char *cera_text_floating(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);
/* }}} */

/* {{{ cera_text_chars() */
const char *cera_text_chars(const char *p, char *out, int room,
                            const cera_where_t *w, const char *field);
/* }}} */

/* {{{ cera_text_put() */
void cera_text_put(cera_textbuf_t *tb, const char *literal);
/* }}} */

/* {{{ cera_text_put_signed() */
void cera_text_put_signed(cera_textbuf_t *tb, const void *bytes, int size);
/* }}} */

/* {{{ cera_text_put_unsigned() */
void cera_text_put_unsigned(cera_textbuf_t *tb, const void *bytes, int size);
/* }}} */

/* {{{ cera_text_put_floating() */
void cera_text_put_floating(cera_textbuf_t *tb, const void *bytes, int size);
/* }}} */

/* {{{ cera_text_put_chars() */
void cera_text_put_chars(cera_textbuf_t *tb, const char *chars, int room);
/* }}} */

/* {{{ cera_map_in_port_static_text() */
void cera_map_in_port_static_text(cera_map_t *m, int station, int port,
                             const char *text);
/* }}} */

/* {{{ cera_map_deliver_argument_text() */
const char *cera_map_deliver_argument_text(cera_map_t *m, int station, int port,
                                      const char *text);
/* }}} */

/* {{{ cera_map_deliver_command_line() */
const char *cera_map_deliver_command_line(cera_map_t *m, int argc, char **argv);
/* }}} */

/* {{{ cera_map_in_port_queue_text() */
const char *cera_map_in_port_queue_text(cera_map_t *m, int station, int port,
                                   const char *text);
/* }}} */

/* {{{ cera_map_in_port_static_write() */
void cera_map_in_port_static_write(cera_map_t *m, int station, int port,
                           const void *bytes, int size);
/* }}} */
/* }}} */

/* {{{ 042 — reading a description */
/* ==================================================================
 *
 * 042 — reading a description
 * ================================================================== */

/* {{{ cera_map_instance_t */
typedef struct map_instance {
    int *station;
    int  count;
} cera_map_instance_t;
/* }}} */

/* {{{ cera_map_part_t */
typedef struct map_part {
    int entrance;
    int result;
} cera_map_part_t;
/* }}} */

/* {{{ cera_map_load_file() */
cera_map_t *cera_map_load_file(const char *path, int n_workers);
/* }}} */

/* {{{ cera_map_load_salvage() */
cera_map_t *cera_map_load_salvage(const char *path, int n_workers);
/* }}} */

/* {{{ cera_map_instantiate_file() */
cera_map_instance_t cera_map_instantiate_file(cera_map_t *m, const char *path);
/* }}} */

/* {{{ cera_map_instance_entrance() */
int  cera_map_instance_entrance(cera_map_t *m, const cera_map_instance_t *in, int nth);
/* }}} */

/* {{{ cera_map_instance_result() */
int  cera_map_instance_result(cera_map_t *m, const cera_map_instance_t *in, int nth);
/* }}} */

/* {{{ cera_map_instance_free() */
void cera_map_instance_free(cera_map_instance_t *in);
/* }}} */

/* {{{ cera_map_add_part() */
const char *cera_map_add_part(cera_map_t *m, const char *what, cera_map_part_t *out);
/* }}} */

/* {{{ cera_map_connect_parts() */
const char *cera_map_connect_parts(cera_map_t *m, cera_map_part_t from, int from_port,
                              cera_map_part_t to, int to_port);
/* }}} */

/* {{{ cera_map_seed_count() */
int cera_map_seed_count(cera_map_t *m);
/* }}} */
/* }}} */

/* {{{ 050 — reports and the observer */
/* ==================================================================
 *
 * 050 — reports and the observer
 * ================================================================== */

/* {{{ enum report_order */
enum report_order {
    CERA_REPORT_BY_TIME = 0,
    CERA_REPORT_BY_CONTENTION,
    CERA_REPORT_BY_COUNT,
    CERA_REPORT_ORDER_COUNT
};
/* }}} */

/* {{{ cera_map_report_buffers() */
void cera_map_report_buffers(cera_map_t *m, FILE *out);
/* }}} */

/* {{{ cera_map_report_stations() */
void cera_map_report_stations(cera_map_t *m, FILE *out, int order);
/* }}} */

/* {{{ cera_stats_box_time() */
void cera_stats_box_time(cera_task_t *t, long ns);
/* }}} */

/* {{{ cera_map_observe_start() */
void cera_map_observe_start(cera_map_t *m, const char *path, int interval_ms);
/* }}} */

/* {{{ cera_map_observe_stop() */
void cera_map_observe_stop(cera_map_t *m);
/* }}} */

/* {{{ cera_map_report_shutdown() */
void cera_map_report_shutdown(cera_map_t *m);
/* }}} */
/* }}} */

/* {{{ 051 — a live map written back out */
/* ==================================================================
 *
 * 051 — a live map written back out
 * ================================================================== */

/* {{{ cera_map_dump() */
void cera_map_dump(cera_map_t *m, FILE *out);
/* }}} */
/* }}} */

/* {{{ 052 — changing a running program */
/* ==================================================================
 *
 * 052 — changing a running program
 * ================================================================== */

/* {{{ cera_map_wire() */
const char *cera_map_wire(cera_map_t *m, int from_station, int port,
                     int to_station, int to_port);
/* }}} */

/* {{{ cera_map_unwire() */
const char *cera_map_unwire(cera_map_t *m, int from_station, int port,
                       int to_station, int to_port);
/* }}} */

/* {{{ cera_map_disconnect() */
void        cera_map_disconnect(cera_map_t *m, int from_station, int port,
                           int to_station, int to_port);
/* }}} */

/* {{{ cera_map_remove_station() */
const char *cera_map_remove_station(cera_map_t *m, int station);
/* }}} */
/* }}} */

/* {{{ 074 — boxes and maps compiled at run time */
/* ==================================================================
 *
 * 074 — boxes and maps compiled at run time
 * ================================================================== */

/* {{{ cera_late_source_dir() */
const char *cera_late_source_dir(void);
/* }}} */

/* {{{ cera_late_box_count() */
int                cera_late_box_count(void);
/* }}} */

/* {{{ cera_late_box_at() */
const cera_box_place_t *cera_late_box_at(int i);
/* }}} */

/* {{{ cera_late_source_text() */
const char *cera_late_source_text(const char *path);
/* }}} */

/* {{{ cera_late_unload_box() */
int cera_late_unload_box(cera_map_t *m, const char *name);
/* }}} */

/* {{{ cera_late_spill_sources() */
int cera_late_spill_sources(const char *dir);
/* }}} */

/* {{{ cera_late_compile_map() */
const cera_map_build_t *cera_late_compile_map(const char *map_text);
/* }}} */

/* {{{ cera_late_compile_source() */
int cera_late_compile_source(const char *c_source);
/* }}} */
/* }}} */

/* {{{ 092 — signals, capture, and the end */
/* ==================================================================
 *
 * 092 — signals, capture, and the end
 * ================================================================== */

/* {{{ exit codes */
#define CERA_EXIT_FINISHED     0    /* ran out of work, or was asked to stop */
#define CERA_EXIT_BAD_FILE     65   /* a map file the engine refused        */
#define CERA_EXIT_BAD_CALL     70   /* an invalid construction call         */
#define CERA_EXIT_NO_RESOURCE  71   /* out of memory; no edit fixes it      */
#define CERA_EXIT_INTERRUPTED  130  /* a person interrupted it              */
#define CERA_EXIT_BUG          134  /* an engine fault; aborts, leaving a core */
/* }}} */

/* {{{ cera_error_fn */
typedef void (*cera_error_fn)(const char *message, int exit_code);
/* }}} */

/* {{{ cera_on_error() */
void cera_on_error(cera_error_fn fn);
/* }}} */

/* {{{ cera_prepare() */
void cera_prepare(const char *report_path);
/* }}} */

/* {{{ cera_report_path() */
const char *cera_report_path(void);
/* }}} */

/* {{{ cera_wait() */
int cera_wait(cera_map_t *m);
/* }}} */

/* {{{ cera_capture() */
int cera_capture(cera_map_t *m, const char *path);
/* }}} */

/* {{{ cera_capture_whole() */
int cera_capture_whole(cera_map_t *m, const char *dir);
/* }}} */

/* {{{ cera_capture_now() */
int cera_capture_now(cera_map_t *m, const char *path);
/* }}} */

/* {{{ cera_stop_now() */
void cera_stop_now(cera_map_t *m, int exit_code, const char *why);
/* }}} */
/* }}} */

#endif /* CERA_H */
