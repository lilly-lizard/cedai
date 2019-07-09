#pragma once
// the different player inputs (set by the Interface class)

enum CD_INPUTS {
	QUIT		= 0x0001,
	FORWARD		= 0x0002,
	BACKWARD	= 0x0004,
	LEFT		= 0x0008,
	RIGHT		= 0x0010,
	UP			= 0x0020,
	DOWN		= 0x0040,
	ROTATEL		= 0x0080,
	ROTATER		= 0x0100,
	MOUSEL		= 0x0200,
	MOUSER		= 0x0400,
	INTERACTL	= 0x0800,
	INTERACTR	= 0x1000,
	ESC			= 0x2000
};