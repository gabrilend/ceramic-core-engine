/* src/013-jsonl-events.h — phase 3 run-log event writer.
 *
 * What it is, in a sentence: emits one JSON object per event,
 * one event per line, to `tmp/last-run.jsonl` for downstream
 * tooling (LLMs, CLI viewers, the integration test harness).
 *
 * Designed in issue 311. This iteration implements the per-event
 * emit functions on top of issue 314's bounded JSON writer.
 * Concurrency is a single per-writer mutex around the file write;
 * the MPSC ring buffer + dedicated writer thread from the
 * architecture doc lands as a follow-on once dispatch is producing
 * events at rate.
 *
 * Each emit returns 0 on success or -1 on a write failure / JSON
 * encode failure. Failures don't cascade: the writer keeps going,
 * and the caller can check by re-emitting or by closing.
 */

#ifndef SORAMECH_JSONL_EVENTS_H
#define SORAMECH_JSONL_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jsonl_writer jsonl_writer_t;

/* {{{ Lifecycle */
/* Open `path` for writing (truncates). Returns NULL on failure. */
jsonl_writer_t *jsonl_writer_open(const char *path);

/* Flush and close the underlying file, free the writer. Safe on NULL. */
void            jsonl_writer_close(jsonl_writer_t *w);
/* }}} */

/* {{{ Event emitters */
/* All timestamps are Unix epoch seconds as doubles (matches the
 * architecture doc's choice). All emits are thread-safe. */

int jsonl_emit_run_start  (jsonl_writer_t *w, double ts,
                           const char *map, int n_workers);

int jsonl_emit_task_submit(jsonl_writer_t *w, double ts,
                           int task_id, const char *box_id, int worker_idx);

int jsonl_emit_task_start (jsonl_writer_t *w, double ts,
                           int task_id, int worker_idx);

int jsonl_emit_task_end   (jsonl_writer_t *w, double ts,
                           int task_id, int worker_idx,
                           long duration_us, int output_size);

int jsonl_emit_run_end    (jsonl_writer_t *w, double ts,
                           long duration_us, int n_tasks);

/* Verbose events (opt-in via SORAMECH_LOG_VALUES=1).
 * `data` is logged as a string with the JSON writer's standard
 * escaping. Bytes containing NUL aren't supported; truncate to
 * the first NUL or the architecture's 4 KB cap. */
int jsonl_emit_task_input (jsonl_writer_t *w, double ts,
                           int task_id, int port_index,
                           const char *data, int size);

int jsonl_emit_task_output(jsonl_writer_t *w, double ts,
                           int task_id, const char *data, int size);

/* Slot allocator events (opt-in via SORAMECH_LOG_SLOTS=1). Emitted
 * once per slot at graph_attach_runtime time so the run log
 * records the slot layout the dispatch will use. Issue 319's
 * runtime self-construction also emits one per slot when a new
 * box is allocated mid-run via create_box. */
int jsonl_emit_slot_alloc (jsonl_writer_t *w, double ts,
                           int slot_id, int cell_capacity, int n_cells,
                           const char *owner_box, const char *owner_port);

/* Runtime graph mutation events (issue 319 / 248 self-construction
 * paths). Always emitted when an active event-queue is present —
 * graph mutations are rare enough that they don't need a verbosity
 * gate. `box_create` fires after the new box is published into
 * the graph; `wire_add` fires after the connection has been
 * appended to the producer's array. Tracing these reveals the
 * order of mutation operations relative to the dispatch's
 * fan-out, which is the load-bearing question for diagnosing
 * runtime-create-then-push races. */
int jsonl_emit_box_create (jsonl_writer_t *w, double ts,
                           const char *box_id, const char *kind,
                           const char *lang, const char *ref,
                           const char *fn);

int jsonl_emit_wire_add   (jsonl_writer_t *w, double ts,
                           const char *from_box, const char *from_branch,
                           const char *to_box, const char *to_input);

/* Per-push events (opt-in via SORAMECH_LOG_SLOTS=1, same gate as
 * slot_alloc since they're the slot-level activity counterpart).
 * Emitted from push_one_connection on every attempt, including
 * the skipped paths — the `result` field carries either "ok" or
 * a short skip reason ("no-to-box-idx", "dst-null",
 * "input-slots-null", "to-input-out-of-range", "push-failed").
 * Crucial for diagnosing the 319 visibility race where the new
 * box's input_slot_ids field reads as NULL at push time even
 * though runtime_create_box populated it. */
int jsonl_emit_push       (jsonl_writer_t *w, double ts,
                           const char *from_box, const char *to_box,
                           const char *to_input, int slot_id,
                           int n_bytes, const char *result);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_JSONL_EVENTS_H */
