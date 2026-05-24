// Thin fetch wrapper for the soramech-server REST API.
// All functions are async and throw on non-2xx responses.

const API = (() => {
  let base_url = '';
  let map_name = '';

  // {{{ init
  function init(url, name) { base_url = url.replace(/\/$/, ''); map_name = name; }
  // }}}

  // {{{ request
  async function request(method, path, body) {
    const opts = { method, headers: {} };
    if (body !== undefined) {
      opts.headers['Content-Type'] = 'application/json';
      opts.body = JSON.stringify(body);
    }
    const res = await fetch(base_url + path, opts);
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { data = { error: text }; }
    if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
    return data;
  }
  // }}}

  // {{{ list_maps
  async function list_maps() { return request('GET', '/maps'); }
  // }}}

  // {{{ list_boxes
  async function list_boxes() {
    return request('GET', `/maps/${map_name}/boxes`);
  }
  // }}}

  // {{{ get_box
  async function get_box(id) {
    return request('GET', `/maps/${map_name}/boxes/${id}`);
  }
  // }}}

  // {{{ put_box
  async function put_box(id, data) {
    return request('PUT', `/maps/${map_name}/boxes/${id}`, data);
  }
  // }}}

  // {{{ delete_box
  async function delete_box(id) {
    return request('DELETE', `/maps/${map_name}/boxes/${id}`);
  }
  // }}}

  // {{{ get_meta
  async function get_meta() {
    return request('GET', `/maps/${map_name}/meta`);
  }
  // }}}

  // {{{ put_meta
  async function put_meta(data) {
    return request('PUT', `/maps/${map_name}/meta`, data);
  }
  // }}}

  // {{{ list_src_files
  async function list_src_files() {
    return request('GET', `/maps/${map_name}/src`);
  }
  // }}}

  // {{{ get_src_file
  // Returns raw text, not JSON — the request() helper would mis-parse source code.
  async function get_src_file(filename) {
    const res = await fetch(base_url + `/maps/${map_name}/src/${encodeURIComponent(filename)}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.text();
  }
  // }}}

  // {{{ list_extra_src
  async function list_extra_src() {
    return request('GET', `/maps/${map_name}/extrasrc`);
  }
  // }}}

  // {{{ get_extra_src_file
  async function get_extra_src_file(dir_index, filename) {
    const res = await fetch(base_url + `/maps/${map_name}/extrasrc/${dir_index}/${encodeURIComponent(filename)}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res.text();
  }
  // }}}

  // {{{ list_dirs
  // Lists immediate subdirectories of an absolute path on the server's filesystem.
  // Returns { path: string, dirs: string[] }.
  async function list_dirs(path) {
    return request('GET', '/fs/dirs?path=' + encodeURIComponent(path));
  }
  // }}}

  // {{{ get_data
  async function get_data(filename) {
    return request('GET', `/maps/${map_name}/data/${filename}`);
  }
  // }}}

  // {{{ put_data
  async function put_data(filename, data) {
    return request('PUT', `/maps/${map_name}/data/${filename}`, data);
  }
  // }}}

  // {{{ Translation file CRUD — issue 246
  // Translation files (per-port custom translation shims) live under
  // maps/<name>/translations/. The inspector creates them via "New"
  // (PUT), views them via "View" (GET), and the file browser can
  // delete them outright (DELETE). The inspector's own delete
  // affordance only clears the box's `custom_translation` field — it
  // does not call delete_translation.
  async function list_translations() {
    return request('GET', `/maps/${map_name}/translations`);
  }
  async function get_translation(filename) {
    return request('GET', `/maps/${map_name}/translations/${filename}`);
  }
  async function put_translation(filename, content) {
    return request('PUT', `/maps/${map_name}/translations/${filename}`, content);
  }
  async function delete_translation(filename) {
    return request('DELETE', `/maps/${map_name}/translations/${filename}`);
  }
  // }}}

  return { init, list_maps, list_boxes, get_box, put_box, delete_box,
           get_meta, put_meta, get_data, put_data,
           list_src_files, get_src_file,
           list_extra_src, get_extra_src_file,
           list_translations, get_translation, put_translation, delete_translation,
           list_dirs };
})();
