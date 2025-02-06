#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "errm.h"

#include <SDL2/SDL.h>

struct ViewPort {
  int width, height;
  SDL_Window *screen;
  SDL_Renderer *ren;
};

void printinfo(FILE *stream, int num, const SDL_RendererInfo *nfo) {
  fprintf(stream, "\n---\n");
  fprintf(stream, "Number: %d\n", num);
  fprintf(stream, "Name: %s\n", nfo->name);
  fprintf(stream, "SW: %x, ACC: %x, VSYNC: %x, TEXTURE: %x\n",
          nfo->flags & SDL_RENDERER_SOFTWARE,
          nfo->flags & SDL_RENDERER_ACCELERATED,
          nfo->flags & SDL_RENDERER_PRESENTVSYNC,
          nfo->flags & SDL_RENDERER_TARGETTEXTURE);
}

void driversinfo(FILE *stream) {
  int ndrivers = SDL_GetNumRenderDrivers();
  if (ndrivers < 0)
    ERR(SDL_GetNumRenderDrivers);

  for (int i = 0; i < ndrivers; ++i) {
    int res;
    SDL_RendererInfo nfo;
    res = SDL_GetRenderDriverInfo(i, &nfo);
    if (res < 0)
      ERR(SDL_GetRenderDriverInfo);
    printinfo(stream, i, &nfo);
  }
}

void renderTriangle(SDL_Renderer *ren) {
  int w, h;
  SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
  SDL_RenderClear(ren);

  SDL_GetRendererOutputSize(ren, &w, &h);
  SDL_Vertex vertex_1 = {{100, 100}, {0, 0, 0xff, 0xff}};
  SDL_Vertex vertex_2 = {{w - 100, 100}, {0xff, 0xff, 0xff, 0xff}};
  SDL_Vertex vertex_3 = {{100, h - 100}, {0xff, 0, 0, 0xff}};

  SDL_Vertex vertices[] = {vertex_1, vertex_2, vertex_3};

  SDL_RenderGeometry(ren, NULL, vertices, 3, NULL, 0);
}

int main() {
  struct ViewPort v;
  SDL_RendererInfo nfo;
  int res;
  FILE *stream = fopen("device.log", "w");

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    ERR(SDL_Init, true);

  v.width = 640;
  v.height = 480;

  if (atexit(SDL_Quit))
		return EXIT_FAILURE;

  v.screen = SDL_CreateWindow("First SDL triangle", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, v.width, v.height,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!v.screen)
    ERR(SDL_CreateWindow, true);

  driversinfo(stream);

  v.ren = SDL_CreateRenderer(
      v.screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!v.ren)
    ERR(SDL_CreateRenderer, true);

  fprintf(stream, "\n[Selected]\n");
  res = SDL_GetRendererInfo(v.ren, &nfo);
  if (res != 0)
    ERR(SDL_GetRendererInfo);
  else
    printinfo(stream, -1, &nfo);

  for (;;) {
    int pending;
    SDL_Event event;
    pending = SDL_PollEvent(&event);
    if (pending && event.type == SDL_QUIT)
      break;
    renderTriangle(v.ren);
    SDL_RenderPresent(v.ren);
  }

  fclose(stream);
  SDL_DestroyRenderer(v.ren);
  SDL_DestroyWindow(v.screen);
  exit(0);
}
