# How to Read Specs

Howdy and welcome to CSE 451. Specs are contained in the same folder as this file: `lab/`. This document was created to address common confusions/anxieties students had when first working through the `lab1.md` lab spec.

## Specs Specify What You Need to Do

While they are interspersed with significant amounts of teaching content, the end goal of the specs are to provide you with a *specification* of what you need to implement to get credit for the labs. There are two graded components to every lab (that the spec specifies):
- Lab questions
- Code specifications

### Lab Questions

As you read through the specs, there will occasionally be questions marked under a header titled "Question #X", like so:

```markdown
#### Question #3:

This is an example question.

```

You should log your answers to such questions in a `txt` file. If you are stumped on a question feel free to move on and come back later, knowing the answer to a question should not be a barrier for continuing to understand the rest of the spec.

There will be an assignment on Gradescope which you should submit your answers to the lab questions to.

### Code specifications

The spec will also explain what you are expected to implement in code. The only way that the testing code (intentionally) interacts with your kernel is through syscalls.

e.g., from lab1:
```C
/*
 * arg0: char * [path to the file]
 * arg1: int [mode for opening the file (see inc/fcntl.h)]
 * [... skipped for brevity]
 */
int
sys_open(void);
```

So your goal as a student is to implement all syscalls that are listed this way. You are done once your implementation pass the provided tests for the lab. The lab specs explain how to run the tests for each lab. There are no hidden tests. As long as you haven't modified the test files, passing the tests locally should generally mean they'll pass when submitted to the autograder.

**Bottom Line:** For this reason you can also view the tests as the purest ground-truth for what you need to do. Make the tests pass and you're good.

> [`starting.md`](../docs/development/starting.md) can provide some helpful tips for right before you start implementing.

## Conclusion

With that you should be ready to go! Feel free to send any questions you have while working on the labs to the staff.
