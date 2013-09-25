/*    
   - The headless server [default]
   - The heartbeat agent -DHEARTBEAT
   - The admin server    -DADMIN
   
 */

#include<stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int admin =
#ifdef ADMIN
  1;
#else
  0;
#endif

static char server_module[]=
#ifdef HEARTBEAT
#include "RunHeartbeat.lch"  
#else
#include "RunServer.lch"
#endif

static char framework_loader[]=
#include "FrameworkLoader.lch"
  


#ifdef WIN32

#define LIB_SUFFIX "dll"
#define DEFAULT_LIBRARY_PATH "C:/Program Files/PathPoint/Application"
  
#else 

#define LIB_SUFFIX "so"
#define DEFAULT_LIBRARY_PATH "../Application"

#endif

int start_server(const char *);
  
int main(int argc, const char* argv[]) {
  const char *path = DEFAULT_LIBRARY_PATH;
  
  if(argc > 1) {
    path = argv[1];
  }
    
  printf("Starting server with path: %s\n",path);  
  return start_server(path);
}

int start_server(const char *mpath) { 
  int stopped  = 0;
  int result   = 1;
  lua_State *L = lua_open();
  
  luaL_openlibs(L);
  
  lua_pushboolean(L,admin);
  lua_setglobal(L,"ADMINISTRATION");
  
  lua_pushstring(L, mpath);
  lua_setglobal(L, "LIBRARY_PATH");
  
  lua_pushstring(L, LIB_SUFFIX);
  lua_setglobal(L, "LIB_SUFFIX");

  
  /*Framework Loader*/
  result = luaL_loadbuffer(L,framework_loader, sizeof(framework_loader),"=framework_loader");
  if(result) {
    printf("Error loading PathPoint Lua FrameworkLoader (%d) \n%s\n",		
	   result, lua_tostring(L,-1));	    			
    lua_pop(L,1);			
    exit(result);    
  }  
  result = lua_pcall(L,0,0,0);
  if(result) {
    printf("Error running PathPoint Lua FrameworkLoader (%d) \n%s\n",result, lua_tostring(L,-1));
    lua_pop(L,1);
    exit(result);
  }
  
  
  /*Server Module*/
  result = luaL_loadbuffer(L,server_module,sizeof(server_module),"=server_module");
  if(result) {
    printf("Error loading PathPoint Server module (%d) \n%s\n",		
	   result, lua_tostring(L,-1));	    			
    lua_pop(L,1);			
    exit(result);    
  }  
  result = lua_pcall(L,0,0,0);
  if(result) {
    printf("Error running PathPoint Lua Server module (%d) \n%s\n",result, lua_tostring(L,-1));
    lua_pop(L,1);
    exit(result);
  }
   
  
  lua_close(L);
  
  /* The stop server function returns a number */
  return (int)luaL_checknumber(L, -1);
}

