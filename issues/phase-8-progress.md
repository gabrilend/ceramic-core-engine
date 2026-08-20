# Phase 8 progress — the workbench

Phase 8's goal: tools that stand outside the engine and help somebody
write a program for it. Everything in the phases beneath makes maps
run; nothing in them helps anyone compose one except a text editor.

**The phase has not started.** This page exists because every other
phase has one and the gap kept being rediscovered, not because there
is progress to report.

| Issue | State | In one line |
|---|---|---|
| [801 — the workbench in the browser](801-browser-workbench.md) | open, not started | A canvas where stations are placed, named, given a kind and a box, and wired; statics filled in; every load-time rule applied as a wire is drawn rather than at startup; and a download of the map together with the C source for the functions it used. Nothing on a server. |

## What it waits on

**The map file format, which is about to change twice.** Boxes
addressed by file ([311a](311a-boxes-addressed-by-file.md)) rewrites
the station line, and the map becoming code
([311d](311d-the-map-becomes-code.md)) changes what reading a map even
means. The canvas emits that format exactly, so building it against a
format with two pending changes means drawing it twice.

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
