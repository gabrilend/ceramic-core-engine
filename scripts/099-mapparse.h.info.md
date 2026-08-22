# 099-mapparse.h — reading a description, from outside

A description is a text file naming stations, the boxes they run, the
constants on their ports, and the arrows between them. This turns one
into a structure saying exactly that and nothing more.

**It belongs to the compiler.** A running program does not read
descriptions — it hands them to the generator, which turns them into
the construction calls they describe, and those are compiled and
loaded like any other code (issue 311d). So the only thing that reads
text is the thing whose job is reading text, and no program built with
this engine carries a parser. Confirmed by looking rather than
claimed: the parser's symbol is absent from every test binary.

## Data structures

**map_description** — what the file said, nothing constructed. A
linked list of stations, each with its name, the box it names, its
kind, its door mark, its input overrides and its output arrows, all
carrying line numbers; plus the statics entries as raw text, and the
file's own path.

The text of a constant stays text here. What shape those bytes take is
decided by the port it lands on, which is not something a description
knows or should.

## Functions

**mapfile_parse(path) → description** — line oriented; the first word
dispatches; `#` starts a comment; indentation means nothing. Any
malformed line is fatal, naming file, line, and what was expected.

**mapfile_free(description)** — everything is allocated per
description and freed together.

## What it touches of the engine

Three enumerations — the station kinds and the door marks. That is the
whole of it, and it is why a build tool can hold this.

## Related

- [100-mapparse.c](100-mapparse.c.info.md), the parser from inside
- [008 — Map file format](../docs/008-map-file-format.md)
- [311d — The map becomes code](../issues/completed/311d-the-map-becomes-code.md)
