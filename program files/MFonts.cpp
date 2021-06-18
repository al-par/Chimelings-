#include "MFonts.h"


MFonts::MFonts(char const* filename)
{
	al_init();
	al_init_font_addon();
	al_init_ttf_addon();

	title = al_load_ttf_font(filename, 80, 0);
	heading = al_load_ttf_font(filename, 48, 0);
	subHeading = al_load_ttf_font(filename, 36, 0);
	body = al_load_ttf_font(filename, 24, 0);
}

MFonts::~MFonts()
{
	al_destroy_font(title);
	al_destroy_font(body);
	al_destroy_font(heading);
}
