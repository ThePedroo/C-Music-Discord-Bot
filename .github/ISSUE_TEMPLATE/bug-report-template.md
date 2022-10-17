---
name: Bug report
about: Help the project be more stable reporting bugs
title: '[bug]: '
labels: bug
assignees: ''
---

## What's wrong?

Inform here what happened.

## Reproducing

Let us know how to reproduce this bug.

## Crashes (debug)

If the bot crashed, please debug it with gdb, following the example below:

```console
$ gdb ./ConcordBot
```

Then, press `c` and `enter`, and now that you are on GDB's shell, please use the command `run`.

You will need to reproduce the crash, and when you successfully reproduce it and crashes the bot, please use the command `bt` and send what you got from the response here.

## Screenshots

If possible, let a screenshot here, if not, please remove this section.

## Final

- [ ] I confirm that I've tried reproducing this bug on the latest release and I'm still able to reproduce