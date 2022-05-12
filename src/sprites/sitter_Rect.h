#ifndef SITTER_RECT_H
#define SITTER_RECT_H

class Rect {
public:

  int x,y,w,h;
  
  Rect() {}
  Rect(int w,int h):x(0),y(0),w(w),h(h) {}
  Rect(int x,int y,int w,int h):x(x),y(y),w(w),h(h) {}
  Rect(const Rect &r):x(r.x),y(r.y),w(r.w),h(r.h) {}
  
  bool valid() const { return ((w>0)&&(h>0)); }
  void set(int x,int y,int w,int h) { this->x=x; this->y=y; this->w=w; this->h=h; }
  void move(int dx,int dy) { x+=dx; y+=dy; }
  void trap(const Rect &limit);
  Rect move_copy(int dx,int dy) const { Rect rtn(*this); rtn.move(dx,dy); return rtn; }
  Rect trap_copy(const Rect &limit) const { Rect rtn(*this); rtn.trap(limit); return rtn; }
  
  int left() const { return x; }
  int centerx() const { return x+(w>>1); }
  int right() const { return x+w; }
  int width() const { return w; }
  int top() const { return y; }
  int centery() const { return y+(h>>1); }
  int bottom() const { return y+h; }
  int height() const { return h; }
  
  void left(int n) { x=n; }
  void centerx(int n) { x=n-(w>>1); }
  void right(int n) { x=n-w; }
  void width(int n) { w=n; }
  void top(int n) { y=n; }
  void centery(int n) { y=n-(h>>1); }
  void bottom(int n) { y=n-h; }
  void height(int n) { h=n; }
  
  bool collides(const Rect &r) const { return ((right()>r.x)&&(r.right()>x)&&(bottom()>r.y)&&(r.bottom()>y)); }
  bool contains(const Rect &r) const { return ((r.x>=x)&&(r.y>=y)&&(r.right()<=right())&&(r.bottom()<=bottom())); }
  
  Rect &operator=(const Rect &r) { x=r.x; y=r.y; w=r.w; h=r.h; return *this; }
  bool operator==(const Rect &r) const { return ((x==r.x)&&(y==r.y)&&(w==r.w)&&(h==r.h)); }
  bool operator!=(const Rect &r) const { return ((x!=r.x)&&(y!=r.y)&&(w!=r.w)&&(h!=r.h)); }
  Rect operator&(const Rect &r) const; // intersection, may be invalid
  Rect &operator&=(const Rect &r);
  Rect operator|(const Rect &r) const; // union
  Rect &operator|=(const Rect &r);
  
};

#endif
