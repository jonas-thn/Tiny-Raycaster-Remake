#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <string>
#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PI 3.14159265358979323846

SDL_Window* window = NULL;
const size_t win_w = 1024;
const size_t win_h = 512;

SDL_Renderer* renderer = NULL;
bool running = false;

std::vector<uint32_t> framebuffer(win_w* win_h, 0xFFFFFFFF);
SDL_Texture* screen_texture = NULL;

bool init_window()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		fprintf(stderr, "Error initializing SDL\n");
		return false;
	}

	window = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, NULL);

	if (!window)
	{
		fprintf(stderr, "Error creating window\n");
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, NULL);

	if (!renderer)
	{
		fprintf(stderr, "Error creating renderer\n");
		return false;
	}

	screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, win_w, win_h);

	return true;
}

void input()
{
	SDL_Event event;
	SDL_PollEvent(&event);

	switch (event.type)
	{
	case SDL_QUIT:
		running = false;
		break;
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
		{
			running = false;
		}
		break;
	}
}

void render()
{
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);

	SDL_UpdateTexture(screen_texture, NULL, &framebuffer[0], (int)(win_w * sizeof(uint32_t)));
	SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	for (size_t i = 0; i < win_w * win_h; i++)
	{
		framebuffer[i] = 0xFFFFFFFF;
	}
}

void destroy()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

uint32_t pack_color(const uint8_t r, const uint8_t g, const uint8_t b, const uint32_t a = 255)
{
	return (a << 24) + (b << 16) + (g << 8) + r;
}

void unpack_color(const uint32_t& color, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
{
	r = (color >> 0) & 255;
	g = (color >> 8) & 255;
	b = (color >> 16) & 255;
	a = (color >> 24) & 255;
}

void drop_ppm_image(const std::string filename, const std::vector<uint32_t>& image, const size_t w, const size_t h)
{
	assert(image.size() == w * h);
	
	std::ofstream ofs(filename, std::ios::binary);
	ofs << "P6\n" << w << " " << h << "\n255\n";

	for (size_t i = 0; i < w*h; i++)
	{
		uint8_t r, g, b, a;
		unpack_color(image[i], r, g, b, a);
		ofs << static_cast<char>(r) << static_cast<char>(g) << static_cast<char>(b);
	}
	ofs.close();
}

bool load_texture(const std::string filename, std::vector<uint32_t>& texture, size_t& tex_size, size_t& tex_count)
{
	int n_channels = -1, w, h;

	unsigned char* pixmap = stbi_load(filename.c_str(), &w, &h, &n_channels, 0);

	if (!pixmap)
	{
		fprintf(stderr, "Error loading textures\n");
		return false;
	}

	if (4 != n_channels)
	{
		fprintf(stderr, "TExture doesnt have 4 channels\n");
		stbi_image_free(pixmap);
		return false;
	}

	tex_count = w / h;
	tex_size = w / tex_count;

	if (w != (h * (int)tex_count))
	{
		fprintf(stderr, "Texture parts are not square\n");
		stbi_image_free(pixmap);
		return false;
	}

	texture = std::vector<uint32_t>(w * h);

	for (int j = 0; j < h; j++)
	{
		for (int i = 0; i < w; i++)
		{
			uint8_t r = pixmap[(i + j * w) * 4 + 0];
			uint8_t g = pixmap[(i + j * w) * 4 + 1];
			uint8_t b = pixmap[(i + j * w) * 4 + 2];
			uint8_t a = pixmap[(i + j * w) * 4 + 3];

			texture[i + j * w] = pack_color(r, g, b, a);
		}
	}

	stbi_image_free(pixmap);

	return true;
}

void draw_rectangle(std::vector<uint32_t>& img, const size_t img_w, const size_t img_h, const size_t x, const size_t y, const size_t w, const size_t h, const uint32_t color)
{
	assert(img.size() == img_w * img_h);
	for (size_t i = 0; i < w; i++)
	{
		for (size_t j = 0; j < h; j++)
		{
			size_t cx = x + i;
			size_t cy = y + j;

			if (cx >= img_w || cy >= img_h)
			{
				continue;
			}
			img[cx + cy * img_w] = color;
		}
	}
}

int main(int argc, char* argv[])
{
	running = init_window();

	const size_t map_w = 16;
	const size_t map_h = 16;

	const char map[] = 
		"0000222222220000"\
		"1              0"\
		"1      11111   0"\
		"1     0        0"\
		"0     0  1110000"\
		"0     3        0"\
		"0   10000      0"\
		"0   0   11100  0"\
		"0   0   0      0"\
		"0   0   1  00000"\
		"0       1      0"\
		"2       1      0"\
		"0       0      0"\
		"0 0000000      0"\
		"0              0"\
		"0002222222200000";

	assert(sizeof(map) == map_w * map_h + 1); //+1 for \0

	float player_x = 3.456;
	float player_y = 2.345;
	float player_a = 1.523;
	const float fov = PI / 3.0;

	const size_t nColors = 10;
	std::vector<uint32_t> colors(nColors);
	for (size_t i = 0; i < nColors; i++)
	{
		colors[i] = pack_color(rand() % 255, rand() % 255, rand() % 255);
	}

	std::vector<uint32_t> walltex;
	size_t walltex_size;
	size_t walltex_count;

	if (!load_texture("./walltext.png", walltex, walltex_size, walltex_count))
	{
		fprintf(stderr, "Failed to load wall textures\n");
		return -1;
	}

	const size_t rect_w = win_w / (map_w * 2);
	const size_t rect_h = win_h / map_h;

	float last_frame = SDL_GetTicks();

	while(running)
	{
		float delta_time = (SDL_GetTicks() - last_frame) / 1000.0;

		last_frame = SDL_GetTicks();

		input();

		player_a += delta_time;

		for (size_t j = 0; j < map_h; j++)
		{
			for (size_t i = 0; i < map_w; i++)
			{
				if (map[i + j * map_w] == ' ')
				{
					continue;
				}

				size_t rect_x = i * rect_w;
				size_t rect_y = j * rect_h;

				size_t iColor = map[i + j * map_w] - '0';
				assert(iColor < nColors);

				draw_rectangle(framebuffer, win_w, win_h, rect_x, rect_y, rect_w, rect_h, colors[iColor]);
			}
		}

		for (size_t i = 0; i < win_w / 2; i++)
		{
			float angle = player_a - fov / 2 + fov * i / float(win_w / 2);

			for (float t = 0; t < 20; t += 0.01)
			{
				float cx = player_x + t * cos(angle);
				float cy = player_y + t * sin(angle);

				size_t pix_x = cx * rect_w;
				size_t pix_y = cy * rect_h;

				framebuffer[pix_x + pix_y * win_w] = pack_color(160, 160, 160);

				if (map[int(cx) + int(cy) * map_w] != ' ')
				{
					size_t iColor = map[int(cx) + int(cy) * map_w] - '0';
					assert(iColor < nColors);

					size_t column_height = win_h / (t * cos(angle - player_a));
					draw_rectangle(framebuffer, win_w, win_h, win_w / 2 + i, win_h / 2 - column_height / 2, 1, column_height, colors[iColor]);
					break;
				}
			}
		}

		const size_t tex_id = 4;

		for (size_t i = 0; i < walltex_size; i++)
		{
			for (size_t j = 0; j < walltex_size; j++)
			{
				framebuffer[i + j * win_w] = walltex[i + tex_id * walltex_size + j * walltex_count * walltex_size];
			}
		}

		render();
	}

	//drop_ppm_image("./out.ppm", framebuffer, win_w, win_h);

	destroy();

	return 0;
}