#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <random>
#include <vector>
#include <cstdlib>

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_audio.h>  
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_primitives.h>

#include "MFonts.h"
#include "BitObject.h"
#include "mappy_A5.h"


#define DISPLAY_WIDTH 1920 //This is also used instead of al_get_display_width to save on processing time (I know these savings are minuscule, but whatever)
#define DISPLAY_HEIGHT 1080
#define FRAMERATE 60
#define GAME_VELOCITY 5

#define TOTAL_CHIMELINGS 200

#define MAP_BORDER_TOP 720
#define MAP_BORDER_BOTTOM 4460
#define MAP_BORDER_LEFT 460
#define MAP_BORDER_RIGHT 4360
#define MAP_SAFTEY_MARGIN 100

#define HUD_EDGE_OFFSET 10
#define TEXT_EDGE_OFFSET 100

#define MAX_TURNS 10000
#define SCORE_SCALING_FACTOR 1000

#define MINIMUM_DISTANCE 300.0


using namespace std;


bool checkCollision(BitObject first, BitObject second);
void checkCloseAndMove(BitObject person, BitObject chimeling);
void newSampleInstanceThread(ALLEGRO_SAMPLE_INSTANCE* sample);
void displayWelcomePage(const MFonts* fonts, ALLEGRO_DISPLAY* display);
void displayInstructionsPage(const MFonts* fonts, ALLEGRO_DISPLAY* display);
void displayResultPage(const MFonts* fonts, ALLEGRO_DISPLAY* display, int score, int collected);


int main()
{
	srand(time(NULL));
	int returnCode = 0;

	//___________________________________________________________________________________________________________________________________________________________________
	//initialize allegro 5
	MFonts fonts("orbitron-light.ttf");

	ALLEGRO_DISPLAY* display;
	ALLEGRO_EVENT_QUEUE* queue;
	ALLEGRO_TIMER* timer;
	
	ALLEGRO_SAMPLE* sampleMainTheme = NULL;
	ALLEGRO_SAMPLE* sampleSuccess = NULL;
	ALLEGRO_SAMPLE* sampleFailure = NULL;

	ALLEGRO_THREAD* threads[10];

	al_init();
	al_init_font_addon();
	al_init_ttf_addon();
	al_install_keyboard();
	al_install_mouse();
	al_install_audio();
	al_init_acodec_addon();
	al_init_primitives_addon();
	al_init_image_addon();

	display = al_create_display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
	queue = al_create_event_queue();
	timer = al_create_timer(1.0 / FRAMERATE); // 60 fps timer

	if (MapLoad((char*)"My_Mappy_map.FMP", 1)) return -2;

	al_reserve_samples(3);
	sampleMainTheme = al_load_sample("Main Theme.wav");
	sampleSuccess = al_load_sample("Success.wav");
	sampleFailure = al_load_sample("Failure.wav");

	BitObject person(al_load_bitmap("Person.png"), 13, 23, 0, 0);
	BitObject chimeling(al_load_bitmap("Chimeling.png"), 13, 27, 0, 0);
	vector<BitObject>* chimelings = new vector<BitObject>;
	for (int i = 0; i < TOTAL_CHIMELINGS; i++)
	{
		chimelings->emplace_back(BitObject(al_clone_bitmap(chimeling.bitmap), chimeling.width, chimeling.height, 
			rand() % (MAP_BORDER_RIGHT - MAP_BORDER_LEFT - MAP_SAFTEY_MARGIN) - MAP_BORDER_LEFT,
			rand() % (MAP_BORDER_BOTTOM - MAP_BORDER_TOP - MAP_SAFTEY_MARGIN) - MAP_BORDER_TOP + MAP_SAFTEY_MARGIN));
	}

	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_mouse_event_source());
	al_register_event_source(queue, al_get_timer_event_source(timer));

	al_start_timer(timer);

	//___________________________________________________________________________________________________________________________________________________________________
	//game update loop variable declarations

	//update loop boolean
	bool running = true;

	//audio
	bool soundsMute = false;
	bool mainThemePlaying = false;
	bool collisionHappened = false; //updated if there is a collision so that the appropriate sound can be played in the audio managment section

	//keyboard
	bool pressed_keys[ALLEGRO_KEY_MAX];
	for (int i = 0; i < ALLEGRO_KEY_MAX; i++) pressed_keys[i] = false; //set all key presses to false by default

	//pages
	enum pages { Welcome, Instructions, Game, Result };
	pages page = Welcome;

	//turn tracking
	int turnsPassed = 0;
	bool turnPassed = false; //Only move the ships and asteroids if the player moved the ship. This bool tracks if the player moved the ship.

	//displayable info
	int score = 0;
	int chimelingsCollected = 0;

	//person
	person.x = (DISPLAY_WIDTH / 2) - (person.width / 2);
	person.y = (DISPLAY_HEIGHT / 2) - (person.height / 2);

	//map offsets
	int xOff = 2000;
	int yOff = 2000;


	//___________________________________________________________________________________________________________________________________________________________________
	//game update loop
	while (running)
	{
		//---------------------------------------------------------------------------------------------------------------------------------------------------------------
		//Audio management

		if (!soundsMute)
		{
			if (!mainThemePlaying)
			{
				al_play_sample(sampleMainTheme, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_LOOP, NULL);
				mainThemePlaying = true;
			}
			if (collisionHappened)
			{
				al_run_detached_thread((void* (*)(void*))newSampleInstanceThread, (void*)al_create_sample_instance(sampleSuccess));
				collisionHappened = false;
			}
		}
		else
		{
			al_stop_samples();
			mainThemePlaying = false;
		}


		//---------------------------------------------------------------------------------------------------------------------------------------------------------------
		//Event handling
		ALLEGRO_EVENT event;
		bool waiting = true;
		while (waiting) //Make update loop wait for an event
		{
			bool render = false;
			al_wait_for_event(queue, &event);
			if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) /*Terminate Program*/
			{
				running = false;
				waiting = false;
				returnCode -= 1; // changing the return code like this instead of setting it to a new value allows the program to return with multiple return codes.
				//yes, I know logging would be 'better', but this isnt a very complex program, and there aren't many return codes.
			}
			else if (event.type == ALLEGRO_EVENT_KEY_DOWN)
			{
				switch (event.keyboard.keycode)
				{
				case ALLEGRO_KEY_LEFT:
					pressed_keys[ALLEGRO_KEY_LEFT] = true;
					break;
				case ALLEGRO_KEY_RIGHT:
					pressed_keys[ALLEGRO_KEY_RIGHT] = true;
					break;
				case ALLEGRO_KEY_UP:
					pressed_keys[ALLEGRO_KEY_UP] = true;
					break;
				case ALLEGRO_KEY_DOWN:
					pressed_keys[ALLEGRO_KEY_DOWN] = true;
					break;
				}
			}
			else if (event.type == ALLEGRO_EVENT_KEY_UP)
			{
				switch (event.keyboard.keycode)
				{
				case ALLEGRO_KEY_LEFT:
					pressed_keys[ALLEGRO_KEY_LEFT] = false;
					break;
				case ALLEGRO_KEY_RIGHT:
					pressed_keys[ALLEGRO_KEY_RIGHT] = false;
					break;
				case ALLEGRO_KEY_UP:
					pressed_keys[ALLEGRO_KEY_UP] = false;
					break;
				case ALLEGRO_KEY_DOWN:
					pressed_keys[ALLEGRO_KEY_DOWN] = false;
					break;
				}
			}
			else if (event.type == ALLEGRO_EVENT_TIMER)
			{
				if (page == Game)
				{
					turnsPassed += pressed_keys[ALLEGRO_KEY_LEFT] + pressed_keys[ALLEGRO_KEY_RIGHT] + pressed_keys[ALLEGRO_KEY_UP] + pressed_keys[ALLEGRO_KEY_DOWN];

					xOff -= pressed_keys[ALLEGRO_KEY_LEFT] * GAME_VELOCITY;
					xOff += pressed_keys[ALLEGRO_KEY_RIGHT] * GAME_VELOCITY;
					yOff -= pressed_keys[ALLEGRO_KEY_UP] * GAME_VELOCITY;
					yOff += pressed_keys[ALLEGRO_KEY_DOWN] * GAME_VELOCITY;

					for (int i = 0; i < chimelings->size(); i++)
					{
						chimelings->at(i).x += pressed_keys[ALLEGRO_KEY_LEFT] * GAME_VELOCITY;
						chimelings->at(i).x -= pressed_keys[ALLEGRO_KEY_RIGHT] * GAME_VELOCITY;
						chimelings->at(i).y += pressed_keys[ALLEGRO_KEY_UP] * GAME_VELOCITY;
						chimelings->at(i).y -= pressed_keys[ALLEGRO_KEY_DOWN] * GAME_VELOCITY;
						if (checkCollision(person, chimelings->at(i)))
						{
							collisionHappened = true;
							al_destroy_bitmap(chimelings->at(i).bitmap);
							chimelings->erase(chimelings->begin() + i);
							score += (MAX_TURNS - turnsPassed) / SCORE_SCALING_FACTOR;
							chimelingsCollected++;
							continue;
						}
						//Have chimelings move randomly
						chimelings->at(i).x += rand() % 3 - 1;
						chimelings->at(i).y += rand() % 3 - 1;

						//Have chimelings avoid person
						int* tx = new int;
						int* ty = new int;
						*tx = person.x + (person.width / 2) - chimelings->at(i).x + (chimelings->at(i).width / 2);
						*ty = person.y + (person.height / 2) - chimelings->at(i).y + (chimelings->at(i).height / 2);
						if (abs(*tx) <= MINIMUM_DISTANCE && abs(*ty) <= MINIMUM_DISTANCE)
						{
							if (abs(*tx) <= MINIMUM_DISTANCE)
								chimelings->at(i).x -= ((*tx / MINIMUM_DISTANCE < 0) ? floor(*tx / MINIMUM_DISTANCE) : ceil(*tx / MINIMUM_DISTANCE)) * (GAME_VELOCITY / 5);

							if (abs(*ty) <= MINIMUM_DISTANCE)
								chimelings->at(i).y -= ((*ty / MINIMUM_DISTANCE < 0) ? floor(*ty / MINIMUM_DISTANCE) : ceil(*ty / MINIMUM_DISTANCE)) * (GAME_VELOCITY / 5);
						}
						delete tx;
						delete ty;
					}

					MapDrawBG(xOff, yOff, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
					//cout << xOff << " " << yOff << endl;

					for (int i = 0; i < chimelings->size(); i++)
					{
						al_draw_bitmap(chimelings->at(i).bitmap,
							chimelings->at(i).x,
							chimelings->at(i).y,
							0);
					}

					al_draw_bitmap(person.bitmap, person.x, person.y, 0);
					al_draw_text(fonts.heading, al_map_rgb(255, 255, 0), HUD_EDGE_OFFSET, HUD_EDGE_OFFSET, 0, ((string)("Score: " + to_string(score))).c_str());
					al_draw_text(fonts.heading, al_map_rgb(255, 255, 0), HUD_EDGE_OFFSET, HUD_EDGE_OFFSET + 50, 0, ((string)("Chimelings Collected: " + to_string(chimelingsCollected))).c_str());
					al_draw_text(fonts.heading, al_map_rgb(255, 255, 0), HUD_EDGE_OFFSET, HUD_EDGE_OFFSET + 100, 0, ((string)("Steps Remaining: " + to_string(MAX_TURNS - turnsPassed))).c_str());

					if (turnsPassed >= MAX_TURNS || chimelingsCollected >= TOTAL_CHIMELINGS) page = Result;
				}
				else if (page == Welcome)
				{
					displayWelcomePage(&fonts, display);
				}
				else if (page == Instructions)
				{
					displayInstructionsPage(&fonts, display);
				}
				else if (page == Result)
				{
					displayResultPage(&fonts, display, score, chimelingsCollected);
				}

				al_flip_display();
				waiting = false;
			}
		}


		//---------------------------------------------------------------------------------------------------------------------------------------------------------------
		//Keyboard management (Using active polling)
		//Active polling is used for detection of when a specific key is released instead of when it is pressed. Much more useful for menus.

		ALLEGRO_KEYBOARD_STATE keyState;
		al_get_keyboard_state(&keyState);

		//Key ESCAPE
		if (al_key_down(&keyState, ALLEGRO_KEY_ESCAPE)) pressed_keys[ALLEGRO_KEY_ESCAPE] = true;
		else if (pressed_keys[ALLEGRO_KEY_ESCAPE] == true) /*Terminate Program*/
		{
			running = false;
			waiting = false;
			returnCode -= 10;
		}

		//Key LCTRL
		if (al_key_down(&keyState, ALLEGRO_KEY_LCTRL))
		{
			//Key LCTRL + h
			if (al_key_down(&keyState, ALLEGRO_KEY_H)) pressed_keys[ALLEGRO_KEY_H] = true;
			else if (pressed_keys[ALLEGRO_KEY_H] == true) { pressed_keys[ALLEGRO_KEY_H] = false; page = Instructions; } /*open help menu*/

			//Key LCTRL + m
			if (al_key_down(&keyState, ALLEGRO_KEY_M)) pressed_keys[ALLEGRO_KEY_M] = true;
			else if (pressed_keys[ALLEGRO_KEY_M] == true)
			{
				pressed_keys[ALLEGRO_KEY_M] = false;
				if (soundsMute) //unmute
				{
					soundsMute = false;
				}
				else if (!soundsMute) //mute
				{
					soundsMute = true;
				}
			} /*mute sound*/

			turnPassed = false;
		}

		//Key ENTER
		if (al_key_down(&keyState, ALLEGRO_KEY_ENTER)) pressed_keys[ALLEGRO_KEY_ENTER] = true;
		else if (pressed_keys[ALLEGRO_KEY_ENTER] == true) //proceed upon release so as to not skip through all the menus
		{
			pressed_keys[ALLEGRO_KEY_ENTER] = false;
			if (page == Welcome) page = Instructions;
			else if (page == Instructions) page = Game;

			turnPassed = false;
		}

	} //end of game update loop


	//___________________________________________________________________________________________________________________________________________________________________
	//destructors and cleanup
	
	MapFreeMem();

	al_destroy_display(display);
	al_uninstall_keyboard();
	al_uninstall_mouse();
	al_destroy_timer(timer);

	//Audio
	al_destroy_sample(sampleSuccess);
	al_destroy_sample(sampleFailure);
	al_destroy_sample(sampleMainTheme);

	al_uninstall_audio();

	//Bitmaps and BitObjects

	//end of program
	return returnCode;
}


bool checkCollision(BitObject first, BitObject second)
{
	if (
		first.x + first.width > second.x && //left hitbox check
		first.x < second.x + second.width && //right hitbox check
		first.y + first.height > second.y && //left hitbox check
		first.y < second.y + second.height //right hitbox check
		)
		return true;
	else
		return false;
}

void checkCloseAndMove(BitObject person, BitObject chimeling)
{
	int tx = person.x + (person.width / 2) - chimeling.x + (chimeling.width / 2);
	int ty = person.y + (person.height / 2) - chimeling.y + (chimeling.height / 2);
	if (abs(tx) <= 100)
	{
		chimeling.x += ceil(tx / 100.0) * (GAME_VELOCITY/5);
	}
	if (abs(ty) <= 100)
	{
		chimeling.y += ceil(ty / 100.0) * (GAME_VELOCITY / 5);
	}
}

void displayWelcomePage(const MFonts* fonts, ALLEGRO_DISPLAY* display)
{
	al_clear_to_color(al_map_rgb(255, 255, 255));
	al_draw_filled_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, al_map_rgba_f(0, 255, 0, 0.6));

	al_draw_text(fonts->title, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 30, ALLEGRO_ALIGN_CENTRE, "Chimeland!");

	al_draw_multiline_text(fonts->subHeading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), DISPLAY_WIDTH / 2, 150, DISPLAY_WIDTH - TEXT_EDGE_OFFSET, 50, ALLEGRO_ALIGN_CENTRE,
		"You are having a dream. In this dream, you are on a magical island known as Chimeland. The island is full of magical creatures called chimelings. You do not \
		know why, but you feel that you must catch them all as fast as you can.");

	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 900, ALLEGRO_ALIGN_CENTRE, "Press ENTER to continue");
}

void displayInstructionsPage(const MFonts* fonts, ALLEGRO_DISPLAY* display)
{
	al_clear_to_color(al_map_rgb(255, 255, 255));
	al_draw_filled_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, al_map_rgba_f(0, 255, 0, 0.6));

	al_draw_text(fonts->title, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 30, ALLEGRO_ALIGN_CENTRE, "Instructions");

	//how to play
	al_draw_multiline_text(fonts->body, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), DISPLAY_WIDTH / 2, 150, DISPLAY_WIDTH - TEXT_EDGE_OFFSET, 30, ALLEGRO_ALIGN_CENTRE,
		"Explore the island and catch as many chimelings as you can before you run out of steps. When you run out of steps, you wake up.");

	//controls
	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), TEXT_EDGE_OFFSET, 300, ALLEGRO_ALIGN_LEFT, "Controls:");
	al_draw_multiline_text(fonts->body, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), TEXT_EDGE_OFFSET, 370, DISPLAY_WIDTH - TEXT_EDGE_OFFSET, 35, ALLEGRO_ALIGN_LEFT,
		"Left Arrow: Move left\n\
		Right Arrow: Move right\n\
		Up Arrow: Move up\n\
		Down Arrow: Move down\n\
		LCTRL + h: Open help menu\n\
		LCTRL + m: Toggle mute\n\
		ESC : Close game");

	//what they can expect at the end of the game
	al_draw_multiline_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), DISPLAY_WIDTH / 2, 700, DISPLAY_WIDTH - TEXT_EDGE_OFFSET, 50, ALLEGRO_ALIGN_CENTRE,
		"Catch the chimelings in as few steps as possible. Your score is dependent on remaining steps!");

	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 900, ALLEGRO_ALIGN_CENTRE, "Press ENTER to continue");
}

void displayResultPage(const MFonts* fonts, ALLEGRO_DISPLAY* display, int score, int collected)
{
	al_clear_to_color(al_map_rgb(255, 255, 255));
	al_draw_filled_rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, al_map_rgba_f(0, 255, 0, 0.6));

	//end message
	al_draw_multiline_text(fonts->body, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), DISPLAY_WIDTH / 2, 100, DISPLAY_WIDTH - TEXT_EDGE_OFFSET, 30, ALLEGRO_ALIGN_CENTRE,
		"You did your best. Probably. Now that you think about it, you aren't sure. Remembering dreams just after waking up is always difficult.");

	//final score
	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 500, ALLEGRO_ALIGN_CENTRE, "Final Score:");
	al_draw_text(fonts->subHeading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 570, ALLEGRO_ALIGN_CENTRE, (to_string(score)).c_str());

	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 650, ALLEGRO_ALIGN_CENTRE, "Chimelings Collected:");
	al_draw_text(fonts->subHeading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 720, ALLEGRO_ALIGN_CENTRE, (to_string(collected)).c_str());

	al_draw_text(fonts->heading, al_map_rgba_f(0.0, 0.0, 0.0, 1.0), (DISPLAY_WIDTH / 2), 900, ALLEGRO_ALIGN_CENTRE, "Press ESC to exit game");
}

void newSampleInstanceThread(ALLEGRO_SAMPLE_INSTANCE* sampleInstance)
{
	al_attach_sample_instance_to_mixer(sampleInstance, al_get_default_mixer());
	al_play_sample_instance(sampleInstance);
	while(al_get_sample_instance_playing(sampleInstance))
	{ }
	al_destroy_sample_instance(sampleInstance);
}