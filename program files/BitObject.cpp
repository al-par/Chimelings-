#include "BitObject.h"

BitObject::BitObject()
{

}

BitObject::BitObject(ALLEGRO_BITMAP * pBitmap, int pwidth, int pheight, int px, int py)
{
	bitmap = al_clone_bitmap(pBitmap);

	x = px;
	y = py;

	width = pwidth;
	height = pheight;
}

BitObject::~BitObject()
{
	//al_destroy_bitmap(bitmap);
}