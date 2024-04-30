# Starting

A quick guide on more subjective mindset-level tips for how/when to start CSE 451 development.

## When to Start Implementing

For lab specs with indicated parts, you're usually expected to complete the implementation laid out in a single part before continuing to the next. This means however that you should typically read to at *least* the end of a section before starting to implement. Often context given further on in the spec can help you understand earlier parts.

Run the tests relevant to what you've implemented so far early and often. Not all parts of a lab need to be completed to start testing your code.

## How to Start Implementing

You should/will have a design doc for each lab. For lab1 its provided, and for labs beyond that we require you to make one. Once you've thought through the implementation usually you'll probably have a good idea of where to start, there's usually few bad answers here.

A key mindset you should have when implementing though is that **the repo is your oyster**. You should feel free to edit any file you need to to make your OS work, with one small caveat. Just be aware that when submitting there's a list of files which the autograder overwrites when testing your code (in order to ensure you don't cheat the tests). This means any changes made to these files will be lost when submitting:
- Everything in `user/`
- `inc/param.h`, `inc/test.h`
- `kernel/Makefrag`, `kernel/initcode.S`
- `Makefile`, `crash_safety_test.py`, `sign.pl`

Even then, if modifying one of these files (typically the tests in `user/`) helps you during debugging, you should feel free to go at it (just note that none of the logic your OS needs to work that you add should be in these files).

As long as you use `git` to track good states of your repo, it's good to explore designs with the understanding that any file is free to be edited.