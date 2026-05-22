# Strategem — commit messages describe the runtime, not the issue

## The pattern

A commit message that uses scope-of-issue language ("these features
belong together," "this isn't done until both ship") gets re-read
later as architecture language ("these features are coupled," "these
two things are not independently deployable"). Future readers cannot
tell the difference between the two; they only see the commit text.
The lie compounds because no one re-reads issue files for archaeology
— they re-read `git log`.

Where this kept showing up: the 317 commit text said the C and Bash
bridges "live or die together," which was true about the *issue
contract* (issue 317 said every spec must implement both directions)
but false about the *architecture* (the C bridge ships independently
of the Bash bridge; each spec is its own .so dlopened in isolation;
adding a Python spec doesn't touch either of them). A future reader
looking at the log entry would conclude the system is more entangled
than it is, and might design around a coupling that doesn't exist.

## How to apply

When writing a commit message:

- Describe what the *runtime* now does — the artifact, the code path,
  the behavior the user / next maintainer can observe.
- Do not editorialise on top of the issue's bookkeeping. The fact
  that an issue says "every spec must X" doesn't make the X-es
  coupled; it makes them in-scope for the issue.
- If multiple files / specs ship in one commit, describe each as its
  own change, even when they share a motivation. "Spec A and spec B
  both gain X" is honest; "X for A and B is one indivisible unit" is
  not.
- The shape of the commit (one or many) is a presentation choice. The
  shape of the *system* is a property of the code. Don't conflate them.

## Why it's worth a strategem and not just a habit

Because when redesigns happen six months later, this is the trap
the team falls into: someone reads the log, decides the coupling
must be real, designs around it, and the system stays falsely
entangled because nobody had a reason to check whether the coupling
was load-bearing. The strategem catches it at commit time, where the
cost of writing two sentences honestly is much less than the cost of
the false belief propagating.
