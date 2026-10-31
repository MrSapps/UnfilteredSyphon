#include "common.h"
#include "level.h"
#include "system.h"
#include "gobj.h"
#include "weapon.h"

/*
 * baseext.ovl holds the level code for BASEEXT, BASEEXT2 and BUNKER (iBaseextInit,
 * iBaseext2Init, iBunkerInit), plus the alert light and spotlight dispatchers.
 */

/* Object types specific to this overlay */
#define BASEEXT_TYPE_CHARGE 0x58    /* explosive charge placement point */
#define BASEEXT_TYPE_FUEL_TANK 0x67 /* fuel tank the charge is planted on */
#define BASEEXT_TYPE_MISSILE 0x64   /* missile to catalog */

/*
 * The bss items sit 4 bytes apart, as aspsx 2.56's .lcomm rule lays them out
 * (docs/data_migration.md).  aspsx 2.77 and maspsx pack them by size instead, so
 * both builds get explicit padding.
 */
#define BSS_PAD3(n) static u8 n; static s16 n##_2;

/* Mission briefing shown while the level loads (init.ovl reads it through gBriefings). */
typedef struct Briefing {
    /* 0x00 */ char *lines[3];
    /* 0x0C */ s32 count;
    /* 0x10 */ char **objectives;
} Briefing;

/* Fog scrim mesh handed to EGSDisplayRegisterFogScrim (an 11x11 vertex grid). */
typedef struct BaseextFogScrim {
    /* 0x00 */ s16 (*verts)[4];
    /* 0x04 */ u8 (*colors)[4];
    /* 0x08 */ u8 (*faces)[4];
    /* 0x0C */ s16 numFaces;
    /* 0x0E */ s16 numVerts;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 r, g, b, pad;
} BaseextFogScrim;

static const s32 D_80140FE4[5] = {0, 1, 1, 1, 0}; /* also the empty objective format */

static char *D_80144A04[2] = {
    "AGENCY DIRECTIVE:\n\nYou're objectives on this mission are threefold:\nplant C4 charges at key "
    "locations for delayed detonation, catalog Rhoemer's missile arsenal for further assessment, and "
    "disable the base's anti-aircraft capability to allow for pick up on the comm. building roof. \n",
    "Additionally, be on the lookout for base CO Gabrek.  You must eliminate him.  Bear in mind, the "
    "explosives you're carrying have short fuses to prevent early discovery.  Once they're primed, you "
    "won't have much time to escape.",
};
Briefing baseextBriefings = {
    {"Rozovka, Kazakhstan", "Rhoemer's Base", "09:01 21:00\n"}, 2, D_80144A04,
};
static char *D_80144A20[2] = {
    "INCOMING FROM LIAN:\n\nGabe, all the explosive charges you set are primed and ready for detonation.  "
    "All you have to do now is catalog the SS-23s and get to the roof.  As you check each one, I'll be "
    "transmiting the firing codes to HQ.  So keep your channel open.\n",
    "The base is definitely on alert now, so you're not going to be able to get back to the surface the "
    "way you came.  Satellite photos indicate that there's a frieght elevator on the roof, and it's "
    "likely connected to the bunker.  That's your exit strategy.",
};
Briefing bunkerBriefings = {
    {"Rozovka, Kazakhstan", "Rhoemer's Base", "09:01 21:15\n"}, 2, D_80144A20,
};
static char *D_80144A3C[2] = {
    "INCOMING FROM RHOEMER:\n\nSurprise, Logan. I'm sure you wouldn't have thought enough of me to hack "
    "Agency communication codes, but here we are.  I'm sorry about Lian. It was unfortunate that she "
    "had to die...\n",
    "By the way, I've triggered your explosive charges.  Yes, I've known about your plan all along,  "
    "but it was a fool's errand.  I was already through with that base... Syphon Filter is the wave of "
    "the future.  You'd better start running, hero.  The clock is ticking.",
};
Briefing baseext2Briefings = {
    {"Rozovka, Kazakhstan", "Rhoemer's Base", "09:01 21:35\n"}, 2, D_80144A3C,
};
static char *D_80144A58[7] = {
    (char *)D_80140FE4, /* replaced by the charges text (D_801452D8) */
    "Eliminate Gabrek and collect keycard",
    "Disable power to motion sensors",
    "Reach missile bunker",
    (char *)D_80140FE4, /* replaced by the missiles text (D_80145318) */
    "Reach comm. building roof",
    "Escape through front gate",
};
static char *D_80144A74[3] = {
    "Do not damage the explosive charges",
    "Do not damage the missiles",
    "Get out of base before timer runs out on explosives",
};
static char *D_80144A80 = "Plant C4 charges at %d%s fuel tank%s";
static char *D_80144A84 = "Catalog %d%s enemy missile%s";
static char *D_80144A88 = " more";
static char *D_80144A8C = "Gabrek";
static char *D_80144A90 = "Place explosive here";
static char *D_80144A94 = "%d more explosive%s to plant";
static char *D_80144A98 = "Motion Sensor Power";
static char *D_80144A9C = "Missile Access Control";
static char *D_80144AA0 = "Missile Access Initiated";
static char *D_80144AA4 = "OTR-23 Oka Missile";
static char *D_80144AA8 = "Missile indexed.\n%d remaining";
/* texture animation tables */
static s32 D_80144AAC[1] = {0x004002F0};
static s32 D_80144AB0[6] = {0x005002F0, 0x006002F0, 0x007002F0, 0x008002F0, 0x009002F0, 0x009002F8};
static LevelObjectives D_80144AC8 = {7, D_80144A58, 3, D_80144A74}; /* objectives/timers save info */
static u8 D_80144AF0 = 0; /* alert light already bright for one check */
static s16 D_80144AF4[121][4] = {
    {-34, -4095, 60, 0}, {489, -3809, -1423, 0}, {422, -2940, -2820, 0}, {-124, -1620, -3760, 0},
    {-867, -51, -4003, 0}, {-1452, 1529, -3513, 0}, {-1567, 2874, -2464, 0}, {-1055, 3776, -1187, 0},
    {34, 4095, -60, 0}, {-78, -3805, -1512, 0}, {-638, -2931, -2788, 0}, {-1478, -1605, -3467, 0},
    {-2250, -31, -3423, 0}, {-2609, 1549, -2753, 0}, {-2331, 2891, -1728, 0}, {-1389, 3786, -720, 0},
    {-639, -3799, -1390, 0}, {-1615, -2917, -2379, 0}, {-2635, -1584, -2707, 0}, {-3331, -6, -2384, 0},
    {-3416, 1573, -1627, 0}, {-2781, 2909, -769, 0}, {-1533, 3795, -165, 0}, {-1118, -3790, -1076, 0},
    {-2379, -2900, -1644, 0}, {-3442, -1561, -1581, 0}, {-3966, 19, -1027, 0}, {-3763, 1595, -285, 0},
    {-2854, 2925, 289, 0}, {-1467, 3803, 405, 0}, {-1452, -3780, -609, 0}, {-2829, -2882, -684, 0},
    {-3789, -1538, -239, 0}, {-4069, 42, 469, 0}, {-3603, 1614, 1090, 0}, {-2541, 2937, 1302, 0},
    {-1200, 3808, 912, 0}, {-1596, -3771, -54, 0}, {-2902, -2866, 373, 0}, {-3630, -1519, 1136, 0},
    {-3627, 59, 1902, 0}, {-2960, 1626, 2316, 0}, {-1884, 2944, 2134, 0}, {-769, 3810, 1291, 0},
    {-1530, -3763, 516, 0}, {-2589, -2854, 1387, 0}, {-2986, -1507, 2362, 0}, {-2699, 68, 3079, 0},
    {-1918, 1631, 3230, 0}, {-974, 2944, 2674, 0}, {-230, 3808, 1488, 0}, {-1262, -3758, 1023, 0},
    {-1933, -2847, 2219, 0}, {-1944, -1502, 3276, 0}, {-1410, 68, 3844, 0}, {-617, 1627, 3706, 0},
    {73, 2938, 2851, 0}, {343, 3803, 1478, 0}, {-832, -3756, 1402, 0}, {-1022, -2847, 2759, 0},
    {-643, -1506, 3752, 0}, {69, 60, 4094, 0}, {768, 1615, 3682, 0}, {1111, 2926, 2640, 0},
    {876, 3796, 1263, 0}, {-293, -3758, 1598, 0}, {25, -2853, 2936, 0}, {742, -1518, 3728, 0},
    {1538, 43, 3795, 0}, {2052, 1596, 3163, 0}, {2004, 2910, 2069, 0}, {1294, 3786, 871, 0},
    {280, -3763, 1589, 0}, {1063, -2865, 2725, 0}, {2026, -1537, 3209, 0}, {2801, 21, 2987, 0},
    {3062, 1574, 2215, 0}, {2632, 2892, 1215, 0}, {1543, 3777, 354, 0}, {813, -3770, 1374, 0},
    {1956, -2881, 2153, 0}, {3036, -1559, 2261, 0}, {3688, -5, 1779, 0}, {3664, 1551, 969, 0},
    {2910, 2875, 192, 0}, {1590, 3768, -217, 0}, {1231, -3780, 982, 0}, {2583, -2898, 1300, 0},
    {3638, -1582, 1015, 0}, {4082, -29, 333, 0}, {3777, 1530, -411, 0}, {2800, 2860, -863, 0},
    {1427, 3761, -767, 0}, {1480, -3789, 465, 0}, {2862, -2916, 277, 0}, {3751, -1603, -365, 0},
    {3928, -49, -1159, 0}, {3384, 1514, -1740, 0}, {2319, 2851, -1807, 0}, {1077, 3758, -1222, 0},
    {1527, -3798, -106, 0}, {2752, -2930, -778, 0}, {3358, -1620, -1694, 0}, {3247, -63, -2495, 0},
    {2540, 1505, -2838, 0}, {1532, 2847, -2515, 0}, {588, 3757, -1521, 0}, {1364, -3805, -656, 0},
    {2271, -2940, -1722, 0}, {2514, -1628, -2792, 0}, {2132, -68, -3496, 0}, {1357, 1504, -3559, 0},
    {541, 2850, -2893, 0}, {24, 3761, -1623, 0}, {1014, -3808, -1111, 0}, {1484, -2944, -2430, 0},
    {1331, -1629, -3514, 0}, {731, -64, -4030, 0}, {-4, 1512, -3807, 0}, {-519, 2859, -2888, 0},
    {-540, 3767, -1515, 0},
};
static u8 D_80144EBC[121][4] = {
    {0x85, 0x9C, 0x86, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x81, 0x98, 0x82, 0x3A}, {0x63, 0x75, 0x63, 0x3A},
    {0x19, 0x1F, 0x1A, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A}, {0x06, 0x07, 0x06, 0x3A}, {0x05, 0x06, 0x05, 0x3A},
    {0x04, 0x04, 0x04, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x77, 0x8C, 0x78, 0x3A}, {0x5F, 0x70, 0x5F, 0x3A},
    {0x18, 0x1C, 0x19, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A},
    {0x82, 0x99, 0x83, 0x3A}, {0x76, 0x8B, 0x77, 0x3A}, {0x64, 0x75, 0x64, 0x3A}, {0x18, 0x1D, 0x19, 0x3A},
    {0x0A, 0x0C, 0x0A, 0x3A}, {0x06, 0x07, 0x06, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A},
    {0x81, 0x98, 0x82, 0x3A}, {0x65, 0x77, 0x65, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A},
    {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x75, 0x8A, 0x76, 0x3A},
    {0x60, 0x70, 0x60, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A}, {0x06, 0x07, 0x06, 0x3A},
    {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x80, 0x97, 0x81, 0x3A}, {0x5F, 0x6F, 0x5F, 0x3A},
    {0x18, 0x1D, 0x19, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A},
    {0x82, 0x99, 0x83, 0x3A}, {0x78, 0x8D, 0x79, 0x3A}, {0x5D, 0x6E, 0x5D, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A},
    {0x0B, 0x0D, 0x0B, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A},
    {0x7D, 0x93, 0x7E, 0x3A}, {0x5F, 0x6F, 0x5F, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A},
    {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x7D, 0x93, 0x7E, 0x3A},
    {0x5D, 0x6D, 0x5D, 0x3A}, {0x18, 0x1D, 0x19, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A}, {0x06, 0x07, 0x06, 0x3A},
    {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x7F, 0x96, 0x80, 0x3A}, {0x66, 0x78, 0x66, 0x3A},
    {0x18, 0x1D, 0x19, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A},
    {0x82, 0x99, 0x83, 0x3A}, {0x7B, 0x91, 0x7C, 0x3A}, {0x5F, 0x6F, 0x5F, 0x3A}, {0x18, 0x1D, 0x19, 0x3A},
    {0x0B, 0x0D, 0x0B, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A},
    {0x80, 0x96, 0x81, 0x3A}, {0x65, 0x76, 0x65, 0x3A}, {0x18, 0x1C, 0x19, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A},
    {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x78, 0x8D, 0x79, 0x3A},
    {0x62, 0x73, 0x62, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A}, {0x0A, 0x0C, 0x0A, 0x3A}, {0x06, 0x07, 0x06, 0x3A},
    {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x7F, 0x96, 0x80, 0x3A}, {0x64, 0x76, 0x64, 0x3A},
    {0x1A, 0x1F, 0x1B, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A}, {0x06, 0x07, 0x06, 0x3A}, {0x05, 0x06, 0x05, 0x3A},
    {0x82, 0x99, 0x83, 0x3A}, {0x7D, 0x93, 0x7E, 0x3A}, {0x63, 0x74, 0x63, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A},
    {0x0A, 0x0C, 0x0A, 0x3A}, {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A},
    {0x7A, 0x8F, 0x7B, 0x3A}, {0x5E, 0x6F, 0x5E, 0x3A}, {0x18, 0x1D, 0x19, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A},
    {0x07, 0x08, 0x07, 0x3A}, {0x05, 0x06, 0x05, 0x3A}, {0x82, 0x99, 0x83, 0x3A}, {0x78, 0x8D, 0x79, 0x3A},
    {0x63, 0x74, 0x63, 0x3A}, {0x19, 0x1E, 0x1A, 0x3A}, {0x0B, 0x0D, 0x0B, 0x3A}, {0x06, 0x07, 0x06, 0x3A},
    {0x05, 0x06, 0x05, 0x3A},
};
static u8 D_801450A0[136][4] = {
    {0, 0, 1, 9}, {1, 9, 2, 10}, {2, 10, 3, 11}, {3, 11, 4, 12},
    {4, 12, 5, 13}, {5, 13, 6, 14}, {6, 14, 7, 15}, {7, 15, 8, 8},
    {0, 0, 9, 16}, {9, 16, 10, 17}, {10, 17, 11, 18}, {11, 18, 12, 19},
    {12, 19, 13, 20}, {13, 20, 14, 21}, {14, 21, 15, 22}, {15, 22, 8, 8},
    {0, 0, 16, 23}, {16, 23, 17, 24}, {17, 24, 18, 25}, {18, 25, 19, 26},
    {19, 26, 20, 27}, {20, 27, 21, 28}, {21, 28, 22, 29}, {22, 29, 8, 8},
    {0, 0, 23, 30}, {23, 30, 24, 31}, {24, 31, 25, 32}, {25, 32, 26, 33},
    {26, 33, 27, 34}, {27, 34, 28, 35}, {28, 35, 29, 36}, {29, 36, 8, 8},
    {0, 0, 30, 37}, {30, 37, 31, 38}, {31, 38, 32, 39}, {32, 39, 33, 40},
    {33, 40, 34, 41}, {34, 41, 35, 42}, {35, 42, 36, 43}, {36, 43, 8, 8},
    {0, 0, 37, 44}, {37, 44, 38, 45}, {38, 45, 39, 46}, {39, 46, 40, 47},
    {40, 47, 41, 48}, {41, 48, 42, 49}, {42, 49, 43, 50}, {43, 50, 8, 8},
    {0, 0, 44, 51}, {44, 51, 45, 52}, {45, 52, 46, 53}, {46, 53, 47, 54},
    {47, 54, 48, 55}, {48, 55, 49, 56}, {49, 56, 50, 57}, {50, 57, 8, 8},
    {0, 0, 51, 58}, {51, 58, 52, 59}, {52, 59, 53, 60}, {53, 60, 54, 61},
    {54, 61, 55, 62}, {55, 62, 56, 63}, {56, 63, 57, 64}, {57, 64, 8, 8},
    {0, 0, 58, 65}, {58, 65, 59, 66}, {59, 66, 60, 67}, {60, 67, 61, 68},
    {61, 68, 62, 69}, {62, 69, 63, 70}, {63, 70, 64, 71}, {64, 71, 8, 8},
    {0, 0, 65, 72}, {65, 72, 66, 73}, {66, 73, 67, 74}, {67, 74, 68, 75},
    {68, 75, 69, 76}, {69, 76, 70, 77}, {70, 77, 71, 78}, {71, 78, 8, 8},
    {0, 0, 72, 79}, {72, 79, 73, 80}, {73, 80, 74, 81}, {74, 81, 75, 82},
    {75, 82, 76, 83}, {76, 83, 77, 84}, {77, 84, 78, 85}, {78, 85, 8, 8},
    {0, 0, 79, 86}, {79, 86, 80, 87}, {80, 87, 81, 88}, {81, 88, 82, 89},
    {82, 89, 83, 90}, {83, 90, 84, 91}, {84, 91, 85, 92}, {85, 92, 8, 8},
    {0, 0, 86, 93}, {86, 93, 87, 94}, {87, 94, 88, 95}, {88, 95, 89, 96},
    {89, 96, 90, 97}, {90, 97, 91, 98}, {91, 98, 92, 99}, {92, 99, 8, 8},
    {0, 0, 93, 100}, {93, 100, 94, 101}, {94, 101, 95, 102}, {95, 102, 96, 103},
    {96, 103, 97, 104}, {97, 104, 98, 105}, {98, 105, 99, 106}, {99, 106, 8, 8},
    {0, 0, 100, 107}, {100, 107, 101, 108}, {101, 108, 102, 109}, {102, 109, 103, 110},
    {103, 110, 104, 111}, {104, 111, 105, 112}, {105, 112, 106, 113}, {106, 113, 8, 8},
    {0, 0, 107, 114}, {107, 114, 108, 115}, {108, 115, 109, 116}, {109, 116, 110, 117},
    {110, 117, 111, 118}, {111, 118, 112, 119}, {112, 119, 113, 120}, {113, 120, 8, 8},
    {0, 0, 114, 1}, {114, 1, 115, 2}, {115, 2, 116, 3}, {116, 3, 117, 4},
    {117, 4, 118, 5}, {118, 5, 119, 6}, {119, 6, 120, 7}, {120, 7, 8, 8},
};
static BaseextFogScrim D_801452C0 = {D_80144AF4, D_80144EBC, D_801450A0, 136, 121, 0, 0x2F, 0x4D, 0x70, 0};

static char D_801452D8[0x40];   /* objective text buffer (charges) */
static char D_80145318[0x40];   /* objective text buffer (missiles) */
static u8 D_80145358[0xD8];     /* light targeting profile */
static u32 D_80145430;          /* alert state: 0 none, 1 alerted, else tick the alert starts */
static s32 D_80145434;          /* bunker exit object, opened once the missiles are cataloged */
static s32 D_80145438;          /* missiles total */
static s32 D_8014543C;          /* missiles indexed */
static s32 D_80145440;          /* charges total */
static s32 D_80145444;          /* charges planted */
static u8 D_80145448; BSS_PAD3(D_80145448_pad) /* snowing */
static u8 D_8014544C; BSS_PAD3(D_8014544C_pad) /* hit-the-tank warning played */
static u8 D_80145450; BSS_PAD3(D_80145450_pad) /* hit-a-missile warning played */
static u8 D_80145454; BSS_PAD3(D_80145454_pad) /* level music may start right away */

/* Written as separate tests so cc1 doesn't merge the bit checks into one mask. */
#define OBJECTIVE_DONE(n) (gObjectives != NULL && (gObjectives->completed & (1 << (n))))

/* main.exe */
void SysExeTickQueueTimer(void (*cb)(s32), s32 ticks, s32 arg);
void SysSoundQueueXA(s32 arg0, s32 track);
void StringContactMeiXingMsg(void);
void DamageKillAllPeople(s32 by, s32 kind);
void ParticleFireWaveDeath(void);
void InterfaceCountdownReset(s32 arg0);
void DamageGOBJ(s32 arg0, s16 id, s16 by, s16 amount, s32 kind);
void SaveInternalTimer(s32 ticks);
void WeaponSpecialUse(s32 weapon);
GameObjectRuntime *iObjectFindNearbyLight(GameObjectRuntime *rt, s32 type);
void ObjectUpdateAmbientObjectSetting(s16 id, s32 arg1);
void ParticleStartSnow(void);
void ParticleStopSnow(void);
void ParticleInitBreath(void);
void EGSDisplayRegisterFogScrim(void *scrim);
void ObjectSetAmbientSndIdxCallback(s32 (*cb)());
void InterfaceStartCountdownAfterGameCam(s32 seconds, s32 arg1, void (*done)());
void LevelSetDirectionPointerFlag(u8 flag);
void JoystickDeactivate(void);

typedef struct LightColor {
    /* 0x00 */ u8 unk00[4];
    /* 0x04 */ s16 r;
    /* 0x06 */ s16 g;
    /* 0x08 */ s16 b;
} LightColor;

/* GameObjectRuntime.anim as seen by this overlay. */
typedef struct BaseextAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 flags08;
    /* 0x09 */ u8 unk09;
    /* 0x0A */ u8 flags0A;
    /* 0x0B */ u8 unk0B;
    /* 0x0C */ EGSFrame *frame;
    /* 0x10 */ GameObjectModel *model;
    /* 0x14 */ u8 unk14[4];
    /* 0x18 */ s32 unk18;
    /* 0x1C */ LightColor *light;
} BaseextAnim;

/* GameObjectRuntime.move as seen by this overlay. */
typedef struct BaseextMove {
    /* 0x000 */ VECTOR32 pos;
    /* 0x010 */ u8 unk010[0xD0];
    /* 0x0E0 */ VECTOR32 unkE0;
    /* 0x0F0 */ u8 unk0F0[0xA4];
    /* 0x194 */ VECTOR32 pendingMove;
} BaseextMove;

/* GameObjectRuntime as seen by the alert light and spotlight dispatchers. */
typedef struct LightRuntime {
    /* 0x00 */ u8 flags0; /* 2: light on, 0x40: active */
    /* 0x01 */ u8 flags1; /* spotlight: low 7 bits ticks left to sweep while switched off */
    /* 0x02 */ s16 id;
    /* 0x04 */ u8 unk04[4];
    /* 0x08 */ BaseextAnim *anim;
    /* 0x0C */ BaseextMove *move;
    /* 0x10 */ void *cas;
    /* 0x14 */ void *target;
    /* 0x18 */ GameObjectHealth *healthInfo;
    /* 0x1C */ void *npc;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 npcState;
    /* 0x23 */ u8 flags23;
} LightRuntime;

/* Low 7 bits of flags1 (a u8 bitfield would get word accesses from cc1). */
#define LIGHT_TIMER(rt) ((rt)->flags1 & 0x7F)
#define LIGHT_SET_TIMER(rt, t) ((rt)->flags1 = ((rt)->flags1 & 0x80) | ((t) & 0x7F))

typedef struct BaseextTexAnim {
    /* 0x00 */ u8 unk00[8];
    /* 0x08 */ u8 unk08;
    /* 0x09 */ u8 unk09;
} BaseextTexAnim;

typedef struct BaseextFlashlight {
    /* 0x00 */ u8 unk00[0x1C];
    /* 0x1C */ s32 unk1C;
} BaseextFlashlight;

typedef struct BaseextCamera {
    /* 0x00 */ u8 unk00[0x8C];
    /* 0x8C */ EGSListNode *objects;
} BaseextCamera;

typedef struct BaseextDisplayObj {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ u16 id; /* low 10 bits object id, 0x2000 = game object */
} BaseextDisplayObj;

s32 PathGetNumPoints(GameObjectRuntime *rt);
u8 PathObjectIsAtPoint(GameObjectRuntime *rt, s32 point);
void PathHandleEx(void *rt, void *scale, s32 b, s32 c, s32 d, s32 e, u8 *done);
void PathSetupEX(LightRuntime *rt, s32 arg1);
void PathGetPoint(LightRuntime *rt, s32 index, VECTOR32 *out);
u8 UseObjectAlreadyInList(s16 id);
void MovementAddGobj(LightRuntime *rt);
void MovementRemoveGobj(LightRuntime *rt);
void MovementGobjInit(LightRuntime *rt, s32 a, s32 b, s32 c, s32 d);
void PhysicsGobjBasicInit(LightRuntime *rt, s32 a, s32 b);
void CollisionGobjBasicInit(LightRuntime *rt, s32 a, s32 b, s32 c, s32 d, s32 e);
void CobjAddBulletCollisionBoxGobj(LightRuntime *rt, s32 a, s32 b, s32 c);
void CobjRemoveBulletCollisionBoxGobj(LightRuntime *rt);
void TargetInitGobj(LightRuntime *rt, void *profile, s32 a, s32 b);
void EGSAnimHideDetailDef(BaseextAnim *anim);
void EGSAnimShowDetailDef(BaseextAnim *anim);
void EGSFrameLookAtPoint(EGSFrame *frame, s32 arg1, VECTOR32 *point, s32 arg3);
BaseextTexAnim *EGSDisplayInitTextureAnimation(void *a, void *b, s32 c, s32 d, s32 e);
void EGSDisplayFlashlightInit(BaseextFlashlight *light);
void EGSDisplayFlashlightParams(BaseextFlashlight *light, s32 a, s32 b, s32 c);
void EGSDisplayFlashlightFrame(BaseextFlashlight *light, EGSFrame *frame, s32 arg2);
void EGSDisplayFlashlightOn(s32 light);
void EGSDisplayFlashlightOff(s32 light);
void func_801463A4(s16 id, s32 arg1); /* LevelCreateGOBJ (init.ovl) */
extern u8 gObjPathScaleVector[];

void iBaseextDetonateExplosives(GameObjectRuntime *rt);
void iBaseextExplosivesDeath(s16 reason);
void BaseextAlert(void);
static void func_80141990(void);
static u8 func_80141C0C(s16 id, s16 by, s16 reason);
static void func_80141FBC(BaseextAnim *anim);
static void func_801420A8(void);
static void func_801420D4(s32 quiet);

/* Rebuild the "N charges remaining" objective text. */
static void func_80141990(void) {
    StringObjectiveObjectsRemaining2(D_801452D8, D_80144A80, D_80144A88, (char *)D_80140FE4, D_80145444, D_80145440);
}

static void func_801419E4(s16 id, s16 by) {
    int type;

    if (by == gPlayerOffset && !D_8014544C && id != GOBJ_ID_666 &&
        ((type = gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type) == BASEEXT_TYPE_FUEL_TANK ||
         type == BASEEXT_TYPE_CHARGE)) {
        D_8014544C = 1;
        SysPlayXA(0x2F, 0, 1);
    }
}

void iBaseextDetonateExplosives(GameObjectRuntime *rt) {
    VECTOR32 pos; /* never filled in */
    GameObjectHealth *health;

    ObjectBigExplosionFX(rt->id, &pos, 0x1000, 5, 10);
    health = rt->healthInfo;
    if (health->health > 0) {
        health->health = -1;
    }
}

void iBaseextExplosivesDeath(s16 reason) {
    if (SysSoundIsXAPlaying()) {
        SysStopXA();
    }
    gDeviceDetonated = 1;
    if (!gMissionFailed) {
        gMissionFailed = 1;
        if (reason != -1) {
            LevelMissionFailed(reason, -1, 0);
        }
    }
    ParticleFireWaveDeath();
    DamageKillAllPeople(gPlayerOffset, 0x11);
}

static void func_80141B90(s16 id, s16 reason) {
    iBaseextDetonateExplosives(gameObjectsInfo[id].runtime);
    iBaseextExplosivesDeath(reason);
    if (gSysInfo.level == 10) {
        InterfaceCountdownReset(0);
    }
}

/* A planted charge or its fuel tank was destroyed: everything goes up. */
static u8 func_80141C0C(s16 id, s16 by, s16 reason) {
    u8 ret;
    s16 type;
    s16 ref;

    ret = 1;
    type = GOBJ_TYPE(id);
    if (type == BASEEXT_TYPE_CHARGE) {
        if (gameObjectsInfo[id].runtime->flags0 & 0x20) {
            ref = ObjectMatchToRefID(id, BASEEXT_TYPE_FUEL_TANK)->id;
            if (id != ref) {
                func_80141B90(ref, reason);
            }
        }
    } else if (type == BASEEXT_TYPE_FUEL_TANK && (ref = gameObjectsInfo[id].refId) != -1) {
        if (GOBJ_TYPE(ref) == BASEEXT_TYPE_CHARGE) {
            func_80141B90(id, reason);
            DamageGOBJ(-1, by, ref, gameObjectsInfo[ref].runtime->healthInfo->health, 0x11);
        }
    } else {
        ret = 0;
    }
    return ret;
}

static void func_80141E14(s16 id, s16 by) {
    int type;

    if (!func_80141C0C(id, by, 0)) {
        type = GOBJ_TYPE(id);
        if (type == 0x21 || type == 0x3D) {
            BaseextAlert();
            return;
        }
        if (D_80145430 == 0 || !(D_80145430 < gSystemTick)) {
            if (gameObjectsInfo[id].runtime->npcState == 2 || type == 0x16) {
                if (D_80145430 == 1) {
                    D_80145430 = gSystemTick + 0x2EE0;
                } else {
                    D_80145430 = D_80145430 - 0x4B0;
                }
                if (D_80145430 > gSystemTick) {
                    SysExeTickQueueTimer(func_801420D4, D_80145430 - gSystemTick, 0);
                } else {
                    func_801420D4(0);
                }
            }
        }
    }
}

/* Flash of the alarm light: once it stays bright the base goes on alert. */
static void func_80141FBC(BaseextAnim *anim) {
    LightColor *light;

    light = anim->light;
    if (light->r + light->g + light->b >= 0xF0B) {
        if (D_80144AF0) {
            D_80145430 = 1;
            SysExeTickQueueTimer(func_801420D4, 0, 0);
            if (SysSoundIsXAPlaying()) {
                SysStopXA();
            }
            SysPlayXA(0x27, 0, 1);
            func_801420D4(0);
        } else {
            EGSDisplayQueueInterrupt((void (*)(void))func_80141FBC, 2, (s32)anim);
            D_80144AF0 = 1;
            return;
        }
    } else {
        EGSDisplayQueueInterrupt((void (*)(void))func_80141FBC, 5, (s32)anim);
    }
    D_80144AF0 = 0;
}

static void func_801420A8(void) {
    SysPlaySong(0, 0x10, 0, 0);
}

/* The base goes on alert. */
static void func_801420D4(s32 quiet) {
    int i;
    int type;

    if (quiet == 0) {
        TextStatusMessage(gLevelAlertedMsg, 0);
    }
    if (D_80145454) {
        func_801420A8();
    } else {
        gSysMovieDoneCallback = func_801420A8;
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        type = GOBJ_TYPE(i);
        if (type == 0x16 || (type == 0x6E && !(gameObjectsInfo[i].runtime->flags0 & 2))) {
            SysPostMessage(0x14, 2, SYS_MSG_LEVEL, i, 0, 0, 0, 0);
        }
    }
    SysExeTickQueueTimer(func_801420D4, 0, 0);
    EGSDisplayQueueInterrupt((void (*)(void))func_80141FBC, 0,
                             (s32)gameObjectsInfo[gPlayerOffset].runtime->anim);
    gLevelAlerted = 1;
    D_80145430 = 1;
    npcSetLevelAlert();
}

void BaseextAlert(void) {
    if (D_80145430 == 0 || !(D_80145430 < gSystemTick)) {
        func_801420D4(0);
    }
}

/* Weapon fired: an unsilenced shot alerts the base. */
static void func_801422DC(s16 id, s16 weapon) {
    if (!((gWeaponInfo[weapon].flags >> 14) & 1) && !gLevelAlerted) {
        if (id != gPlayerOffset) {
            if (SysSoundIsXAPlaying()) {
                SysStopXA();
            }
            SysPlayXA(0x27, 0, 1);
        }
        BaseextAlert();
    }
}

static s32 func_80142378(void) {
    if (!gMissionFailed && OBJECTIVE_DONE(0) && OBJECTIVE_DONE(1) && OBJECTIVE_DONE(2)) {
        return 1;
    }
    if (gMissionFailed) {
        StringMissionFailedMsg();
    } else {
        SysPlayXA(0x64, 0, 1);
        LevelObjectivesIncomplete(gObjectivesIncompleteMsg);
    }
    return 0;
}

/* A charge was planted. */
static void func_80142418(s16 id) {
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == BASEEXT_TYPE_CHARGE) {
        D_80145444++;
        if (D_80145444 < D_80145440) {
            StringInteractObjectsRemaining(D_80144A94, D_80145440 - D_80145444);
            func_80141990();
            if (D_80145444 == 1) {
                SysPlayXA(0x29, 0, 1);
            }
            WeaponSpecialUse(0x18);
            if (!gMissionFailed) {
                SaveInternalTimer(D_80145430);
            }
            if (!gMissionFailed) {
                LevelMissionCtrMsg(gCheckpointMsg, 0);
                SaveCreateContinue();
            }
        } else {
            func_80141990();
            WeaponRemove(0x18);
            SysPlayXA(0x2A, 0, 1);
            LevelObjectiveComplete(0);
        }
    }
}

/* Turn off the motion sensor light next to the power switch. */
static void func_80142578(GameObjectRuntime *rt) {
    GameObjectRuntime *light;

    light = iObjectFindNearbyLight(rt, 0x6B);
    if (light != NULL && light->healthInfo->health > 0 && (light->flags0 & 8)) {
        SysPostMessage(0x14, SYS_MSG_IMMEDIATE, rt->id, light->id, 0, 0, 0, 0);
    }
}

static s32 func_801425FC(s16 id) {
    int type;
    s32 ref;

    type = GOBJ_TYPE(id);
    if (type == 0x1D || type == 0x7C) {
        ref = gameObjectsInfo[id].refId;
        if (ref != -1) {
            type = GOBJ_TYPE(ref);
            if (type == 0x61) {
                func_80142578(gameObjectsInfo[id].runtime);
                SysPlayXA(0x26, 0, 1);
                if (!gMissionFailed) {
                    SaveInternalTimer(D_80145430);
                }
                LevelObjectiveComplete(2);
                if (gMissionFailed) {
                    return 0;
                }
            }
        }
        return 1;
    }
    ref = gameObjectsInfo[id].refId;
    if (ref != -1 && gameObjectsInfo[ref].runtime->npcState == 9) {
        return 1;
    }
    return 3;
}

static s32 func_80142810(s16 id) {
    s32 ref;
    s32 ok;

    ref = gameObjectsInfo[id].refId;
    ok = 1;
    if (ref != -1) {
        ok = (gameObjectsInfo[ref].runtime->flags0 >> 5) & 1;
    }
    return ok;
}

static void func_80142880(s16 id) {
    s32 ref;
    int type;
    s32 setting;

    ref = gameObjectsInfo[id].refId;
    if (ref != -1) {
        type = GOBJ_TYPE(ref);
        if (type == 0x1D) {
            LevelObjectiveAdd(2, id);
        } else if (type == 0x61) {
            if (gObjectives == NULL || !(gObjectives->completed & 4)) {
                if (!gMissionFailed) {
                    LevelMissionFailed(2, 0x25, 1);
                }
                BaseextAlert();
            }
        }
    } else {
        setting = gameObjectsInfo[id].pathStart;
        ObjectUpdateAmbientObjectSetting(id, 0x5F);
        if (setting == 2) {
            if (!D_80145448) {
                D_80145448 = 1;
                ParticleStartSnow();
            }
        } else if (D_80145448) {
            D_80145448 = 0;
            ParticleStopSnow();
        }
    }
}

static s32 func_80142A14(void) {
    return 0x1C;
}

static void *func_80142A1C(void) {
    SysSoundQueueXA(-1, 0x23);
    StringContactMeiXingMsg();
    return gLockCodeRequiredMsg;
}

static void *func_80142A50(s16 id) {
    int type;

    type = GOBJ_TYPE(id);
    if (type == BASEEXT_TYPE_CHARGE) {
        return D_80144A90;
    }
    if (type == 0x1D || type == 0x7C) {
        return D_80144A98;
    }
    return NULL;
}

static void func_80142AEC(void) {
    LevelObjectiveComplete(1);
}

void iBaseextInit(s32 mode) {
    BaseextAnim *anim;
    GameObjectRuntime *rt;
    GameObjectInfo *info;
    int i;
    int type;
    u8 indexed;
    u32 *words;
    s32 bit;
    s32 misalign;
    s32 word;
    u32 mask;

    anim = (BaseextAnim *)gameObjectsInfo[gPlayerOffset].runtime->anim;
    EGSDisplayRegisterFogScrim(&D_801452C0);
    D_80145430 = 0;
    D_80145440 = 0;
    D_80145444 = 0;
    D_8014544C = 0;
    D_80145448 = 1;
    ParticleStartSnow();
    ParticleInitBreath();
    D_80144AC8.primaryTexts[0] = D_801452D8;
    D_80144AC8.primaryTexts[4] = D_80145318;
    SaveObjectivesNTimersReinit(&D_80144AC8, (s32)&D_80145430);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(0, -1);
        LevelObjectiveAdd(1, -1);
        LevelObjectiveAdd(3, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 1;
        }
    }
    D_80145454 = mode == 3;
    DamageSetDamagedCallback(func_801419E4);
    DamageSetDestroyedCallback(func_80141E14);
    LevelSetEOLCallback(func_80142378);
    UseSetLevSpecObjCallback(func_80142418);
    UseSetSwitchCallback(func_801425FC);
    ObjectSetQuipObjectCallback(func_80142810);
    ObjectSetSpecialObjectCallback(func_80142880);
    ObjectSetAmbientSndIdxCallback(func_80142A14);
    SetLockedStringCallback(func_80142A1C);
    SetUseStringCallback(func_80142A50);
    PickupSetKeyCallback(func_80142AEC);
    WeaponSetWeaponFiredCallback(func_801422DC);
    EGSDisplayQueueInterrupt((void (*)(void))func_80141FBC, 0x28, (s32)anim);
    SysStopSong(0, 0x1C);
    SysStopSong(0, 0x10);
    if (D_80145430 != 0) {
        if (D_80145430 != 1) {
            SysExeTickQueueTimer(func_801420D4, D_80145430 - gSystemTick, 0);
        } else {
            func_801420D4((s32)gObjPlayer);
        }
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (!gLevelAlerted && i != GOBJ_ID_666 &&
            gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type == 0x6E) {
            SysPostMessage(0x14, 2, SYS_MSG_LEVEL, i, 0, 0, 0, 0);
        } else if (type == BASEEXT_TYPE_CHARGE) {
            D_80145440++;
            if (rt->flags0 & 0x20) {
                D_80145444++;
                ((BaseextAnim *)rt->anim)->flags08 &= ~8;
            }
        } else if (type == BASEEXT_TYPE_MISSILE) {
            indexed = (*(u32 *)rt & 0x28) == 0x20;
            D_80145438++;
            if (indexed) {
                if (rt->move != NULL && !PathObjectIsAtPoint(rt, PathGetNumPoints(rt) - 1)) {
                    indexed = 0;
                }
                if (indexed) {
                    D_8014543C++;
                }
            }
        } else if (type == 0x5F) {
            if (gameObjectsInfo[i].refId == -1) {
                info = &gameObjectsInfo[(s16)i];
                if (info->flags24 & 0x20) {
                    func_80142880(i);
                }
            }
        }
    }
    if (!OBJECTIVE_DONE(0)) {
        /* bit test tolerant of an unaligned base */
        words = gWeaponsAvailable;
        bit = 0x18;
        misalign = (u32)words & 3;
        if (misalign) {
            bit = misalign * 8 + 0x18;
            words = (u32 *)((u32)words & ~3);
        }
        word = bit >> 5;
        mask = 1 << (bit & 0x1F);
        if (!(words[word] & mask)) {
            WeaponAdd(0x18, D_80145440);
        }
    }
    func_80141990();
    if (gLevelSpecificNpcID != -1 && gLevelSpecificNpcID != GOBJ_ID_666 &&
        gameObjectsModelInfo[gameObjectsInfo[gLevelSpecificNpcID].modelIndex].type == 1) {
        gLevelSpecificNpcName = D_80144A8C;
        gameObjectsInfo[gLevelSpecificNpcID].runtime->npc->nameMsgId = 0xB;
    }
    gDefaultLevelDmgFX = 11;
}

/* BASEEXT2 countdown expired: the first visible armed charge goes off and takes the base with it. */
static void func_80143124(void) {
    EGSListNode *node;
    int id;
    GameObjectRuntime *rt;

    if (!gDeviceDetonated) {
        for (node = ((BaseextCamera *)*gCamera)->objects; node != NULL; node = node->next) {
            id = ((BaseextDisplayObj *)node->data)->id;
            if ((id & 0x2000) && (id &= 0x3FF) != GOBJ_ID_666 &&
                gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == BASEEXT_TYPE_CHARGE) {
                rt = gameObjectsInfo[id].runtime;
                if (rt->healthInfo->health > 0) {
                    iBaseextDetonateExplosives(rt);
                }
                break;
            }
        }
        iBaseextExplosivesDeath(2);
    }
}

static void func_80143228(s16 id, s16 by) {
    func_80141C0C(id, by, 0);
}

void iBaseext2Init(s32 mode) {
    s16 i;
    GameObjectRuntime *rt;

    EGSDisplayRegisterFogScrim(&D_801452C0);
    D_8014544C = 0;
    InterfaceStartCountdownAfterGameCam(0xB4, 0, func_80143124);
    ParticleStartSnow();
    ParticleInitBreath();
    SaveObjectivesNTimersReinit(&D_80144AC8, 0);
    D_80145454 = mode == 3;
    func_801420D4((s32)gObjPlayer);
    LevelObjectiveAdd(6, -1);
    if (gObjectives != NULL) {
        gObjectives->unk24 |= 5;
    }
    DamageSetDamagedCallback(func_801419E4);
    DamageSetDestroyedCallback(func_80143228);
    ObjectSetAmbientSndIdxCallback(func_80142A14);
    LevelSetDirectionPointerFlag(1);
    for (i = 0; i < gLevelGameObjCount; i++) {
        if (i != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[i].modelIndex].type == BASEEXT_TYPE_CHARGE) {
            rt = gameObjectsInfo[i].runtime;
            ((BaseextAnim *)rt->anim)->flags08 &= ~8;
            rt->flags0 |= 0x20;
        }
    }
    gDefaultLevelDmgFX = 11;
}

void BunkerMissileAction(GameObjectRuntime *rt) {
    u8 done;
    s32 ref;
    u8 flags;

    PathHandleEx(rt, 0, 1, 1, -1, -1, &done);
    if (!done) {
        SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
    } else {
        flags = rt->flags0;
        rt->flags0 = flags & ~0x28;
        if ((flags & 0x40) && !UseObjectAlreadyInList(rt->id)) {
            SysPostMessage(0x12, SYS_MSG_IMMEDIATE, rt->id, rt->id, 0, 0, 0, 0);
        }
        ref = gameObjectsInfo[rt->id].refId;
        if (ref != -1) {
            gameObjectsInfo[ref].runtime->flags0 |= 8;
            SysPostMessage(10, 4, rt->id, ref, 0, 0, 0, 0);
        }
    }
}

/* Misnamed in the map: rebuilds the "N missiles remaining" objective text. */
void seq_hans_death(void) {
    StringObjectiveObjectsRemaining2(D_80145318, D_80144A84, D_80144A88, (char *)D_80140FE4, D_8014543C, D_80145438);
}

/* Complete a bunker objective; once the missiles are cataloged the exit opens. */
static void func_801435C0(s16 objective) {
    s32 bit;

    if (gObjectives != NULL) {
        bit = 1 << objective;
        if (!(gObjectives->unk18 & bit)) {
            LevelObjectiveAdd(objective, -1);
        }
        gObjectives->completed |= bit;
    }
    if (!gMissionFailed && OBJECTIVE_DONE(4)) {
        SysPostMessage(0x1B, SYS_MSG_IMMEDIATE, gPlayerOffset, D_80145434, 0, 0, 0, 0);
    }
    LevelObjectiveComplete(objective);
}

static void func_801436AC(s16 id, s16 by) {
    if (by == gPlayerOffset && !D_80145450 && id != GOBJ_ID_666 &&
        gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == BASEEXT_TYPE_MISSILE) {
        D_80145450 = 1;
        SysPlayXA(0x30, 0, 1);
    }
}

/* A missile was destroyed: the warhead takes out the bunker. */
static void func_80143760(s16 id) {
    VECTOR32 pos; /* never filled in */

    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == BASEEXT_TYPE_MISSILE) {
        if (SysSoundIsXAPlaying()) {
            SysStopXA();
        }
        gDeviceDetonated = 1;
        LevelMissionFailed(1, -1, 0);
        ParticleFireWaveDeath();
        ObjectBigExplosionFX(id, &pos, 0x1000, 5, 10);
        DamageKillAllPeople(gPlayerOffset, 0x11);
    }
}

/* A missile was cataloged. */
static void func_8014384C(s16 id) {
    if (id != GOBJ_ID_666 && gameObjectsModelInfo[gameObjectsInfo[id].modelIndex].type == BASEEXT_TYPE_MISSILE) {
        D_8014543C++;
        if (D_8014543C < D_80145438) {
            StringInteractObjectsRemaining(D_80144AA8, D_80145438 - D_8014543C);
            seq_hans_death();
            if (D_8014543C == 1) {
                SysSoundQueueXA(-1, 0x2B);
                GameSysSetLetterBoxMode(1);
                JoystickDeactivate();
                CASRequestAction(gPlayerOffset, 0x5B);
            }
            if (!gMissionFailed) {
                LevelMissionCtrMsg(gCheckpointMsg, 0);
                SaveCreateContinue();
            }
        } else {
            seq_hans_death();
            SysPlayXA(0x2C, 0, 1);
            func_801435C0(4);
        }
    }
}

static s32 func_80143994(s16 id) {
    int type;
    s32 ref;

    type = GOBJ_TYPE(id);
    if (type == 0x1D || type == 0x7C) {
        ref = gameObjectsInfo[id].refId;
        if (ref != -1) {
            type = GOBJ_TYPE(ref);
            if (type == 0x12) {
                func_80142578(gameObjectsInfo[id].runtime);
            } else if (type == BASEEXT_TYPE_MISSILE) {
                gameObjectsInfo[ref].runtime->flags0 |= 8;
                SysPostMessage(10, 4, id, ref, 0, 0, 0, 0);
                TextStatusMessage(D_80144AA0, 0);
                return 0;
            }
        }
        return 1;
    }
    return 3;
}

static void *func_80143B5C(void) {
    SysPlayXA(0x64, 0, 1);
    return gObjectivesIncompleteMsg;
}

static void *func_80143B8C(s16 id) {
    int type;
    s32 ref;

    type = GOBJ_TYPE(id);
    if (type == BASEEXT_TYPE_MISSILE) {
        return D_80144AA4;
    }
    if (type == 0x1D || type == 0x7C) {
        ref = gameObjectsInfo[id].refId;
        if (ref != -1 && ref != GOBJ_ID_666 &&
            gameObjectsModelInfo[gameObjectsInfo[ref].modelIndex].type == BASEEXT_TYPE_MISSILE) {
            return D_80144A9C;
        }
    }
    return NULL;
}

void iBunkerInit(s32 mode) {
    BaseextTexAnim *texAnim;
    GameObjectRuntime *rt;
    int i;
    int type;
    u8 indexed;

    texAnim = EGSDisplayInitTextureAnimation(D_80144AAC, D_80144AB0, 6, 3, 0);
    texAnim->unk09 = 0x10;
    texAnim->unk08 = 0x10;
    D_80145434 = -1;
    D_80145438 = 0;
    D_8014543C = 0;
    D_80145450 = 0;
    D_80144AC8.primaryTexts[4] = D_80145318;
    SaveObjectivesNTimersReinit(&D_80144AC8, (s32)&D_80145430);
    SysStopSong(0, 0x1C);
    D_80145454 = mode == 3;
    func_801420D4((s32)gObjPlayer);
    if (gObjectives->unk18 == 0) {
        LevelObjectiveAdd(4, -1);
        LevelObjectiveAdd(5, -1);
        if (gObjectives != NULL) {
            gObjectives->unk24 |= 2;
        }
    }
    DamageSetDamagedCallback(func_801436AC);
    DamageSetDestroyedCallback(func_80143760);
    UseSetLevSpecObjCallback(func_8014384C);
    UseSetSwitchCallback(func_80143994);
    SetLockedStringCallback(func_80143B5C);
    SetUseStringCallback(func_80143B8C);
    if (D_80145430 != 0) {
        if (D_80145430 != 1) {
            SysExeTickQueueTimer(func_801420D4, D_80145430 - gSystemTick, 0);
        } else {
            func_801420D4((s32)gObjPlayer);
        }
    }
    for (i = 0; i < gLevelGameObjCount; i++) {
        rt = gameObjectsInfo[i].runtime;
        type = GOBJ_TYPE(i);
        if (type == BASEEXT_TYPE_MISSILE) {
            indexed = (*(u32 *)rt & 0x28) == 0x20;
            D_80145438++;
            if (indexed) {
                if (rt->move != NULL && !PathObjectIsAtPoint(rt, PathGetNumPoints(rt) - 1)) {
                    indexed = 0;
                }
                if (indexed) {
                    D_8014543C++;
                }
            }
        } else if (type == 0xC) {
            D_80145434 = i;
        }
    }
    seq_hans_death();
    gDefaultLevelDmgFX = 11;
}

/* Alert light helpers: the light's flashlight handle is kept in its refId. */
static void func_80143FDC(LightRuntime *rt, BaseextFlashlight *light) {
    EGSDisplayFlashlightInit(light);
    EGSDisplayFlashlightParams(light, 0x78, 0xE, 4);
    light->unk1C = 0xFF;
}

/* Switch on: start spinning. */
static void func_80144024(LightRuntime *rt) {
    VECTOR32 spin;

    MovementAddGobj(rt);
    spin.vz = 0;
    spin.vx = 0;
    spin.vy = 0xE38E3;
    rt->move->unkE0 = spin;
    EGSDisplayFlashlightOn(gameObjectsInfo[rt->id].refId);
}

static void func_801440B8(LightRuntime *rt) {
    MovementRemoveGobj(rt);
    EGSDisplayFlashlightOff(gameObjectsInfo[rt->id].refId);
}

void ALERTLIGHTObjDispatch(SysMsg *msg) {
    LightRuntime *rt;
    BaseextFlashlight *light;

    rt = (LightRuntime *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 6:
        rt->flags0 |= 0x40;
        if (rt->healthInfo->health > 0 && (rt->flags0 & 2)) {
            func_80144024(rt);
        }
        CobjAddBulletCollisionBoxGobj(rt, 1, 0, 0);
        break;
    case 5:
        rt->flags0 &= ~0x40;
        if (rt->healthInfo->health > 0 && (rt->flags0 & 2)) {
            func_801440B8(rt);
        }
        CobjRemoveBulletCollisionBoxGobj(rt);
        break;
    case 13:
    case 42:
        if (rt->healthInfo->health <= 0) {
            rt->flags23 |= 2;
            rt->flags0 &= ~2;
            func_801440B8(rt);
        }
        break;
    case 20:
        if (rt->healthInfo->health > 0) {
            rt->flags0 ^= 2;
            if (rt->flags0 & 2) {
                EGSAnimHideDetailDef(rt->anim);
                if (rt->flags0 & 0x40) {
                    func_80144024(rt);
                }
            } else {
                EGSAnimShowDetailDef(rt->anim);
                if (rt->flags0 & 0x40) {
                    func_801440B8(rt);
                }
            }
        }
        break;
    case 2:
        func_801463A4(rt->id, 0);
        rt->npcState = 0xC;
        TargetInitGobj(rt, D_80145358, 0, 0);
        rt->cas = NULL;
        rt->unk21 = 0xA0;
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x17;
        rt->flags0 |= 2;
        rt->flags0 &= ~0xC0;
        MovementGobjInit(rt, 0, 0, 0, 0);
        PhysicsGobjBasicInit(rt, 0x2000, 1);
        CollisionGobjBasicInit(rt, rt->move->pos.vy, 0x666, 0x1000, 0x800, 0);
        light = EGSMemMalloc(0x40);
        gameObjectsInfo[rt->id].refId = (s32)light;
        func_80143FDC(rt, light);
        EGSDisplayFlashlightFrame(light, rt->anim->frame, 0);
        break;
    }
}

void SPOTLIGHTObjDispatch(SysMsg *msg) {
    LightRuntime *rt;
    BaseextFlashlight *light;
    EGSFrame *frame;
    BaseextMove *move;
    VECTOR32 saved;
    u8 done;
    s32 timer;

    rt = (LightRuntime *)gameObjectsInfo[msg->to].runtime;
    switch (msg->id) {
    case 10:
        /* Sweep while switched on; once off, keep going until the timer runs out. */
        if ((rt->healthInfo->health > 0 && (rt->flags0 & 2)) ||
            (timer = LIGHT_TIMER(rt) - 1, LIGHT_SET_TIMER(rt, timer), LIGHT_TIMER(rt) != 0)) {
            frame = rt->anim->frame;
            move = rt->move;
            saved.vx = frame->pos.vx;
            saved.vy = frame->pos.vy;
            saved.vz = frame->pos.vz;
            frame->pos.vx = move->pos.vx;
            frame->pos.vy = move->pos.vy;
            frame->pos.vz = move->pos.vz;
            PathHandleEx(rt, gObjPathScaleVector, 1, 0, -1, -1, &done);
            frame->pos.vx += rt->move->pendingMove.vx;
            frame->pos.vy += rt->move->pendingMove.vy;
            frame->pos.vz += rt->move->pendingMove.vz;
            rt->move->pendingMove.vx = 0;
            rt->move->pendingMove.vy = 0;
            rt->move->pendingMove.vz = 0;
            rt->move->pos.vx = frame->pos.vx;
            rt->move->pos.vy = frame->pos.vy;
            rt->move->pos.vz = frame->pos.vz;
            frame->pos.vx = saved.vx;
            frame->pos.vy = saved.vy;
            frame->pos.vz = saved.vz;
            EGSFrameLookAtPoint(frame, 0, &move->pos, 0);
            if (rt->flags0 & 0x40) {
                SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
            }
        }
        break;
    case 6:
        rt->flags0 |= 0x40;
        if (rt->healthInfo->health > 0 && (rt->flags0 & 2)) {
            EGSDisplayFlashlightOn(gameObjectsInfo[rt->id].refId);
            SysPostMessage(10, 4, rt->id, rt->id, 0, 0, 0, 0);
        }
        CobjAddBulletCollisionBoxGobj(rt, 1, 0, 0);
        break;
    case 5:
        rt->flags0 &= ~0x40;
        if (rt->healthInfo->health > 0 && (rt->flags0 & 2)) {
            EGSDisplayFlashlightOff(gameObjectsInfo[rt->id].refId);
        }
        CobjRemoveBulletCollisionBoxGobj(rt);
        break;
    case 13:
    case 42:
        if (rt->healthInfo->health <= 0) {
            EGSAnimShowDetailDef(rt->anim);
            rt->anim->unk18 = 0;
            rt->anim->unk09 = 0x14;
            if (rt->flags0 & 2) {
                rt->flags0 &= ~2;
                EGSDisplayFlashlightOff(gameObjectsInfo[rt->id].refId);
            }
        }
        break;
    case 20:
        if (rt->healthInfo->health > 0) {
            rt->flags0 ^= 2;
            if (rt->flags0 & 2) {
                rt->anim->unk18 = 0;
                rt->anim->unk09 = 0x1C;
                if (rt->flags0 & 0x40) {
                    EGSDisplayFlashlightOn(gameObjectsInfo[rt->id].refId);
                }
            } else {
                rt->anim->unk18 = 0;
                rt->anim->unk09 = 0x14;
                if (rt->flags0 & 0x40) {
                    EGSDisplayFlashlightOff(gameObjectsInfo[rt->id].refId);
                }
            }
        }
        break;
    case 2:
        func_801463A4(rt->id, 0);
        rt->npcState = 0xC;
        TargetInitGobj(rt, D_80145358, 0, 0);
        rt->cas = NULL;
        rt->unk21 = 0xC8;
        rt->anim->unk18 = 0;
        rt->anim->unk09 = 0x1C;
        rt->flags0 |= 2;
        rt->flags0 &= ~0xC0;
        if (rt->anim->flags0A & 8) {
            rt->unk20 |= 0x80;
        }
        PathSetupEX(rt, 1);
        if (rt->move != NULL) {
            PathGetPoint(rt, 0, &rt->move->pos);
            rt->move->pos.vy = -rt->move->pos.vy;
        }
        light = EGSMemMalloc(0x40);
        gameObjectsInfo[rt->id].refId = (s32)light;
        EGSDisplayFlashlightInit(light);
        EGSDisplayFlashlightFrame(light, rt->anim->frame, 0);
        EGSDisplayFlashlightParams(light, 0x96, 0xC, 6);
        LIGHT_SET_TIMER(rt, 12);
        break;
    }
}
