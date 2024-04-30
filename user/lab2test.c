#include <cdefs.h>
#include <fcntl.h>
#include <stat.h>
#include <stdarg.h>
#include <sysinfo.h>
#include <user.h>
#include <test.h>

// For concurrent_pipe_tests: how much to write at once. 
// Chosen to be about 2.5 kernel pages, forcing implementations
// to split a write into 3 pieces that need to be atomic
#define WRITE_AMT (10 * 1024)
// For concurrent_pipe_tests: how many concurrent writers to have
// One for each letter in the alphabet
#define NUM_WRITERS 26

void run_test(char*);
void fork_wait_exit_basic(void);
void fork_wait_exit_cleanup(void);
void fork_wait_exit_multiple(void);
void fork_wait_exit_50(void);
void fork_wait_exit_tree(void);
void fork_fd_test(void);
void pipe_test(void);
void pipe_closed_ends(void);
void pipe_concurrent(void);
void exec_bad_args(void);
void exec_ls(void);
void exec_echo(void);
void kill_test(void);

int main() {
  int pid, wpid;
  char buf[40];

  if (open("console", O_RDWR) < 0) {
    error("main: failed to open console file\n");
  }
  if (dup(0) < 0) {  // stdout
    error("lab1test: failed to dup stdin to stdout");
  }
  if (dup(0) < 0) {  // stderr
    error("lab1test: failed to dup stdin to stderr");
  }

  pid = fork();
  if (pid < 0) {
    error("fork failed");
    exit();
  }

  if (pid == 0) {
    while (true) {
      shell_prompt("lab2");
      memset(buf, 0, sizeof(buf));
      gets(buf, sizeof(buf));
      if (buf[0] == 0) {
        continue;
      }
      run_test(buf);
    }
  }

  while ((wpid = wait()) >= 0 && wpid != pid) {
    printf(stdout, "zombie!\n");
  }

  exit();
  return 0;
}

void run_test(char* test) {
  if (strcmp(test, "part1\n") == 0) {
    fork_wait_exit_basic();
    fork_wait_exit_cleanup();
    fork_wait_exit_multiple();
    fork_wait_exit_50();
    fork_wait_exit_tree();  
    fork_fd_test();
    pass("lab2 part1 tests!");
  } else if (strcmp(test, "part2\n") == 0) {
    pipe_test();
    pipe_closed_ends();
    pipe_concurrent();
    exec_bad_args();
    exec_ls();
    exec_echo();
    kill_test();
    pass("lab2 part2 tests!");
  } else if (strcmp(test, "all\n") == 0) {
    fork_wait_exit_basic();
    fork_wait_exit_cleanup();
    fork_wait_exit_multiple();
    fork_wait_exit_50();
    fork_wait_exit_tree();  
    fork_fd_test();
    pipe_test();
    pipe_closed_ends();
    pipe_concurrent();
    exec_bad_args();
    exec_ls();
    exec_echo();
    kill_test();
    pass("lab2 tests!");
  } else if (strcmp(test, "exit\n") == 0) {
    exit();
  } else if (strcmp(test, "fork_wait_exit_basic\n") == 0) {
    fork_wait_exit_basic();
  } else if (strcmp(test, "fork_wait_exit_cleanup\n") == 0) {
    fork_wait_exit_cleanup();
  } else if (strcmp(test, "fork_wait_exit_multiple\n") == 0) {
    fork_wait_exit_multiple();
  } else if (strcmp(test, "fork_wait_exit_50\n") == 0) {
    fork_wait_exit_50();
  } else if (strcmp(test, "fork_wait_exit_tree\n") == 0) {
    fork_wait_exit_tree();
  } else if (strcmp(test, "fork_fd_test\n") == 0) {
    fork_fd_test();
  } else if (strcmp(test, "pipe_test\n") == 0) {
    pipe_test();
  } else if (strcmp(test, "pipe_closed_ends\n") == 0) {
    pipe_closed_ends();
  } else if (strcmp(test, "pipe_concurrent\n") == 0) {
    pipe_concurrent();
  } else if (strcmp(test, "exec_bad_args\n") == 0) {
    exec_bad_args();
  } else if (strcmp(test, "exec_ls\n") == 0) {
    exec_ls();
  } else if (strcmp(test, "exec_echo\n") == 0) {
    exec_echo();
  } else if (strcmp(test, "kill_test\n") == 0) {
    kill_test();
  } else {
    printf(stderr, "input matches no test: %s" , test);
  }
}

// test basic interaction between fork, wait, and exit
void fork_wait_exit_basic(void) {
  test("fork_wait_exit_basic");

  int pid, tmp;

  // try to wait when there's no child
  if (wait() != -1) {
    error("fork_wait_exit_basic: able to wait when there's no child");
  }

  pid = fork();
  if (pid == 0) {
    exit(); // child exits
    error("fork_wait_exit_basic: child returned from exit");
  }

  // only executed by the parent
  tmp = wait();
  if (tmp != pid) {
    error("fork_wait_exit_basic: wait returned wrong value %d, expecting child pid %d", tmp, pid);
  }

  pass("");  
}

// test whether all of child's memory is cleaned up after wait and exit
void fork_wait_exit_cleanup(void) {
  test("fork_wait_exit_cleanup");

  int pid, tmp, used_pages;
  struct sys_info info;

  // save pages used before fork
  assert(sysinfo(&info) == 0);
  used_pages = info.pages_in_use; 

  pid = fork();
  if (pid == 0) {
    exit(); // child exits
    error("fork_wait_exit_cleanup: child returned from exit");
  }

  // only executed by the parent
  tmp = wait();
  if (tmp != pid) {
    error("fork_wait_exit_cleanup: wait returned wrong value %d, expecting child pid %d", tmp, pid);
  }

  // check pages post exit
  assert(sysinfo(&info) == 0);
  if (info.pages_in_use != used_pages) {
    assert(info.pages_in_use >= used_pages);
    error("fork_wait_exit_cleanup: child's memory is not fully cleaned up after wait and exit, %d pages of memory lost", info.pages_in_use - used_pages);
  }

  pass("");  
}

void fork_wait_exit_multiple(void) {
  test("fork_wait_exit_multiple");

  int n, pid;
  int nproc = 6;

  for (n = 0; n < nproc; n++) {
    pid = fork();
    if (pid < 0) {
      break;
    }
    if (pid == 0) {
      exit(); // child exits
      error("fork_wait_exit_multiple: child returned from exit");
    }
  }

  if (n != nproc) {
    error("fork_wait_exit_multiple: tried to fork %d times but only succeeded %d times", nproc, n);
  }

  for (; n > 0; n--) {
    if (wait() < 0) {
      error("fork_wait_exit_multiple: wait stopped early");
    }
  }

  if (wait() != -1) {
    error("fork_wait_exit_multiple: wait succeeded when no child is left");
  }

  pass("");
}

void fork_wait_exit_50(void) {
  test("fork_wait_exit_50");

  int i, pid;

  // fork 100 children
  for (i = 0; i < 50; i++) {
    pid = fork();
    if (pid < 0) {
      error("fork_wait_exit_50: fork failed at iteration %d", i);
    }
    if (pid == 0) { // child case
      exit();
      error("fork_wait_exit_50: returned from exit");
    }
  }

  // wait 100 times
  for (i = 0; i < 50; i++) {
    if (wait() < 0) {
      error("fork_wait_exit_50: wait failed at iteration %d", i);
    }
  }

  pass("");
}


static void tree_helper(int depth) {
  int pids[3];
  int i, j, p;

  if (depth == 0) {
    return;
  }

  for (i = 0; i < 3; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      error("tree_helper: failed to fork child at depth %d, iteration %d", depth, i); 
    }

    if (pids[i] == 0) { // child case
      tree_helper(depth-1); // each child spawns 3 children if depth > 0
      exit();
      error("tree_helper: returned from exit"); 
    }
  }

  for (i = 0; i < 3; i++) {
    p = wait();
    if (p < 0) {
      error("tree_helper: wait failed, depth %d, iteration %d", depth, i);
    }

    // look through child pids list, mark waited child
    for (j = 0; j < 3; j++) {
      if (p == pids[j]) {
        pids[j] = -1;
        break;
      }
    }

    // unable to find the waited pid in the child list
    if (j == 3) {
      error("tree_helper: returned pid was not a child or has already been waited, wait returned %d", p);
    }
  }

  // No more children
  if (wait() != -1) {
    error("tree_helper: wait succeeded when no child is left");
  }
}

// a fork tree, child forks more children
void fork_wait_exit_tree(void) {
  test("fork_wait_exit_tree");
  tree_helper(2);
  pass("");
}

// check that fork has the correct file descriptor semantics
void fork_fd_test(void) {
  test("fork_fd_test");

  int pid;
  int fd1, fd2, fd3, fd4, fd5;
  char buf[11] = {0};

  // open a file, fd should be inherited by child
  assert((fd1 = open("l2_share.txt", O_RDONLY)) >= 0);

  pid = fork();
  if (pid == 0) { // child case
    if (read(fd1, buf, 10) != 10) {
      error("fork_fd_test: failed to read 10 bytes from fd1");
    }
    if (strcmp("cccccccccc", buf) != 0) {
      error("fork_fd_test: should have read 10 c's but read '%s' instead", buf);
    }
    exit();
    error("fork_fd_test: returned from exit");
  }

  // only executed by parent
  assert(wait() == pid);
  
  // child should have advanced file offsets to 10 bytes, read the next 10 bytes
  if (read(fd1, buf, 10) != 10) {
    error("fork_fd_test: failed to read 10 bytes from fd1");
  }

  if (strcmp("ppppppppp\n", buf) != 0) {
    error("fork_fd_test: should have reead 9 p's (and a newline), but read '%s' instead", buf);
  }

  assert(close(fd1) == 0); 

  // set up more file descriptors and make sure child inherits the current view at fork

  assert((fd2 = open("l2_share.txt", O_RDONLY)) > -1);
  assert((fd3 = open("l2_share.txt", O_RDONLY)) > -1);
  assert((fd4 = open("l2_share.txt", O_RDONLY)) > -1);
  assert((fd5 = open("l2_share.txt", O_RDONLY)) > -1);

  assert(close(fd3) != -1);
  assert(close(fd4) != -1);

  pid = fork();
  if (pid == 0) { // child case
    assert(read(fd2, buf, 10) == 10);
    assert(read(fd3, buf, 10) == -1); // this fd shouldn't be open
    assert(read(fd4, buf, 10) == -1); // this fd shouldn't be open
    assert(read(fd5, buf, 10) == 10);
    exit();
    error("fork_fd_test: returned from exit");
  }

  assert(close(fd2) == 0);
  assert(close(fd5) == 0);

  // only executed by parent
  assert(wait() == pid);
  pass("");
}

void pipe_test(void) {
  test("pipe_test");

  char buf[500];
  int fds[2], pid;
  int seq, i, n, cc, total;

  seq = 0;
  total = 0;

  // pipe fills in array fds with a read end at idx 0, write end at idx 1
  if (pipe(fds) != 0) {
    error("pipe_test: pipe failed");
  }

  pid = fork();
  if (pid < 0) {
    error("pipe_test: fork failed");
  }
  
  if (pid == 0) { // child case
    assert(close(fds[0]) != -1); // closes the read end, child only writes to the pipe

    // fill buffer with 0-499
    for (i = 0; i < 500; i++) {
      buf[i] = seq++;
    }

    // starting write size
    cc = sizeof(buf)/2;
    while (total < 500 && (n = write(fds[1], buf+total, cc)) > 0) {
      total += n;
      cc = cc / 2;
      if (cc == 0 || total + cc > 500) {
        cc = 500 - total;
      }
    }

    if (total != 500) {
      error("pipe_test: writer failed to write alll 500 bytes, wrote %d bytes", total);
    }
    exit();
  }
  
  // only executed by parent

  assert(close(fds[1]) != -1); // closes the write end, parent only reads from the pipe

  cc = 1;
  while ((n = read(fds[0], buf, cc)) > 0) {
    // check validity of the read
    for (i = 0; i < n; i++) {
      if ((buf[i] & 0xff) != (seq++ & 0xff)) {        
        error("pipe_test: read incorrect value %d, expecting %d\n", buf[i] & 0xff, seq & 0xff);
      }
    }
    
    total += n;
    cc = cc * 2;
    if (cc > sizeof(buf)) {
      cc = sizeof(buf);
    }
  }

  if (total != 500) {
    error("pipe_test: faile to read all 500 bytes, read %d bytes total", total);
  }

  assert(close(fds[0]) == 0);
  assert(wait() == pid);

  pass("");
}

// test for pipe behavior when all read ends or all write ends are closed
void pipe_closed_ends(void) {
  test("pipe_closed_ends");

  int pid;
  int n, cc, len, total;
  int fds[2];
  char *buf = "pipetestwrite";

  n = 0;
  total = 0;
  len = strlen(buf) + 1;
  cc = len;

  if (pipe(fds) != 0) {
    error("pipe_closed_ends: pipe() failed");
  }

  pid = fork();
  if (pid < 0) {
    error("pipe_closed_ends: fork() failed");
  }

  if (pid == 0) { // child case
    while (total < len && (n = write(fds[1], buf+total, cc)) > 0) {
      total += n;
      if (total != len) {
        cc = len - total;
      }
    }

    if (total != len) {
      error("pipe_closed_ends: writer failed to write %d bytes, wrote %d bytes", len, total);
    }

    exit(); // should close all opened fds, including read and write ends
    error("pipe_closed_ends: returned from exit()");
  }

  // only executed by parent

  assert(close(fds[1]) == 0); // parent closes write end
  if (wait() != pid) {
    error("pipe_closed_ends: wait() failed\n");
  }

  // read all content from the pipe after all write ends are closed
  char buf2[20];
  while(cc > 0) {
    if ((n = read(fds[0], buf2+total, cc)) <= 0) {
      error("pipe_closed_ends: failed to read from a non empty pipe, read returned %d", n);
    }
    total += n;
    cc -= n;
  }

  // all pipe data is read, more reads should return EOF
  if (read(fds[0], buf, 1) != 0) {
    error("pipe_closed_ends: read() returns non-zero at EOF");
  }

  if (strcmp(buf, buf2) != 0) {
    error("pipe_closed_ends: read wrong data from pipe, read %s instead of %s", buf2, buf);
  }

  // close read ends, all pipe fds should be closed by now
  assert(close(fds[0]) != -1);

  // start a new pipe to test for write to closed read ends
  assert(pipe(fds) != -1);
  assert(write(fds[1], buf, len) >= 0); // write should succeed
  assert(close(fds[0]) != -1);  // close read end

  // try to write to a pipe with no more read end
  if (write(fds[1], buf, len) != -1) {
    error("pipe_closed_ends: can write to a pipe with no read ends open");
  }
  assert(close(fds[1]) != -1);  // close read end

  pass("");
}

/**
 * Opens a pipe then forks NUM_WRITERS times.
 * Each writer writes WRITE_AMT bytes to the pipe
 * 
 * The parent process reads the pipe on a delay
 * until all expected bytes are in to ensure that 
 * multiple writers are in contention for the pipe.
 * It then ensures there is no interleaving of read 
 * bytes.
*/
void pipe_concurrent(void) {
  test("pipe_concurrent");
  int fds[2] = {0};
  if (pipe(fds) < 0)
    error("pipe failed\n");
  int pid = fork();

  if (pid == 0) {
    // Child
    char buf[WRITE_AMT] = {0};
    close(fds[0]);
    int i;
    for (i = 1; i < NUM_WRITERS; i++) {
      pid = fork();
      if (pid == 0) {
        break;
      }
    }

    for (int j = 0; j < WRITE_AMT; j++) {
      buf[j] = 'A' + i - 1;
    }
    int written;
    if ((written = write(fds[1], (void *)buf, WRITE_AMT)) != WRITE_AMT) {
      error("Required complete write, got %d", written);
    }

    if (pid != 0) {
      while (wait() != -1);
    }
    exit();
  }
  else {
    // Parent
    char buf[WRITE_AMT] = {0};
    close(fds[1]);
    bool found[NUM_WRITERS] = {0};
    int total_read = 0;
    while (true) {
      sleep(10);
      int amount_read = 0;
      while (amount_read < WRITE_AMT) {
        int just_read = read(fds[0],
                             (void *)(buf + amount_read),
                             WRITE_AMT - amount_read);
        if (just_read < 0) {
          error("Failed to read");
        }
        if (just_read == 0) {
          goto readdone;
        }
        if (just_read > WRITE_AMT - amount_read) {
          error("Expected to read %n bytes, got %n", WRITE_AMT - amount_read, just_read);
        }
        amount_read += just_read;
      }
      // printf(stdout, "Parent done reading.\n");
      // printf(stdout, "Read: %s\n", buf);

      // Check if the writes are atomic.
      char c = buf[0];
      char writer = c - 'A';
      if (writer < 0 || writer >= NUM_WRITERS) {
        error("Invalid value written: %c (must be less than %c and greater than %c)", buf[0], (char)(NUM_WRITERS + 'A'), (char)('A' - 1));
      }
      if (found[(int) writer]) {
        error("Writes should only happen once, but already found write for %d", (int)(c - 'A'));
      }
      found[c - 'A'] = true;
      // All WRITE_AMT of the next chars should be c.
      for (int j = 0; j < WRITE_AMT; j++) {
        if (buf[j] != c)
          error("Write not atomic (expected '%c', got '%c')", c, buf[j]);
      }
      total_read += amount_read;
    }

  readdone:
    if (total_read < NUM_WRITERS * WRITE_AMT) {
      error("Expected %n bytes, got %n", NUM_WRITERS * WRITE_AMT, total_read);
    }
    // Check that all writes completed exactly once.
    for (int i = 0; i < NUM_WRITERS; i++) {
      if (!found[i])
        error("not all writes completed once");
    }

    int children = 0;
    while (wait() != -1) {
      children++;
    }
    if (children == 0) {
      error("Found 0 children, expected at least 1");
    }

    pass("");
  }
}


void exec_bad_args(void) {
  test("exec_bad_args");

  int pid = fork();
  if (pid < 0) {
    error("exec_bad_args: fork failed");
  }

  if (pid == 0) { // child
    // test for invalid argv
    char **argv1 = (char **)0xf00df00d;
    if (exec("ls", argv1) != -1) {
      error("exec_bad_args: invalid argv");
    }
    // test for invalid argv[1]
    char *argv3[] = { "ls", (char *) 0xf00df00d, 0 };
    if (exec("ls", argv3) != -1) {
      error("exec_bad_args: argv[1] out of stack");
    }
    exit();
  }
  assert(wait() == pid);

  pass("");
}

int get_token(char **ps, char *es, char**start, int* end) {
  char *s;
  int ret;
  char whitespace[] = " \t\r\n\v";

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  ret = *s;
  switch (*s) {
    case 0:
      break;
    default:
      *start = s;
      ret = 'a';
      while (s < es && !strchr(whitespace, *s))
        s++;
      break;
  }
  *end = s - *start;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

void check_ls_output(char* line, int line_len) {
  int i, tok_len;
  char *end_line;
  char* toks[5];
  struct stat stat_info;

  printf(stdout, "%s\n", line); // ls line output

  i = 0;
  tok_len = 0;
  end_line = line + line_len;

  while(get_token(&line, end_line, &toks[i], &tok_len)) {
    if (i == 4) {
      error("check_ls_output: too many tokens");
    }
    toks[i][tok_len] = 0;
    i++;
  }

  if (i != 4) {
    error("check_ls_output: did not get enough tokens, got %d", i);
  }

  assert(stat(toks[0], &stat_info) == 0);
  assert(atoi(toks[1]) == stat_info.type);
  assert(atoi(toks[2]) == stat_info.ino);
  assert(atoi(toks[3]) == stat_info.size);
}

void exec_ls(void) {
  test("exec_ls");

  int pid, i, n, total;
  int fds[2];
  char *argv[] = {"ls", 0};
  static char buf[PGSIZE];

  assert(pipe(fds) == 0);

  if ((pid = fork()) < 0) {
    error("exec_ls: fork failed");
  }
  if (pid == 0) { // child case
    assert(close(fds[0]) == 0); // close pipe read end
    assert(close(stdout) == 0);
    assert(dup(fds[1]) == stdout); // redirect pipe write end to stdout
    assert(close(fds[1]) == 0);
    exec("ls", argv);
    error("exec_ls: exec ls failed");
  }
  assert(close(fds[1]) == 0); // close pipe write end
  assert(wait() == pid);

  // read outputs by ls
  i = 0;
  n = 0;
  total = 0;

  printf(stdout, "\n------ ls output -------\n");

  while(read(fds[0], buf+total, 1) > 0) {
    if (buf[total] == '\n') {
      buf[total] = '\0';
      check_ls_output(buf+i, total-i);
      i = total+1;
      n++;
    }
    total++;
  }
  printf(stdout, "-------------------------\n");

  if (n < 25) {
    error("exec_ls: child process 'ls' failed to stat all 25 directory entries, only got %d entires", n);
  }

  pass("");
}

void exec_echo(void) {
  test("exec_echo");
  char *echoargv[] = {"echo", "echotest", "ok", 0};
  int fds[2];

  assert(pipe(fds) == 0);

  int pid = fork();
  if (pid < 0) {
    error("exec_echo: fork failed");
  }

  if (pid == 0) { // child case
    assert(close(stdout) == 0);     // close child's inherited stdout
    assert(dup(fds[1]) == stdout);  // redirect pipe write end to stdout, future writes to stdout can be read from the pipe
    exec("echo", echoargv);
    error("exec_echo: exec echo failed");
  }

  assert(wait() == pid);

  // read output from child
  // printf(stdout, "exec_echo: test output\n");
  assert(close(fds[1]) == 0); // close write end, read until EOF

  char buf[16];
  char *expected = "echotest ok\n";

  int byte_to_read = 15;
  int count = 0;
  
  while (count < 12) {
    int ret = read(fds[0], buf + count, byte_to_read);
    if (ret <= 0) {
      error("exec_echo: echo produced no output");
    }
    byte_to_read -= ret;
    count += ret;
  }

  buf[count] = 0;
  if (strcmp(buf, expected) != 0) {
    error("exec_echo: echo produced invalid output, expected %s, got %s", expected, buf);
  }

  pass("");
}

void kill_test(void) {
  test("kill_test");

  int pid1, pid2, pid3;
  int fds[2];
  char buf[11];

  pid1 = fork();
  assert(pid1 != -1);
  if (pid1 == 0) { // child goes into infinite loop
    for (;;)
      ;
  }

  pid2 = fork();
  assert(pid2 != -1);
  if (pid2 == 0) { // child goes into infinite loop
    for (;;)
      ;
  }
    
  assert(pipe(fds) == 0);
  pid3 = fork();
  assert(pid3 != -1);

  if (pid3 == 0) { // child case
    assert(close(fds[0]) == 0);
    if (write(fds[1], "x", 1) != 1) {
      error("kill_test: write error");
    }
    assert(close(fds[1]) == 0);
    for (;;)
      ;
  }

  // only executed by parent
  assert(close(fds[1]) == 0);
  if (read(fds[0], buf, sizeof(buf)) != 1) {
    error("kill_test: read error");
  }
  assert(close(fds[0]) == 0);

  assert(kill(pid1) == 0);
  assert(kill(pid2) == 0);
  assert(kill(pid3) == 0);

  assert(wait() > 0);
  assert(wait() > 0);
  assert(wait() > 0);

  pass("");
}
