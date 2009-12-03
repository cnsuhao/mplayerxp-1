/*
   Shared object testing
*/

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[])
{
  if(argc > 1)
  {
    void *dhandle;
    if(!(dhandle=dlopen(argv[1],RTLD_LAZY|RTLD_GLOBAL)))
    {
      printf("Can't load library: %s because: %s\n",argv[1],dlerror());
      return EXIT_FAILURE;
    }
    printf("library '%s' was loaded successfully\n",argv[1]);
    dlclose(dhandle);
  }
  else printf("Too few arguments\n");
  return EXIT_SUCCESS;
}
