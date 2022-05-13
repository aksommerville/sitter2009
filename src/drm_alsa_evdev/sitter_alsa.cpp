#include "sitter_alsa.h"
extern "C" {
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <string.h>
#include <stdio.h>
}

#define SITTER_ALSA_BUFFER_SIZE 1024

/* Globals.
 */
 
static struct {

  void (*cb)(AudioManager *audio,int16_t *v,int c);
  AudioManager *audio;
  unsigned int rate,chanc;

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int hwbuffersize;
  int bufc; // frames
  int bufc_samples;
  char *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort;
  int cberror;
  
} sitter_alsa={0};

/* I/O thread.
 */
 
static void *sitter_alsa_iothd(void *arg) {
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);
  const int samplesize=2;
  while (1) {
    pthread_testcancel();

    if (pthread_mutex_lock(&sitter_alsa.iomtx)) {
      sitter_alsa.cberror=1;
      return 0;
    }
    sitter_alsa.cb(sitter_alsa.audio,(int16_t*)sitter_alsa.buf,sitter_alsa.bufc_samples*sizeof(int16_t));
    pthread_mutex_unlock(&sitter_alsa.iomtx);
    if (sitter_alsa.ioabort) return 0;

    char *samplev=sitter_alsa.buf;
    int samplep=0,samplec=sitter_alsa.bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      int err=snd_pcm_writei(sitter_alsa.alsa,samplev+samplep*samplesize,samplec-samplep);
      if (sitter_alsa.ioabort) return 0;
      if (err<=0) {
        if ((err=snd_pcm_recover(sitter_alsa.alsa,err,0))<0) {
          sitter_alsa.cberror=1;
          return 0;
        }
        break;
      }
      samplep+=err;
    }
  }
  return 0;
}

/* Quit.
 */
 
void sitter_alsa_quit() {
  sitter_alsa.ioabort=1;
  if (sitter_alsa.iothd&&!sitter_alsa.cberror) {
    pthread_cancel(sitter_alsa.iothd);
    pthread_join(sitter_alsa.iothd,0);
  }
  pthread_mutex_destroy(&sitter_alsa.iomtx);
  if (sitter_alsa.hwparams) snd_pcm_hw_params_free(sitter_alsa.hwparams);
  if (sitter_alsa.alsa) snd_pcm_close(sitter_alsa.alsa);
  if (sitter_alsa.buf) free(sitter_alsa.buf);
  memset(&sitter_alsa,0,sizeof(sitter_alsa));
}

/* Init.
 */

int sitter_alsa_init(
  int rate,int chanc,
  void (*cb)(AudioManager *audio,int16_t *v,int c),
  AudioManager *audio
) {
  
  sitter_alsa.cb=cb;
  sitter_alsa.audio=audio;
  sitter_alsa.rate=rate;
  sitter_alsa.chanc=chanc;
  
  snd_pcm_format_t format=SND_PCM_FORMAT_S16;
  int samplesize=2;
  const char *device="default";
  if (
    (snd_pcm_open(&sitter_alsa.alsa,device,SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&sitter_alsa.hwparams)<0)||
    (snd_pcm_hw_params_any(sitter_alsa.alsa,sitter_alsa.hwparams)<0)||
    (snd_pcm_hw_params_set_access(sitter_alsa.alsa,sitter_alsa.hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(sitter_alsa.alsa,sitter_alsa.hwparams,format)<0)||
    (snd_pcm_hw_params_set_rate_near(sitter_alsa.alsa,sitter_alsa.hwparams,&sitter_alsa.rate,0)<0)||
    (snd_pcm_hw_params_set_channels_near(sitter_alsa.alsa,sitter_alsa.hwparams,&sitter_alsa.chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size(sitter_alsa.alsa,sitter_alsa.hwparams,SITTER_ALSA_BUFFER_SIZE)<0)||
    (snd_pcm_hw_params(sitter_alsa.alsa,sitter_alsa.hwparams)<0)
  ) return -1;
  
  fprintf(stderr,"ALSA rate=%d chanc=%d\n",rate,chanc);
  
  if (snd_pcm_nonblock(sitter_alsa.alsa,0)<0) return -1;
  if (snd_pcm_prepare(sitter_alsa.alsa)<0) return -1;

  sitter_alsa.bufc=SITTER_ALSA_BUFFER_SIZE;
  sitter_alsa.bufc_samples=sitter_alsa.bufc*chanc;
  if (!(sitter_alsa.buf=(char*)malloc(sitter_alsa.bufc_samples*samplesize))) return -1;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&sitter_alsa.iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&sitter_alsa.iothd,0,sitter_alsa_iothd,0)) return -1;
  
  return 0;
}

/* Lock.
 */

int sitter_alsa_lock() {
  if (pthread_mutex_lock(&sitter_alsa.iomtx)) return -1;
  return 0;
}

int sitter_alsa_unlock() {
  if (pthread_mutex_unlock(&sitter_alsa.iomtx)) return -1;
  return 0;
}
