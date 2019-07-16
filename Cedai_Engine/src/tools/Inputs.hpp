#pragma once
// the different player inputs (set by the Interface class)

enum CD_INPUTS {
	ESC			= (1 << 0),
	FORWARD		= (1 << 1),
	BACKWARD	= (1 << 2),
	LEFT		= (1 << 3),
	RIGHT		= (1 << 4),
	UP			= (1 << 5),
	DOWN		= (1 << 6),
	ROTATEL		= (1 << 7),
	ROTATER		= (1 << 8),
	MOUSEL		= (1 << 9),
	MOUSER		= (1 << 10),
	INTERACTL	= (1 << 11),
	INTERACTR	= (1 << 12)
};