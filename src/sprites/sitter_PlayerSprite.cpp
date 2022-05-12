#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_AudioManager.h"
#include "sitter_SpriteFace.h"
#include "sitter_Player.h"
#include "sitter_AnimateOnceSprite.h"
#include "sitter_PlayerSprite.h"

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
PlayerSprite::PlayerSprite(Game *game):Sprite(game) {
  plr=NULL;
  crushed=false;
  identity=SITTER_SPRIDENT_PLAYER;
}

PlayerSprite::~PlayerSprite() {
}

/******************************************************************************
 * events
 *****************************************************************************/
 
void PlayerSprite::update() {
  if (plr) plr->suspended=false;
  if (alive) {
    if (!update_gravity()) {
      if (gravityok&&plr&&plr->carry) { // check for suspension
        SpriteGroup *bindrestore=boundsprites;
        boundsprites=game->grp_alwaysempty;
        bool legsdangling=move(0,1,false);
        boundsprites=bindrestore;
        if (legsdangling) {
          bindrestore=plr->carry->boundsprites;
          plr->carry->boundsprites=game->grp_alwaysempty;
          bool objfree=plr->carry->move(0,1,false);
          plr->carry->boundsprites=bindrestore;
          if (objfree) {
            plr->carry->r.y--;
          } else {
            plr->suspended=true;
            plr->walkmotor->stop();
          }
          r.y--;
        }
      }
    }
    update_inertia();
  } else if (crushed) {
    //move(0,4);
  }
  update_animation();
}

void PlayerSprite::updateBackground() {
  update_animation();
}
 
void PlayerSprite::gridKill(int dx,int dy,int col,int row) {
  if (!alive) return;
  plr->toss_on();
  plr->toss_off();
  alive=false;
  pushable=false;
  removeGroup(game->grp_carry);
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  if (!game->grid||(game->grid->play_mode==SITTER_GRIDMODE_COOP)) game->murderc++;
  if (dx<0) {
    flop=false;
    setFace("impalel");
    r.centerx((col+1)*game->grid->colw-dx);
  } else if (dx>0) {
    flop=false;
    setFace("impaler");
    r.centerx(col*game->grid->colw-dx);
  } else if (dy<0) {
    setFace("impaleu");
    r.w=48;
    r.h=32;
    r.centery((row+1)*game->grid->rowh-dy);
  } else {
    setFace("impaled");
    r.w=48;
    r.h=32;
    r.centery(row*game->grid->rowh-dy);
  }
  sitter_makeExplosion(game,r);
  game->audio->playEffect("impale");
}

void PlayerSprite::crush() {
  if (!alive) return;
  plr->toss_on();
  plr->toss_off();
  alive=false;
  crushed=true;
  // killing a player only counts as murder in cooperative mode
  if (!game->grid||(game->grid->play_mode==SITTER_GRIDMODE_COOP)) game->murderc++;
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  removeGroup(game->grp_solid);
  removeGroup(game->grp_carry);
  sprite_collisions=false;
  while (move(0,32)>0) ;
  r.y+=(r.h>>1);
  layer=-1;
  setFace("splatter");
  if (beingcarried) removeGroup(game->grp_vis); // just disappear (don't make splatter pile in mid-air)
  sitter_makeExplosion(game,r);
  game->audio->playEffect("crush");
}

void PlayerSprite::slice() {
  if (!alive) return;
  plr->toss_on();
  plr->toss_off();
  alive=false;
  if (!game->grid||(game->grid->play_mode==SITTER_GRIDMODE_COOP)) game->murderc++;
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  removeGroup(game->grp_solid);
  removeGroup(game->grp_carry);
  removeGroup(game->grp_vis);
  sitter_makeExplosion(game,r);
  game->audio->playEffect("slice");
}

void PlayerSprite::enterCannon(Sprite *cannon) {
  plr->ctl_cannon=(CannonSprite*)cannon;
}

void PlayerSprite::exitCannon(Sprite *cannon) {
  plr->ctl_cannon=NULL;
}
