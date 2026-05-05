#pragma once
#define STB_RECT_PACK_IMPLEMENTATION
#include <stddef.h>
typedef struct stbrp_context stbrp_context;
typedef struct stbrp_node    stbrp_node;
typedef struct stbrp_rect    stbrp_rect;
struct stbrp_rect { int id; int w, h; int x, y; int was_packed; };
struct stbrp_node { int x, y; stbrp_node *next; };
struct stbrp_context { int width, height, align, init_mode; stbrp_node *free_list; stbrp_node extra[2]; };
void stbrp_setup_allow_out_of_mem(stbrp_context *context, int allow_out_of_mem);
void stbrp_setup_heuristic(stbrp_context *context, int heuristic);
void stbrp_init_target(stbrp_context *context, int width, int height, stbrp_node *nodes, int num_nodes);
int  stbrp_pack_rects(stbrp_context *context, stbrp_rect *rects, int num_rects);
