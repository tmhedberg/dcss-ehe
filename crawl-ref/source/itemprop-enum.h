#ifndef ITEMPROP_ENUM_H
#define ITEMPROP_ENUM_H

enum armour_type
{
    ARM_ROBE,
    ARM_LEATHER_ARMOUR,
    ARM_RING_MAIL,
    ARM_SCALE_MAIL,
    ARM_CHAIN_MAIL,
    ARM_PLATE_ARMOUR,

    ARM_CLOAK,

    ARM_CAP,
    ARM_WIZARD_HAT,
    ARM_HELMET,

    ARM_GLOVES,

    ARM_BOOTS,

    ARM_BUCKLER, // order of shields matters
    ARM_SHIELD,
    ARM_LARGE_SHIELD,
    ARM_MAX_RACIAL = ARM_LARGE_SHIELD,

    ARM_CRYSTAL_PLATE_ARMOUR,
    ARM_MIN_UNBRANDED = ARM_CRYSTAL_PLATE_ARMOUR,

    ARM_ANIMAL_SKIN,

    ARM_TROLL_HIDE,
    ARM_TROLL_LEATHER_ARMOUR,

    ARM_FIRE_DRAGON_HIDE,
    ARM_FIRE_DRAGON_ARMOUR,
    ARM_ICE_DRAGON_HIDE,
    ARM_ICE_DRAGON_ARMOUR,
    ARM_STEAM_DRAGON_HIDE,
    ARM_STEAM_DRAGON_ARMOUR,
    ARM_MOTTLED_DRAGON_HIDE,
    ARM_MOTTLED_DRAGON_ARMOUR,
    ARM_STORM_DRAGON_HIDE,
    ARM_STORM_DRAGON_ARMOUR,
    ARM_GOLD_DRAGON_HIDE,
    ARM_GOLD_DRAGON_ARMOUR,
    ARM_SWAMP_DRAGON_HIDE,
    ARM_SWAMP_DRAGON_ARMOUR,
    ARM_PEARL_DRAGON_HIDE,
    ARM_PEARL_DRAGON_ARMOUR,
    ARM_MAX_UNBRANDED = ARM_PEARL_DRAGON_ARMOUR,

    ARM_CENTAUR_BARDING,
    ARM_NAGA_BARDING,

    NUM_ARMOURS
};

enum armour_property_type
{
    PARM_AC,
    PARM_EVASION,
};

enum boot_type          // used in pluses2
{
    TBOOT_BOOTS,
    TBOOT_NAGA_BARDING,
    TBOOT_CENTAUR_BARDING,
    NUM_BOOT_TYPES
};

const int SP_FORBID_EGO   = -1;
const int SP_FORBID_BRAND = -1;
const int SP_UNKNOWN_BRAND = 31; // seen_weapon/armour is a 32-bit bitfield

// Be sure to update _debug_acquirement_stats and _str_to_ego to match.
enum brand_type // item_def.special
{
    SPWPN_FORBID_BRAND = -1,
    SPWPN_NORMAL,
    SPWPN_FLAMING,
    SPWPN_FREEZING,
    SPWPN_HOLY_WRATH,
    SPWPN_ELECTROCUTION,
#if TAG_MAJOR_VERSION == 34
    SPWPN_ORC_SLAYING,
#endif
    SPWPN_DRAGON_SLAYING,
    SPWPN_VENOM,
    SPWPN_PROTECTION,
    SPWPN_DRAINING,
    SPWPN_SPEED,
    SPWPN_VORPAL,
    SPWPN_FLAME,   // ranged, only
    SPWPN_FROST,   // ranged, only
    SPWPN_VAMPIRICISM,
    SPWPN_PAIN,
    SPWPN_ANTIMAGIC,
    SPWPN_DISTORTION,
#if TAG_MAJOR_VERSION == 34
    SPWPN_REACHING,
    SPWPN_RETURNING,
#endif
    SPWPN_CHAOS,
    SPWPN_EVASION,

    MAX_PAN_LORD_BRANDS = SPWPN_EVASION,

#if TAG_MAJOR_VERSION == 34
    SPWPN_CONFUSE, // XXX not a real weapon brand, only for Confusing Touch
#endif
    SPWPN_PENETRATION,
    SPWPN_REAPING,

// From this point on save compat is irrelevant.
    NUM_REAL_SPECIAL_WEAPONS,

    SPWPN_ACID,    // acid bite only for the moment
#if TAG_MAJOR_VERSION != 34
    SPWPN_CONFUSE, // Confusing Touch only for the moment
#endif
    SPWPN_DEBUG_RANDART,
    NUM_SPECIAL_WEAPONS,
};

enum corpse_type
{
    CORPSE_BODY,
    CORPSE_SKELETON,
};

enum hands_reqd_type
{
    HANDS_ONE,
    HANDS_TWO,
};

enum helmet_desc_type
{
    THELM_DESC_PLAIN,
    THELM_DESC_WINGED,
    THELM_DESC_HORNED,
    THELM_DESC_CRESTED,
    THELM_DESC_PLUMED,
    THELM_DESC_MAX_SOFT = THELM_DESC_PLUMED,
    THELM_DESC_SPIKED,
    THELM_DESC_VISORED,
    THELM_DESC_GOLDEN,
    THELM_NUM_DESCS
};

enum jewellery_type
{
    RING_REGENERATION,
    RING_FIRST_RING = RING_REGENERATION,
    RING_PROTECTION,
    RING_PROTECTION_FROM_FIRE,
    RING_POISON_RESISTANCE,
    RING_PROTECTION_FROM_COLD,
    RING_STRENGTH,
    RING_SLAYING,
    RING_SEE_INVISIBLE,
    RING_INVISIBILITY,
    RING_HUNGER,
    RING_TELEPORTATION,
    RING_EVASION,
    RING_SUSTAIN_ABILITIES,
    RING_SUSTENANCE,
    RING_DEXTERITY,
    RING_INTELLIGENCE,
    RING_WIZARDRY,
    RING_MAGICAL_POWER,
    RING_FLIGHT,
    RING_LIFE_PROTECTION,
    RING_PROTECTION_FROM_MAGIC,
    RING_FIRE,
    RING_ICE,
    RING_TELEPORT_CONTROL,
    NUM_RINGS,                         //   keep as last ring; should not overlap
                                       //   with amulets!
    // RINGS after num_rings are for unique types for artefacts
    //   (no non-artefact version).
    // Currently none.

    AMU_RAGE = 35,
    AMU_FIRST_AMULET = AMU_RAGE,
    AMU_CLARITY,
    AMU_WARDING,
    AMU_RESIST_CORROSION,
    AMU_THE_GOURMAND,
    AMU_CONSERVATION,
#if TAG_MAJOR_VERSION == 34
    AMU_CONTROLLED_FLIGHT,
#endif
    AMU_INACCURACY,
    AMU_RESIST_MUTATION,
    AMU_GUARDIAN_SPIRIT,
    AMU_FAITH,
    AMU_STASIS,

    NUM_JEWELLERY
};

enum launch_retval
{
    LRET_FUMBLED,
    LRET_LAUNCHED,
    LRET_THROWN,
};

enum misc_item_type
{
    MISC_BOTTLED_EFREET,
    MISC_FAN_OF_GALES,
    MISC_LAMP_OF_FIRE,
    MISC_STONE_OF_TREMORS,
    MISC_LANTERN_OF_SHADOWS,
    MISC_HORN_OF_GERYON,
    MISC_BOX_OF_BEASTS,
    MISC_CRYSTAL_BALL_OF_ENERGY,
#if TAG_MAJOR_VERSION == 34
    MISC_BUGGY_EBONY_CASKET,
#endif
    MISC_DISC_OF_STORMS,

    // pure decks
    MISC_DECK_OF_ESCAPE,
    MISC_DECK_OF_DESTRUCTION,
    MISC_DECK_OF_DUNGEONS,
    MISC_DECK_OF_SUMMONING,
    MISC_DECK_OF_WONDERS,
    MISC_DECK_OF_PUNISHMENT,

    // mixed decks
    MISC_DECK_OF_WAR,
    MISC_DECK_OF_CHANGES,
    MISC_DECK_OF_DEFENCE,

    MISC_RUNE_OF_ZOT,

    MISC_QUAD_DAMAGE, // Sprint only

    MISC_PHIAL_OF_FLOODS,
    MISC_SACK_OF_SPIDERS,

    NUM_MISCELLANY, // mv: used for random generation
    MISC_FIRST_DECK = MISC_DECK_OF_ESCAPE,
    MISC_LAST_DECK  = MISC_DECK_OF_DEFENCE,
};

enum missile_type
{
    MI_DART,
    MI_NEEDLE,
    MI_ARROW,
    MI_BOLT,
    MI_JAVELIN,

    MI_STONE,
    MI_LARGE_ROCK,
    MI_SLING_BULLET,
    MI_THROWING_NET,
    MI_TOMAHAWK,

    NUM_MISSILES,
    MI_NONE             // was MI_EGGPLANT... used for launch type detection
};

enum rune_type
{
    RUNE_SWAMP,
    RUNE_SNAKE,
    RUNE_SHOALS,
    RUNE_SLIME,
    RUNE_ELF, // unused
    RUNE_VAULTS,
    RUNE_TOMB,

    RUNE_DIS,
    RUNE_GEHENNA,
    RUNE_COCYTUS,
    RUNE_TARTARUS,

    RUNE_ABYSSAL,

    RUNE_DEMONIC,

    // order must match monsters
    RUNE_MNOLEG,
    RUNE_LOM_LOBON,
    RUNE_CEREBOV,
    RUNE_GLOORX_VLOQ,

    RUNE_SPIDER,
    RUNE_FOREST,
    NUM_RUNE_TYPES
};

enum scroll_type
{
    SCR_IDENTIFY,
    SCR_TELEPORTATION,
    SCR_FEAR,
    SCR_NOISE,
    SCR_REMOVE_CURSE,
    SCR_SUMMONING,
    SCR_ENCHANT_WEAPON_I,
    SCR_ENCHANT_ARMOUR,
    SCR_TORMENT,
    SCR_RANDOM_USELESSNESS,
    SCR_CURSE_WEAPON,
    SCR_CURSE_ARMOUR,
    SCR_IMMOLATION,
    SCR_BLINKING,
    SCR_MAGIC_MAPPING,
    SCR_FOG,
    SCR_ACQUIREMENT,
    SCR_ENCHANT_WEAPON_II,
    SCR_BRAND_WEAPON,
    SCR_RECHARGING,
    SCR_ENCHANT_WEAPON_III,
    SCR_HOLY_WORD,
    SCR_VULNERABILITY,
    SCR_SILENCE,
    SCR_AMNESIA,
    SCR_CURSE_JEWELLERY,
    NUM_SCROLLS
};

// Be sure to update _debug_acquirement_stats and _str_to_ego to match.
enum special_armour_type
{
    SPARM_FORBID_EGO = -1,
    SPARM_NORMAL,
    SPARM_RUNNING,
    SPARM_FIRE_RESISTANCE,
    SPARM_COLD_RESISTANCE,
    SPARM_POISON_RESISTANCE,
    SPARM_SEE_INVISIBLE,
    SPARM_DARKNESS,
    SPARM_STRENGTH,
    SPARM_DEXTERITY,
    SPARM_INTELLIGENCE,
    SPARM_PONDEROUSNESS,
    SPARM_FLYING,
    SPARM_MAGIC_RESISTANCE,
    SPARM_PROTECTION,
    SPARM_STEALTH,
    SPARM_RESISTANCE,
    SPARM_POSITIVE_ENERGY,
    SPARM_ARCHMAGI,
    SPARM_PRESERVATION,
    SPARM_REFLECTION,
    SPARM_SPIRIT_SHIELD,
    SPARM_ARCHERY,
    SPARM_JUMPING,
    NUM_REAL_SPECIAL_ARMOURS,
    NUM_SPECIAL_ARMOURS,
};

// Be sure to update _str_to_ego to match.
enum special_missile_type // to separate from weapons in general {dlb}
{
    SPMSL_FORBID_BRAND = -1,
    SPMSL_NORMAL,
    SPMSL_FLAME,
    SPMSL_FROST,
    SPMSL_POISONED,
    SPMSL_CURARE,                      // Needle-only brand
    SPMSL_RETURNING,
    SPMSL_CHAOS,
    SPMSL_PENETRATION,
    SPMSL_DISPERSAL,
    SPMSL_EXPLODING,
    SPMSL_STEEL,
    SPMSL_SILVER,
    SPMSL_PARALYSIS,                   // needle only from here on
    SPMSL_SLOW,
    SPMSL_SLEEP,
    SPMSL_CONFUSION,
#if TAG_MAJOR_VERSION == 34
    SPMSL_SICKNESS,
#endif
    SPMSL_FRENZY,
    NUM_REAL_SPECIAL_MISSILES,
    SPMSL_BLINDING,
    NUM_SPECIAL_MISSILES,
};

enum special_ring_type // jewellery mitm[].special values
{
    SPRING_RANDART = 200,
    SPRING_UNRANDART = 201,
};

enum stave_type
{
    STAFF_WIZARDRY,
    STAFF_POWER,
    STAFF_FIRE,
    STAFF_COLD,
    STAFF_POISON,
    STAFF_ENERGY,
    STAFF_DEATH,
    STAFF_CONJURATION,
#if TAG_MAJOR_VERSION == 34
    STAFF_ENCHANTMENT,
#endif
    STAFF_SUMMONING,
    STAFF_AIR,
    STAFF_EARTH,
#if TAG_MAJOR_VERSION == 34
    STAFF_CHANNELING,
#endif
    NUM_STAVES,
};

enum rod_type
{
    ROD_LIGHTNING,
    ROD_SWARM,
    ROD_FIERY_DESTRUCTION,
    ROD_FRIGID_DESTRUCTION,
    ROD_DESTRUCTION,
    ROD_INACCURACY,
    ROD_WARDING,
    ROD_DEMONOLOGY,
    ROD_STRIKING,
    ROD_VENOM,
    NUM_RODS,
};

enum weapon_type
{
    WPN_CLUB,
    WPN_WHIP,
    WPN_HAMMER,
    WPN_MACE,
    WPN_FLAIL,
    WPN_MORNINGSTAR,
    WPN_ROD, // base item for magical rods only
    WPN_DIRE_FLAIL,
    WPN_EVENINGSTAR,
    WPN_GREAT_MACE,

    WPN_DAGGER,
    WPN_QUICK_BLADE,
    WPN_SHORT_SWORD,
    WPN_SABRE,

    WPN_FALCHION,
    WPN_LONG_SWORD,
    WPN_SCIMITAR,
    WPN_GREAT_SWORD,

    WPN_HAND_AXE,
    WPN_WAR_AXE,
    WPN_BROAD_AXE,
    WPN_BATTLEAXE,
    WPN_EXECUTIONERS_AXE,

    WPN_SPEAR,
    WPN_TRIDENT,
    WPN_HALBERD,
    WPN_GLAIVE,
    WPN_BARDICHE,

    WPN_BLOWGUN,
    WPN_CROSSBOW,
    WPN_BOW,
    WPN_LONGBOW,
    WPN_MAX_RACIAL = WPN_LONGBOW,

    WPN_DEMON_WHIP,
    WPN_GIANT_CLUB,
    WPN_GIANT_SPIKED_CLUB,

    WPN_DEMON_BLADE,
    WPN_DOUBLE_SWORD,
    WPN_TRIPLE_SWORD,

    WPN_DEMON_TRIDENT,
    WPN_SCYTHE,

    WPN_STAFF,          // Just used for the weapon stats for magical staves.
    WPN_QUARTERSTAFF,
    WPN_LAJATANG,

    WPN_SLING,

    WPN_BLESSED_FALCHION,
    WPN_BLESSED_LONG_SWORD,
    WPN_BLESSED_SCIMITAR,
    WPN_BLESSED_GREAT_SWORD,
    WPN_EUDEMON_BLADE,
    WPN_BLESSED_DOUBLE_SWORD,
    WPN_BLESSED_TRIPLE_SWORD,
    WPN_SACRED_SCOURGE,
    WPN_TRISHULA,

    NUM_WEAPONS,

// special cases
    WPN_UNARMED,
    WPN_UNKNOWN,
    WPN_RANDOM,
    WPN_VIABLE,

// thrown weapons (for hunter weapon selection) - rocks, javelins, tomahawks
    WPN_THROWN,
};

enum weapon_property_type
{
    PWPN_DAMAGE,
    PWPN_HIT,
    PWPN_SPEED,
    PWPN_ACQ_WEIGHT,
};

enum vorpal_damage_type
{
    // These are the types of damage a weapon can do.  You can set more
    // than one of these.
    DAM_BASH            = 0x0000,       // non-melee weapon blugeoning
    DAM_BLUDGEON        = 0x0001,       // crushing
    DAM_SLICE           = 0x0002,       // slicing/chopping
    DAM_PIERCE          = 0x0004,       // stabbing/piercing
    DAM_WHIP            = 0x0008,       // whip slashing (no butcher)
    DAM_MAX_TYPE        = DAM_WHIP,

    // These are used for vorpal weapon descriptions.  You shouldn't set
    // more than one of these.
    DVORP_NONE          = 0x0000,       // used for non-melee weapons
    DVORP_CRUSHING      = 0x1000,
    DVORP_SLICING       = 0x2000,
    DVORP_PIERCING      = 0x3000,
    DVORP_CHOPPING      = 0x4000,       // used for axes
    DVORP_SLASHING      = 0x5000,       // used for whips
    DVORP_STABBING      = 0x6000,       // used for knives/daggers

    DVORP_CLAWING       = 0x7000,       // claw damage
    DVORP_TENTACLE      = 0x8000,       // tentacle damage

    // These are shortcuts to tie vorpal/damage types for easy setting...
    // as above, setting more than one vorpal type is trouble.
    DAMV_NON_MELEE      = DVORP_NONE     | DAM_BASH,            // launchers
    DAMV_CRUSHING       = DVORP_CRUSHING | DAM_BLUDGEON,
    DAMV_SLICING        = DVORP_SLICING  | DAM_SLICE,
    DAMV_PIERCING       = DVORP_PIERCING | DAM_PIERCE,
    DAMV_CHOPPING       = DVORP_CHOPPING | DAM_SLICE,
    DAMV_SLASHING       = DVORP_SLASHING | DAM_WHIP,
    DAMV_STABBING       = DVORP_STABBING | DAM_PIERCE,

    DAM_MASK            = 0x0fff,       // strips vorpal specification
    DAMV_MASK           = 0xf000,       // strips non-vorpal specification
};

enum wand_type
{
    WAND_FLAME,
    WAND_FROST,
    WAND_SLOWING,
    WAND_HASTING,
    WAND_MAGIC_DARTS,
    WAND_HEAL_WOUNDS,
    WAND_PARALYSIS,
    WAND_FIRE,
    WAND_COLD,
    WAND_CONFUSION,
    WAND_INVISIBILITY,
    WAND_DIGGING,
    WAND_FIREBALL,
    WAND_TELEPORTATION,
    WAND_LIGHTNING,
    WAND_POLYMORPH,
    WAND_ENSLAVEMENT,
    WAND_DRAINING,
    WAND_RANDOM_EFFECTS,
    WAND_DISINTEGRATION,
    NUM_WANDS
};

enum zap_count_type
{
    ZAPCOUNT_EMPTY       = -1,
    ZAPCOUNT_UNKNOWN     = -2,
    ZAPCOUNT_RECHARGED   = -3,
};

enum food_type
{
    FOOD_MEAT_RATION,
    FOOD_BREAD_RATION,
    FOOD_PEAR,
    FOOD_APPLE,
    FOOD_CHOKO,
    FOOD_HONEYCOMB,
    FOOD_ROYAL_JELLY,
    FOOD_SNOZZCUMBER,
    FOOD_PIZZA,
    FOOD_APRICOT,
    FOOD_ORANGE,
    FOOD_BANANA,
    FOOD_STRAWBERRY,
    FOOD_RAMBUTAN,
    FOOD_LEMON,
    FOOD_GRAPE,
    FOOD_SULTANA,
    FOOD_LYCHEE,
    FOOD_BEEF_JERKY,
    FOOD_CHEESE,
    FOOD_SAUSAGE,
    FOOD_CHUNK,
    FOOD_AMBROSIA,
    NUM_FOODS
};

#endif
