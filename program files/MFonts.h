#include <allegro5/allegro.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_font.h>


class MFonts
{
public:
	MFonts(char const* filename);
	~MFonts();

	ALLEGRO_FONT* title;
	ALLEGRO_FONT* heading;
	ALLEGRO_FONT* subHeading;
	ALLEGRO_FONT* body;
};