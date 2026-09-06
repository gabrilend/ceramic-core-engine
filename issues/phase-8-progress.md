# Phase 8 progress — the workbench

Phase 8's goal: tools that stand outside the engine and help somebody
write a program for it. Everything in the phases beneath makes maps
run; nothing in them helps anyone compose one except a text editor.

**The phase has started, and it is the drawing half that stands.**
Stations can be placed and wired on a canvas, and several drawings
live side by side in the browser's own storage. What none of it does
yet is produce a file: the canvas is a drawing surface and not a door.

| Issue | State | In one line |
|---|---|---|
| [801 — the workbench in the browser](801-browser-workbench.md) | **in progress** | A canvas where stations are placed, named, given a kind and a box, and wired; statics filled in; every load-time rule applied as a wire is drawn rather than at startup; and a download of the map together with the C source for the functions it used. Nothing on a server. Drawing and keeping several drawings are done; the writer that turns one into a map exists in C and is not in the page yet. |
| [802 — a program you can watch](completed/802-a-program-you-can-watch.md) | **complete** | A build flag makes a program leave a trail as it runs — every station that ran, every value that moved, every buffer that grew — in a ring in shared memory that any number of processes can read. Without the flag the emitting is not compiled at all, which is read from the object file. A slow reader loses events and is told exactly how many; the arithmetic is checked. |
| [803 — the viewer](803-the-viewer.md) | open, **unblocked** | A page that watches a running program and **cannot touch it**: stations lighting as they run, buffers filling, and the page saying so when the trail lost events. Shares nothing with the workbench, which is a door where this is a window. |

## What it waited on, and no longer does

**The map file format, which was about to change twice.** Boxes
addressed by file ([311a](completed/311a-boxes-addressed-by-file.md))
rewrote the station line, and the map becoming code
([311d](completed/311d-the-map-becomes-code.md)) changed what reading a
map even means. The canvas emits that format exactly, so building it
against a format with two pending changes would have meant drawing it
twice.

**Both have landed**, and the format gained two things since which the
canvas has to know about: a station line may carry `@N` saying where an
iterator had got to, and an input line may carry `[a, b]` saying what
is waiting in a buffer
([712](completed/712-capturing-a-running-program.md)). Neither is
something a person composing a new map writes — both describe a program
that has been running — so the canvas reads them and does not offer
them.

**A linker, briefly, and it was one package.** Getting the project's
own parser and writer into the page means compiling them to
WebAssembly, and this was recorded as blocked because the machine had
no linker for that target. The compiler was already there and already
knew the target; only the linker that turns its object files into a
module was missing, and it ships separately in the same LLVM version.
Worth recording as a shape rather than an incident: *blocked on
tooling* deserves one check of what the tooling actually is, because
the difference between a wall and a package is the difference between
redesigning around it and typing one command. The module is a build
artifact committed beside the page, so building the engine still needs
a C compiler and nothing else, and opening the workbench needs no
compiler at all.

## The two halves of this phase

**A door and a window, and they share nothing.** The workbench
composes a map that does not exist yet: everything is alike, nothing is
running, and the page's whole job is to let somebody make changes. The
viewer watches one that is running and cannot reach it at all.

A drawing layer serving both would have to serve a picture where the
interesting thing is *which station is busy right now* and a picture
where the interesting thing is *what somebody is about to connect*, and
one serving both serves neither. What they may share is an
understanding of the map file format, because there is one format and
two readers of it would be two things that must agree.

## What is already decided about this phase

**The canvas is an alternative to writing a map file by hand, never a
prerequisite for it.** If the canvas can ever express something the
format cannot, the format is what needs fixing. This is the constraint
the whole phase is held to.

**It shares nothing with the demo panels**
([713](713-demos-you-can-steer.md)), and that was decided rather than
drifted into. The two draw a similar picture and answer to opposite
requirements: a canvas for composing a map must treat every station
alike, and a page explaining one mechanism must be able to draw the
station it is about larger than the others.

**No box function ever executes in the browser.** What the page
carries is a model of the *scheduling* — a box has a duration you dial
and nothing else, and what travels the wires is a token meaning
something arrived rather than a value. It shows the shape of a running
program and cannot show a result. The slideshow
([709](709-slideshow-and-transcripts.md)) was promised the same
machinery to draw its screens with.

## The phase's demo

Not yet written as an issue. Phase 8 is the one phase whose panel is
not a terminal program with a page in front of it, since the thing
being demonstrated is already a page — which is a difference worth
thinking about before an issue file commits to an answer.
