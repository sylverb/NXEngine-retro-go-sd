
#ifndef _PROFILE_H
#define _PROFILE_H

// how many bytes of data long a profile.dat is.
// used by the replays which use the regular profile functions
// to write a savefile then tack their own data onto the end.
#define PROFILE_LENGTH		0x604

struct Profile
{
	int stage;
	int songno;
	int px, py, pdir;
	int hp, maxhp, num_whimstars;
	uint32_t equipmask;
	
	int curWeapon;
	struct
	{
		bool hasWeapon;
		int level;
		int xp;
		int ammo, maxammo;
	} weapons[WPN_COUNT];
	
	int inventory[MAX_INVENTORY];
	int ninventory;
	
	bool flags[NUM_GAMEFLAGS];
	
	struct
	{
		int slotno;
		int scriptno;
	} teleslots[NUM_TELEPORTER_SLOTS];
	int num_teleslots;
};

/*
 * Save-select UI only. Omits flags[NUM_GAMEFLAGS] (~8 KiB) and teleporter
 * slots so 5 previews fit in a few KiB of BSS instead of a 43 KiB heap alloc.
 */
struct ProfilePreview
{
	int stage;
	int hp, maxhp;
	uint32_t equipmask;
	int curWeapon;
	struct
	{
		bool hasWeapon;
		int level;
		int xp;
	} weapons[WPN_COUNT];
	int inventory[MAX_INVENTORY];
	int ninventory;
};

bool profile_load_preview(const char *pfname, ProfilePreview *file);

#endif
