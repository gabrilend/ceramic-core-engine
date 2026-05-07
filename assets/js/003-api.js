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

  return { init, list_maps, list_boxes, get_box, put_box, delete_box,
           get_meta, put_meta, get_data, put_data };
})();
