// KiCad 10 macOS CLI DRC can query the GUI mouse while committing DRC markers.
// In a session without WindowServer, that aborts before the report is written.
// Return a dummy cursor position only; no DRC, geometry or rule APIs are changed.
extern void wxGetMousePosition(int*,int*);
struct Point { int x,y; };
extern Point wxGetMousePosition();
static void no_mouse(int*x,int*y){if(x)*x=0;if(y)*y=0;}
static Point no_point(){return {0,0};}
#define INTERPOSE(replacement,original,name) \
 __attribute__((used)) static struct {const void* replacement_ptr;const void* original_ptr;} name \
 __attribute__((section("__DATA,__interpose"))) = {(const void*)(replacement),(const void*)(original)}
INTERPOSE(no_mouse,static_cast<void(*)(int*,int*)>(wxGetMousePosition),mouse_interpose);
INTERPOSE(no_point,static_cast<Point(*)()>(wxGetMousePosition),point_interpose);
