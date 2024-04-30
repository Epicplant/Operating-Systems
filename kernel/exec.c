#include <cdefs.h>
#include <defs.h>
#include <elf.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <trap.h>
#include <x86_64.h>

int exec(char *path, char **argv) {

  // Acquire the number of arguments
  int argc = 0;
  for(argc = 0; argv[argc] != NULL; argc++);


  // Create a new vspace (struct vspace) and initialize it (vspaceinit)
    // If vspaceinit returns -1, we don’t have enough room in the kernel so we immediately exit
  struct vspace vp;

  
  if(vspaceinit(&vp) == -1)
    return -1;


  uint64_t rip;
  // Set up the new virtual spaces code with vspaceloadcode
    // The address of the file must be a valid .ELF file, and if it isn’t we return -1.
    // As a result, if we get a -1 here we should immediately exit after freeing the previously created vspace
  if(vspaceloadcode(&vp, path, &rip) == -1)
    return -1;


  // Call vspaceinitstack to initialize the new virtual spaces unique stack
  // at the address SZ_2G.
    // If this returns -1 then we immediately exit and return -1 after freeing the previously created vspace
  if(vspaceinitstack(&vp, SZ_2G) == -1)
    return -1;

  // Next, we need to update the process’ registers before we start the processes main with vspaceinstall.
    // Use vspacewritetova VR_USTACK



  // 1. We need to create the first stack frame on the stack which involves
  //    the arguments of main. As a result, we’ll need to use SZ_2G to get
  //    to the address of the stack, and then we will write out the string of every
  //    argument within argv so that we have the data for argv to point to. We will
  //    need to place them at varying byte offsets from SZ_2G depending on how many
  //    characters are in each string (since they will take up different amounts of space).
  //    Additionally, we will place the last arguments higher up then the earlier arguments
  //    (argc-1 then argc-2, etc.). Once we have the strings of every argument, we will pad
  //    with \0 until we are 8 byte aligned again
  
  // Position in stack
  uint64_t location = SZ_2G;
  uint64_t locations[argc];

  // Loop through argument list (last to first);
  for(int i = argc-1; i >= 0; i--) {
    
    // Get size of argument 
    int arglen = strlen(argv[i]) + 1;

   
    // Increment the location in the stack we will be placing things
    // and record its address
    location -= arglen;
    locations[i] = location;

    // Place argument into the stack
    vspacewritetova(&vp, location, argv[i], arglen);


  }

  // Pad by null terminators so location is 8 byte aligned
  int padding = location % 8;
  location -= padding;

  // 2. We will place NULL (argv[argc] = NULL) after the padding, so that we know we are passed the
  //    string data containing our arguments
 
  uint64_t ptr = 0;
  location -= sizeof(ptr);
  vspacewritetova(&vp, location, (char*) &ptr, sizeof(ptr));

  // 3. Now, we’ll place every every pointer going from argv[argc-1] to argv[0] to place the
  //    actual pointers within the argument argv on the stack (which will point to their
  //    corresponding strings that we placed on the stack earlier)
  for(int i = argc-1; i >= 0; i--) {
    
    // Increment the location in the stack we will be placing things;
    location -= sizeof(uint64_t);

    // Place argument into the stack
    vspacewritetova(&vp, location, (char*) &locations[i], sizeof(uint64_t));
  }    

  // 5. Finally, we will make %rdi in the trap frame equal to argc, we will make %rsi
  //    in the trap frame point to where we place the argv[0] pointer,
  //    we will make %rsp point to the top of the stack, and then
  //    we will point %rsi 8 bytes below where we placed argv[0] (as we begin creating main’s frame pointer).
 
  // Add RSI to stack
  location -= sizeof(rip);
  vspacewritetova(&vp, location, "0000000", 8);
 
  // Update trap frame
  struct trap_frame* frame = myproc()->tf;
  frame->rdi = argc;
  frame->rsi = (uint64_t) location+8;
  frame->rip = rip;
  frame->rsp = location;
 
  struct vspace old_space =  myproc()->vspace;

  // Set the new vspace as the current process's vspace while saving a reference to the old vspace
  myproc()->vspace = vp;

  // Activate the processes new vspace via vspaceinstall.
  vspaceinstall(myproc());

  // vspacedumpstack(&myproc()->vspace);

  // Free the old vspace to avoid any memory leaks (vspacefree).
  vspacefree(&old_space);

  // Does not return on success
  return 0;
}
