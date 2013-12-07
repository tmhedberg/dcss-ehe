/**
 * @file
 * @brief Collects information for output of status effects.
**/

#ifndef STATUS_H
#define STATUS_H

enum status_type
{
    STATUS_AIRBORNE = NUM_DURATIONS + 1,
    STATUS_BEHELD,
    STATUS_BURDEN,
    STATUS_CONTAMINATION,
    STATUS_NET,
    STATUS_HUNGER,
    STATUS_REGENERATION,
    STATUS_ROT,
    STATUS_SICK,
    STATUS_SPEED,
    STATUS_CLINGING,
    STATUS_SAGE,
    STATUS_STR_ZERO,
    STATUS_INT_ZERO,
    STATUS_DEX_ZERO,
    STATUS_FIREBALL,
    STATUS_BACKLIT,
    STATUS_UMBRA,
    STATUS_CONSTRICTED,
    STATUS_MANUAL,
    STATUS_AUGMENTED,
    STATUS_SUPPRESSED,
    STATUS_TERRAIN,
    STATUS_SILENCE,
    STATUS_MISSILES,
    STATUS_NO_CTELE,
    STATUS_BEOGH,
    STATUS_RECALL,
    STATUS_LIQUEFIED,
    STATUS_DRAINED,
    STATUS_RAY,
    STATUS_ELIXIR,
    STATUS_LAST_STATUS = STATUS_ELIXIR
};

struct status_info
{
    int light_colour;
    string light_text; // status light
    string short_text; // @: line
    string long_text;  // @ message
};

// status should be a duration or status_type
// *info will be filled in as appropriate for current
// character state
// returns true if the status has a description
bool fill_status_info(int status, status_info* info);

void init_duration_index();

#endif
