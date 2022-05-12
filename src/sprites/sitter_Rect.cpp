#include "sitter_Rect.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN1(a,b) ({ register int _a=(a); register int _b=(b); MIN(_a,_b); })
#define MAX1(a,b) ({ register int _a=(a); register int _b=(b); MAX(_a,_b); })

void Rect::trap(const Rect &r) {
  if (w>=r.w) centerx(r.centerx());
  else if (x<r.x) x=r.x;
  else if (right()>r.right()) right(r.right());
  if (h>=r.h) centery(r.centery());
  else if (y<r.y) y=r.y;
  else if (bottom()>r.bottom()) bottom(r.bottom());
}

Rect Rect::operator&(const Rect &r) const {
  int sleft=MAX(x,r.x);
  int stop=MAX(y,r.y);
  int sright=MIN1(right(),r.right());
  int sbottom=MIN1(bottom(),r.bottom());
  return Rect(sleft,stop,sright-sleft,sbottom-stop);
}

Rect &Rect::operator&=(const Rect &r) {
  int sleft=MAX(x,r.x);
  int stop=MAX(y,r.y);
  int sright=MIN1(right(),r.right());
  int sbottom=MIN1(bottom(),r.bottom());
  x=sleft; y=stop; w=sright-sleft; h=sbottom-stop;
  return *this;
}

Rect Rect::operator|(const Rect &r) const {
  int sleft=MIN(x,r.x);
  int stop=MIN(y,r.y);
  int sright=MAX1(right(),r.right());
  int sbottom=MAX1(bottom(),r.bottom());
  return Rect(sleft,stop,sright-sleft,sbottom-stop);
}

Rect &Rect::operator|=(const Rect &r) {
  int sleft=MIN(x,r.x);
  int stop=MIN(y,r.y);
  int sright=MAX1(right(),r.right());
  int sbottom=MAX1(bottom(),r.bottom());
  x=sleft; y=stop; w=sright-sleft; h=sbottom-stop;
  return *this;
}
