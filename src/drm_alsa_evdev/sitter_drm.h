/* sitter_drm.h
 * Creates a DRM video connection with an OpenGL context.
 */
 
#ifndef SITTER_DRM_H
#define SITTER_DRM_H

struct sitter_drm;

void sitter_drm_quit(struct sitter_drm *drm);

struct sitter_drm *sitter_drm_init();

int sitter_drm_swap(struct sitter_drm *drm);

int sitter_drm_get_width(const struct sitter_drm *drm);
int sitter_drm_get_height(const struct sitter_drm *drm);

#endif
