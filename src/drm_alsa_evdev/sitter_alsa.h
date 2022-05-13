/* sitter_alsa.h
 */
 
#ifndef SITTER_ALSA_H
#define SITTER_ALSA_H

#include <stdint.h>

class AudioManager;

void sitter_alsa_quit();

int sitter_alsa_init(
  int rate,int chanc,
  void (*cb)(AudioManager *audio,int16_t *v,int len_bytes),
  AudioManager *audio
);

int sitter_alsa_lock();
int sitter_alsa_unlock();

#endif
