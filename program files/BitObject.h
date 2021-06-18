#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>


class BitObject
{
public:
	BitObject();
	BitObject(ALLEGRO_BITMAP * pBitmap, int pwidth, int pheight, int px, int py);
	~BitObject();

	ALLEGRO_BITMAP* bitmap;

	int x;
	int y;
	int width;
	int height;
};