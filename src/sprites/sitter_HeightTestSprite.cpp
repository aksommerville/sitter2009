#include "sitter_Game.h"
#include "sitter_HeightTestSprite.h"

#define BAD_FACE_TIME 60
#define HEIGHT_LIMIT 48 // >= this, we don't destroy you

HeightTestSprite::HeightTestSprite(Game *game):Sprite(game) {
  layer=-1;
  badfacetime=0;
}

HeightTestSprite::~HeightTestSprite() {
}

void HeightTestSprite::update() {
  Sprite::update();
  if (badfacetime>0) {
    if (!--badfacetime) setFace("ok");
  }
  for (int i=0;i<game->grp_crushable->sprc;i++) if (Sprite *spr=game->grp_crushable->sprv[i]) {
    // must be fully contained:
    if (spr->identity==SITTER_SPRIDENT_PLAYER) continue; // a ducking player is too small...
    if (spr->r.h>=HEIGHT_LIMIT) continue;
    if (spr->r.left()<r.left()) continue;
    if (spr->r.right()>r.right()) continue;
    if (spr->r.top()<r.top()) continue;
    if (spr->r.bottom()>r.bottom()) continue;
    spr->crush();
    setFace("bad");
    badfacetime=BAD_FACE_TIME;
  }
}
