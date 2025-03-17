#include "vcsLib.h"

void init2600();

const int SCREEN_HEIGHT = 192;
const int MAX_SPRITES = 4;

enum NumberSizeEnum
{
	OneCopy,
	TwoCopiesClose,
	TwoCopiesMedium,
	ThreeCopiesClose,
	TwoCopiesWide,
	DoubleSize,
	ThreeCopiesMedium,
	QuadSize
};

struct Sprite
{
	int Height;
	int PositionX;
	int PositionY;
	int FramesSkipped; // Keep track of how many times the sprite was skipped
	NumberSizeEnum NumberSize;
	const uint8_t *pGraphics;
	const uint8_t *pColors;
};

struct SpriteMove
{
	Sprite *pSprite;
	uint8_t NumSiz;
	uint8_t HmoveFirst;
	uint8_t HmoveSecond;
	uint8_t RespRegister;
	uint8_t RespOffset;
};

class MultiSpriteKernel
{
public:
	uint8_t BackgroundColors[SCREEN_HEIGHT];
	uint8_t PlayfieldColors[SCREEN_HEIGHT];
	uint8_t PlayfieldGraphics[SCREEN_HEIGHT * 5]; // 40 bits per line
	Sprite *Sprites[MAX_SPRITES];

	void Render();

private:
	uint8_t grp0Buffer[SCREEN_HEIGHT];
	uint8_t colup0Buffer[SCREEN_HEIGHT]; // D0 is used to flag Sprite Boundary, D7: 0-start, 1-end, D6..D1: SpriteMove index
	uint8_t grp1Buffer[SCREEN_HEIGHT];
	uint8_t colup1Buffer[SCREEN_HEIGHT];
	SpriteMove Moves[MAX_SPRITES];
};

void gameLoop();

extern "C" int elf_main(uint32_t *args)
{
	init2600();
	gameLoop();
	return 0;
}

const uint8_t spriteGraphics[10] = {0xff, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff};
const uint8_t spriteColors[10] = {0x88, 0x86, 0x84, 0x82, 0x54, 0x54, 0x82, 0x84, 0x86, 0x88};

void gameLoop()
{
	MultiSpriteKernel kernel;

	// Seed with some random data
	for (int i = 0; i < SCREEN_HEIGHT; i++)
	{
		kernel.BackgroundColors[i] = i & 0xf0;
		kernel.PlayfieldColors[i] = (256 - i) | 0x8;
		for (int j = 0; j < 5; j++)
		{
			kernel.PlayfieldGraphics[i * 5 + j] = (j & 1) ? i : ~i;
		}
	}

	// Make some sprites
	Sprite s0;
	s0.Height = 10;
	s0.NumberSize = NumberSizeEnum::ThreeCopiesClose;
	s0.pColors = spriteColors;
	s0.pGraphics = spriteGraphics;
	s0.PositionX = 0;
	s0.PositionY = 20;

	Sprite s1;
	s1.Height = 10;
	s1.NumberSize = NumberSizeEnum::DoubleSize;
	s1.pColors = spriteColors;
	s1.pGraphics = spriteGraphics;
	s1.PositionX = 125;
	s1.PositionY = 33;

	Sprite s2;
	s2.Height = 10;
	s2.NumberSize = NumberSizeEnum::QuadSize;
	s2.pColors = spriteColors;
	s2.pGraphics = spriteGraphics;
	s2.PositionX = 130;
	s2.PositionY = 46;

	Sprite s3;
	s3.Height = 10;
	s3.NumberSize = NumberSizeEnum::TwoCopiesWide;
	s3.pColors = spriteColors;
	s3.pGraphics = spriteGraphics;
	s3.PositionX = 140;
	s3.PositionY = 59;

	kernel.Sprites[0] = &s0;
	kernel.Sprites[1] = &s1;
	kernel.Sprites[2] = &s2;
	kernel.Sprites[3] = &s3;

	while (1)
	{
		kernel.Render();

		// PF and Background colors and graphics can be updated as desired. I.E.
		kernel.BackgroundColors[0] = 0x55;
		kernel.PlayfieldColors[0] = 0x55;
		kernel.PlayfieldGraphics[0] = 0x55;

		// Move the sprites around
		s0.PositionX++;
		if (s0.PositionX > 159)
		{
			s0.PositionX = 0;
		}
		s1.PositionX--;
		if (s1.PositionX < 0)
		{
			s1.PositionX = 159;
		}
		s2.PositionX+=2;
		if (s2.PositionX > 159)
		{
			s2.PositionX = 0;
		}
		s3.PositionX-=2;
		if (s3.PositionX < 0)
		{
			s3.PositionX = 159;
		}
	}
}

void init2600()
{
	// Always reset PC, cause it's going to at the end of the 6507 address space
	vcsJmp3();

	// Init TIA and RIOT RAM
	vcsLda2(0);
	for (int i = 0; i < 256; i++)
	{
		vcsSta3((unsigned char)i);
	}
	vcsCopyOverblankToRiotRam();

	// transfer control to TIA RAM so the 6507 can keep itself busy during program initialization
	vcsStartOverblank();
}

void MultiSpriteKernel::Render()
{
	// Prepare sprite buffers
	for (int i = 0; i < SCREEN_HEIGHT; i++)
	{
		grp0Buffer[i] = 0;
		grp1Buffer[i] = 0;
		colup0Buffer[i] = 0;
		colup1Buffer[i] = 0;
	}

	// Calculate Sprite positioning
	for (int i = 0; i < MAX_SPRITES; i++)
	{
		// TODO use both players to limit flickering
		// TODO handle sprites on top line
		// TODO handle sprite overlapping: exactly partial top aligned, partial bottom aligned, partial top, partial bottom
		// TODO handle positions outside 0-159
		Sprite *sprite = Sprites[i];
		int index = sprite->PositionY - 2;

		// 1	140-9
		// 2	10-39
		// 3	40-69
		// 4	64-93
		// 5	94-123
		// 6	115-144
		// Positioning
		colup0Buffer[index] = (i << 1) + 1;
		index++;
		colup0Buffer[index] = (i << 1) + 1;
		Moves[i].pSprite = sprite;
		Moves[i].NumSiz = (uint8_t)sprite->NumberSize;
		int x = sprite->PositionX;
		if (x < 10)
		{
			Moves[i].RespOffset = 1;
			Moves[i].HmoveFirst = x == 9 ? 0x80 : 0x90 + ((9 - x) * 0x10);
			Moves[i].HmoveSecond = 0x80;
		}
		else if (x < 40)
		{
			Moves[i].RespOffset = 2;
			if (x > 24)
			{
				Moves[i].HmoveFirst = 0x80 + ((40 - x) * 0x10);
				Moves[i].HmoveSecond = 0x80;
			}
			else
			{
				Moves[i].HmoveFirst = 0x80 + ((25 - x) * 0x10);
				Moves[i].HmoveSecond = 0x70;
			}
		}
		else if (x < 70)
		{
			Moves[i].RespOffset = 3;
			if (x > 54)
			{
				Moves[i].HmoveFirst = 0x80 + ((70 - x) * 0x10);
				Moves[i].HmoveSecond = 0x80;
			}
			else
			{
				Moves[i].HmoveFirst = 0x80 + ((55 - x) * 0x10);
				Moves[i].HmoveSecond = 0x70;
			}
		}
		else if (x < 94)
		{
			Moves[i].RespOffset = 4;
			if (x > 78)
			{
				Moves[i].HmoveFirst = 0x80 + ((94 - x) * 0x10);
				Moves[i].HmoveSecond = 0x80;
			}
			else
			{
				Moves[i].HmoveFirst = 0x80 + ((79 - x) * 0x10);
				Moves[i].HmoveSecond = 0x70;
			}
		}
		else if (x < 124)
		{
			Moves[i].RespOffset = 5;
			if (x > 108)
			{
				Moves[i].HmoveFirst = 0x80 + ((124 - x) * 0x10);
				Moves[i].HmoveSecond = 0x80;
			}
			else
			{
				Moves[i].HmoveFirst = 0x80 + ((109 - x) * 0x10);
				Moves[i].HmoveSecond = 0x70;
			}
		}
		else if (x < 145)
		{
			Moves[i].RespOffset = 6;
			if (x > 129)
			{
				Moves[i].HmoveFirst = 0x80 + ((145 - x) * 0x10);
				Moves[i].HmoveSecond = 0x80;
			}
			else
			{
				Moves[i].HmoveFirst = 0x80 + ((130 - x) * 0x10);
				Moves[i].HmoveSecond = 0x70;
			}
		}
		else
		{
			Moves[i].RespOffset = 1;
			Moves[i].HmoveFirst = 0x80 + ((160 - x) * 0x10);
			Moves[i].HmoveSecond = 0x30;
		}
		Moves[i].RespRegister = RESP0;
		index++;
		for (int y = 0; y < sprite->Height && index < SCREEN_HEIGHT; y++)
		{
			// Graphics
			grp0Buffer[index] = sprite->pGraphics[y];
			// Colors
			colup0Buffer[index] = sprite->pColors[y] & 0xfe;
			index++;
		}
		if (index < SCREEN_HEIGHT)
		{
			grp0Buffer[index] = 0;
			// Mark the end of the sprite
			colup0Buffer[index] = 0x81;
		}
	}

	// Transfer control back to ARM
	// TIMING MATTERS FROM HERE ON OUT
	vcsEndOverblank();

	vcsWrite5(VDELP1, 1); // Enable VDEL for P1 so GRP1 can be written any time prior to GRP0

	// Partial line to align loop
	vcsSta3(WSYNC);
	vcsSta3(RESP0);
	vcsWrite5(HMP0, 0x80);
	vcsWrite5(COLUBK, BackgroundColors[0]);
	vcsWrite5(COLUPF, PlayfieldColors[0]);
	vcsWrite5(GRP1, grp1Buffer[0]);
	vcsWrite5(PF0, ReverseByte[PlayfieldGraphics[0] >> 4]);
	vcsWrite5(HMP1, 0x80);
	vcsSta3(RESPONE);
	vcsLda2(grp0Buffer[0]);
	vcsNop2n(19);
	SpriteMove &move = Moves[0];
	int moveOffset = -1;
	for (int line = 0;;)
	{
		//
		//                      [-COLORS---------------------------------------------]
		//                      [-0a--]         [-2a-------]    [-1b-------]
		//                            [-1a------]          [-0b-]          [-2b------]
		//][B][P][0][CL0][CL1][1a-][2a-][0b-][j]nn[1b-]nn[-N3-][2b-][GR1][0a-][][H][][
		//
		if (line == 0)
		{
			vcsLdx2(0);
			vcsStx4(VBLANK);
		}
		else
		{
			vcsStx3(COLUBK);
			vcsSty3(COLUPF);
		}
		if ((colup0Buffer[line] & 0x81) == 1)
		{
			// Positioning sprite
			if (moveOffset < 0)
			{
				move = Moves[(colup0Buffer[line] >> 1) & 0x3f];
				moveOffset = move.RespOffset;
				vcsWrite5(NUSIZ0, move.NumSiz);
			}
			else
			{
				vcsJmp3();
				moveOffset = 0;
				vcsWrite5(COLUP0, colup0Buffer[line + 1]);
			}
		}
		else
		{
			vcsSta3(GRP0);
			if (moveOffset >= 0)
			{
				moveOffset = -1;
				vcsWrite5(move.RespRegister + 0x10, 0x80);
			}
			else
			{
				vcsWrite5(COLUP0, colup0Buffer[line]);
			}
		}
		vcsWrite5(COLUP1, 0xcc);
		vcsWrite5(PF1, (PlayfieldGraphics[line * 5] << 4) | (PlayfieldGraphics[line * 5 + 1] >> 4));
		if (moveOffset == 1)
		{
			vcsSta3(move.RespRegister);
		}
		vcsWrite5(PF2, ReverseByte[(uint8_t)((PlayfieldGraphics[line * 5 + 1] << 4) | (PlayfieldGraphics[line * 5 + 2] >> 4))]);
		vcsWrite5(PF0, ReverseByte[PlayfieldGraphics[line * 5 + 2]]);
		if (moveOffset == 2)
		{
			vcsSta3(move.RespRegister);
		}
		vcsJmp3();
		vcsNop2();
		vcsWrite5(PF1, PlayfieldGraphics[line * 5 + 3]);
		if (moveOffset == 3)
		{
			vcsSta3(move.RespRegister);
		}
		vcsNop2();
		if (moveOffset > 0)
		{
			vcsLda2(move.HmoveFirst);
		}
		else if (moveOffset == 0)
		{
			vcsLda2(move.HmoveSecond);
		}
		else
		{
			vcsNop2();
		}
		if (moveOffset >= 0)
		{
			vcsSta4(move.RespRegister + 0x10);
		}
		else
		{
			vcsNop2n(2);
		}
		if (moveOffset == 4)
		{
			vcsSta3(move.RespRegister);
		}
		vcsWrite5(PF2, ReverseByte[PlayfieldGraphics[line * 5 + 4]]);
		// Increment line here because we start loading for next line before current line ends
		line++;
		if (line == SCREEN_HEIGHT)
		{
			break;
		}
		vcsWrite5(GRP1, line);
		if (moveOffset == 5)
		{
			vcsSta3(move.RespRegister);
		}
		vcsWrite5(PF0, ReverseByte[PlayfieldGraphics[line * 5] >> 4]);
		vcsLda2(grp0Buffer[line]);
		if (moveOffset == 6)
		{
			vcsSta3(move.RespRegister);
		}
		vcsSta3(HMOVE);
		vcsLdx2(BackgroundColors[line]);
		vcsLdy2(PlayfieldColors[line]);
	}
	vcsSta3(WSYNC);
	vcsWrite5(VBLANK, 2);

	vcsStartOverblank();
}