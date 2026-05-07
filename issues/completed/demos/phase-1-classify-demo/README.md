# Phase 1 Demo — classify-demo

Demonstrates every phase 1 capability in one coherent run:
lua driver, bash driver, branch routing, data file access.

## Map: maps/classify-demo/

```
read-input (lua)
    reads name from data/input.json
    returns "hello, <name>"
        ↓ greeting
stamp (bash)
    appends ISO timestamp
        ↓ stamped
router (branch)
    contains "SoraMech" → greeting port → write-result
    else              → else port    → write-error
        ↓ (greeting)            ↓ (else)
write-result (lua)      write-error (lua)
data/output.json        data/error.json
```

## Running

```bash
luajit soramech-runner.lua maps/classify-demo/
```

Input name is set in `maps/classify-demo/data/input.json`.

## Expected outputs (name = "SoraMech")

`data/output.json`:
```json
{"fields":{"result":{"value":"hello, SoraMech [<timestamp>]","constant":false}},"constant":false}
```

## Testing the else path (name = "stranger")

Set `data/input.json` field `name` to `"stranger"`. Re-run. The greeting
`"hello, stranger [ts]"` does not contain "SoraMech", so the else port fires.
`data/error.json` receives the message instead of output.json.

## Phase 1 checklist

- [x] Create a box in the browser and save it to disk (editor, issue 106/107)
- [x] Wire two boxes together in the browser (issue 107)
- [x] Run a 3-box+ map from the CLI and see outputs written to disk (this map)
- [x] Write a custom driver and have the runner use it (bash stamp.sh)
- [x] Branch box routes correctly based on a predicate (router box)
- [x] Data file read and write from within a box function (input.json / output.json)
