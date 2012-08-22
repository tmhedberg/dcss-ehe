/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"
#include "externs.h"

#include "act-iter.h"
#include "coord.h"
#include "delay.h"
#include "env.h"
#include "food.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-place.h"
#include "mon-util.h"
#include "place.h"
#include "player.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"

spret_type cast_cure_poison(int pow, bool fail)
{
    if (!you.duration[DUR_POISONING])
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    reduce_poison_player(2 + random2(pow) + random2(3));
    return SPRET_SUCCESS;
}

spret_type cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    int wielded = you.equip[EQ_WEAPON];

    if (wielded != -1)
    {
        if (you.inv[wielded].base_type == OBJ_FOOD
            && you.inv[wielded].sub_type == FOOD_CHUNK)
        {
            fail_check();
            success = true;

            mpr("The chunk of flesh you are holding crumbles to dust.");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(5 + random2(2 + pow / 15));

            dec_inv_item_quantity(wielded, 1);

            if (mons_genus(you.inv[wielded].mon_type) == MONS_ORC)
                did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
            if (mons_class_holiness(you.inv[wielded].mon_type) == MH_HOLY)
                did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);
        }
        else if (is_blood_potion(you.inv[wielded])
                 && item_type_known(you.inv[wielded]))
        {
            fail_check();
            success = true;

            mprf("The blood within %s froths and boils.",
                 you.inv[wielded].quantity > 1 ? "one of your flasks"
                                               : "the flask you are holding");

            mpr("A flood of magical energy pours into your mind!");

            inc_mp(5 + random2(2 + pow / 15));

            remove_oldest_blood_potion(you.inv[wielded]);
            dec_inv_item_quantity(wielded, 1);
        }
        else
            wielded = -1;
    }

    if (wielded == -1)
    {
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr("A conflicting enchantment prevents the spell from "
                "coming into effect.");
        }
        else if (!you.can_bleed())
        {
            if (you.species == SP_VAMPIRE)
                mpr("You don't have enough blood to draw power from your own body.");
            else
                mpr("Your body is bloodless.");
        }
        else if (!enough_hp(2, true))
            mpr("Your attempt to draw power from your own body fails.");
        else
        {
            int food = 0;
            // Take at most 90% of currhp.
            const int minhp = max(div_rand_round(you.hp, 10), 1);

            while (you.magic_points < you.max_magic_points && you.hp > minhp
                   && (you.is_undead != US_SEMI_UNDEAD
                       || you.hunger - food >= HUNGER_SATIATED))
            {
                fail_check();
                success = true;

                inc_mp(1);
                dec_hp(1, false);

                if (you.is_undead == US_SEMI_UNDEAD)
                    food += 15;

                for (int loopy = 0; loopy < (you.hp > minhp ? 3 : 0); ++loopy)
                    if (x_chance_in_y(6, pow))
                        dec_hp(1, false);

                if (x_chance_in_y(6, pow))
                    break;
            }
            if (success)
                mpr("You draw magical energy from your own body!");
            else
                mpr("Your attempt to draw power from your own body fails.");

            make_hungry(food, false);
        }
    }

    return success ? SPRET_SUCCESS : SPRET_ABORT;
}

spret_type cast_death_channel(int pow, god_type god, bool fail)
{
    if (you.duration[DUR_DEATH_CHANNEL] >= 30 * BASELINE_DELAY)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return SPRET_ABORT;
    }

    fail_check();
    mpr("Malign forces permeate your being, awaiting release.");

    you.increase_duration(DUR_DEATH_CHANNEL, 15 + random2(1 + pow/3), 100);

    if (god != GOD_NO_GOD)
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);

    return SPRET_SUCCESS;
}

spret_type cast_recall(bool fail)
{
    fail_check();
    start_recall(0);
    return SPRET_SUCCESS;
}

struct recall_sorter
{
    bool operator()(const pair<mid_t,int> &a, const pair<mid_t,int> &b)
    {
        return a.second > b.second;
    }
};

// Type recalled:
// 0 = anything
// 1 = undead only (Yred religion ability)
// 2 = orcs only (Beogh religion ability)
void start_recall(int type)
{
    // Assemble the recall list.
    vector<pair<mid_t, int> > rlist;

    you.recall_list.clear();
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mons_is_recallable(&you, *mi))
            continue;

        if (type == 1) // undead
        {
            if (mi->holiness() != MH_UNDEAD)
                continue;
        }
        else if (type == 2) // Beogh
        {
            if (!is_orcish_follower(*mi))
                continue;
        }

        pair<mid_t, int> m = make_pair(mi->mid, mi->hit_dice);
        rlist.push_back(m);
    }

    if (type > 0 && branch_allows_followers(you.where_are_you))
        populate_offlevel_recall_list(rlist);

    if (!rlist.empty())
    {
        // Sort the recall list roughly by HD, randomizing a little
        for (unsigned int i = 0; i < rlist.size(); ++i)
            rlist[i].second += random2(10);
        sort(rlist.begin(), rlist.end(), recall_sorter());

        you.recall_list.clear();
        for (unsigned int i = 0; i < rlist.size(); ++i)
            you.recall_list.push_back(rlist[i].first);

        you.attribute[ATTR_NEXT_RECALL_INDEX] = 1;
        you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
        mpr("You begin recalling your allies.");
    }
    else
        mpr("Nothing appears to have answered your call.");
}

// Remind a recalled ally (or one skipped due to proximity) not to run
// away or wander off.
void recall_orders(monster *mons)
{
    // FIXME: is this okay for berserk monsters? We still want them to
    // stick around...

    // Don't patrol
    mons->patrol_point = coord_def(0, 0);

    // Don't wander
    mons->behaviour = BEH_SEEK;

    // Don't pursue distant enemies
    const actor *foe = mons->get_foe();
    if (foe && !you.can_see(foe))
        mons->foe = MHITYOU;
}

// Attempt to recall a single monster by mid, which might be either on or off
// our current level. Returns whether this monster was successfully recalled.
static bool _try_recall(mid_t mid)
{
    monster* mons = monster_by_mid(mid);
    // Either it's dead or off-level.
    if (!mons)
        return recall_offlevel_ally(mid);
    else if (mons->alive())
    {
        // Don't recall monsters that are already close to the player
        if (mons->pos().distance_from(you.pos()) < 3
            && mons->see_cell_no_trans(you.pos()))
        {
            recall_orders(mons);
            return false;
        }
        else
        {
            coord_def empty;
            if (find_habitable_spot_near(you.pos(), mons_base_type(mons), 3, false, empty)
                && mons->move_to_pos(empty))
            {
                recall_orders(mons);
                simple_monster_message(mons, " is recalled.");
                return true;
            }
        }
    }

    return false;
}

// Attempt to recall a number of allies proportional to how much time
// has passed. Once the list has been fully processed, terminate the
// status.
void do_recall(int time)
{
    while (time > you.attribute[ATTR_NEXT_RECALL_TIME])
    {
        // Try to recall an ally.
        mid_t mid = you.recall_list[you.attribute[ATTR_NEXT_RECALL_INDEX]-1];
        you.attribute[ATTR_NEXT_RECALL_INDEX]++;
        if (_try_recall(mid))
        {
            time -= you.attribute[ATTR_NEXT_RECALL_TIME];
            you.attribute[ATTR_NEXT_RECALL_TIME] = 3 + random2(4);
        }
        if ((unsigned int)you.attribute[ATTR_NEXT_RECALL_INDEX] >
             you.recall_list.size())
        {
            end_recall();
            mpr("You finish recalling your allies.");
            return;
        }
    }

    you.attribute[ATTR_NEXT_RECALL_TIME] -= time;
    return;
}

void end_recall()
{
    you.attribute[ATTR_NEXT_RECALL_INDEX] = 0;
    you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
    you.recall_list.clear();
}

// Cast_phase_shift: raises evasion (by 8 currently) via Translocations.
spret_type cast_phase_shift(int pow, bool fail)
{
    if (you.duration[DUR_DIMENSION_ANCHOR])
    {
        mpr("You are anchored firmly to the material plane!");
        return SPRET_ABORT;
    }

    fail_check();
    if (!you.duration[DUR_PHASE_SHIFT])
        mpr("You feel the strange sensation of being on two planes at once.");
    else
        mpr("You feel the material plane grow further away.");

    you.increase_duration(DUR_PHASE_SHIFT, 5 + random2(pow), 30);
    you.redraw_evasion = true;
    return SPRET_SUCCESS;
}

static bool _feat_is_passwallable(dungeon_feature_type feat)
{
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls...
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return true;
    default:
        return false;
    }
}

spret_type cast_passwall(const coord_def& delta, int pow, bool fail)
{
    int shallow = 1 + you.skill(SK_EARTH_MAGIC) / 8;
    int range = shallow + random2(pow) / 25;
    int maxrange = shallow + pow / 25;

    coord_def dest;
    for (dest = you.pos() + delta;
         in_bounds(dest) && _feat_is_passwallable(grd(dest));
         dest += delta)
    {}

    int walls = (dest - you.pos()).rdist() - 1;
    if (walls == 0)
    {
        mpr("That's not a passable wall.");
        return SPRET_ABORT;
    }

    fail_check();

    // Below here, failing to cast yields information to the
    // player, so we don't make the spell abort (return true).
    if (!in_bounds(dest))
        mpr("You sense an overwhelming volume of rock.");
    else if (cell_is_solid(dest))
        mpr("Something is blocking your path through the rock.");
    else if (walls > maxrange)
        mpr("This rock feels extremely deep.");
    else if (walls > range)
        mpr("You fail to penetrate the rock.");
    else
    {
        string msg;
        if (grd(dest) == DNGN_DEEP_WATER)
            msg = "You sense a large body of water on the other side of the rock.";
        else if (grd(dest) == DNGN_LAVA)
            msg = "You sense an intense heat on the other side of the rock.";

        if (check_moveto(dest, "passwall", msg))
        {
            // Passwall delay is reduced, and the delay cannot be interrupted.
            start_delay(DELAY_PASSWALL, 1 + walls, dest.x, dest.y);
        }
    }
    return SPRET_SUCCESS;
}

static int _intoxicate_monsters(coord_def where, int pow, int, actor *)
{
    monster* mons = monster_at(where);
    if (mons == NULL
        || mons_intel(mons) < I_NORMAL
        || mons->holiness() != MH_NATURAL
        || mons->res_poison() > 0)
    {
        return 0;
    }

    if (x_chance_in_y(40 + pow/3, 100))
    {
        if (mons->check_clarity(false))
            return 1;
        mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you));
        simple_monster_message(mons, " looks rather confused.");
        return 1;
    }
    return 0;
}

spret_type cast_intoxicate(int pow, bool fail)
{
    fail_check();
    mpr("You radiate an intoxicating aura.");
    if (x_chance_in_y(60 - pow/3, 100))
        potion_effect(POT_CONFUSION, 10 + (100 - pow) / 10);

    if (one_chance_in(20)
        && lose_stat(STAT_INT, 1 + random2(3), false,
                      "casting intoxication"))
    {
        mpr("Your head spins!");
    }

    apply_area_visible(_intoxicate_monsters, pow, &you);
    return SPRET_SUCCESS;
}

// The intent of this spell isn't to produce helpful potions
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not
// using up the corpse might also lead to game balance problems. - bwr
spret_type cast_fulsome_distillation(int pow, bool check_range, bool fail)
{
    int num_corpses = 0;
    item_def *corpse = corpse_at(you.pos(), &num_corpses);
    if (num_corpses && you.flight_mode())
        num_corpses = -1;

    // If there is only one corpse, distill it; otherwise, ask the player
    // which corpse to use.
    switch (num_corpses)
    {
        case 0: case -1:
            // Allow using Z to victory dance fulsome.
            if (!check_range)
            {
                fail_check();
                mpr("The spell fizzles.");
                return SPRET_SUCCESS;
            }

            if (num_corpses == -1)
                mpr("You can't reach the corpse!");
            else
                mpr("There aren't any corpses here.");
            return SPRET_ABORT;
        case 1:
            // Use the only corpse available without prompting.
            break;
        default:
            // Search items at the player's location for corpses.
            // The last corpse detected earlier is irrelevant.
            corpse = NULL;
            for (stack_iterator si(you.pos(), true); si; ++si)
            {
                if (item_is_corpse(*si))
                {
                    const std::string corpsedesc =
                        get_menu_colour_prefix_tags(*si, DESC_THE);
                    const std::string prompt =
                        make_stringf("Distill a potion from %s?",
                                     corpsedesc.c_str());

                    if (yesno(prompt.c_str(), true, 0, false))
                    {
                        corpse = &*si;
                        break;
                    }
                }
            }
    }

    if (!corpse)
    {
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    potion_type pot_type = POT_WATER;

    switch (mons_corpse_effect(corpse->mon_type))
    {
    case CE_CLEAN:
        pot_type = POT_WATER;
        break;

    case CE_CONTAMINATED:
        pot_type = (mons_weight(corpse->mon_type) >= 900)
            ? POT_DEGENERATION : POT_CONFUSION;
        break;

    case CE_POISONOUS:
    case CE_POISON_CONTAM:
        pot_type = POT_POISON;
        break;

    case CE_MUTAGEN:
        pot_type = POT_MUTATION;
        break;

    case CE_ROTTEN:         // actually this only occurs via mangling
    case CE_ROT:            // necrophage
        pot_type = POT_DECAY;
        break;

    case CE_NOCORPSE:       // shouldn't occur
    default:
        break;
    }

    switch (corpse->mon_type)
    {
    case MONS_RED_WASP:              // paralysis attack
        pot_type = POT_PARALYSIS;
        break;

    case MONS_YELLOW_WASP:           // slowing attack
        pot_type = POT_SLOWING;
        break;

    default:
        break;
    }

    struct monsterentry* smc = get_monster_data(corpse->mon_type);

    for (int nattk = 0; nattk < 4; ++nattk)
    {
        if (smc->attack[nattk].flavour == AF_POISON_MEDIUM
            || smc->attack[nattk].flavour == AF_POISON_STRONG
            || smc->attack[nattk].flavour == AF_POISON_STR
            || smc->attack[nattk].flavour == AF_POISON_INT
            || smc->attack[nattk].flavour == AF_POISON_DEX
            || smc->attack[nattk].flavour == AF_POISON_STAT)
        {
            pot_type = POT_STRONG_POISON;
        }
    }

    const bool was_orc = (mons_genus(corpse->mon_type) == MONS_ORC);
    const bool was_holy = (mons_class_holiness(corpse->mon_type) == MH_HOLY);

    // We borrow the corpse's object to make our potion.
    corpse->base_type = OBJ_POTIONS;
    corpse->sub_type  = pot_type;
    corpse->quantity  = 1;
    corpse->plus      = 0;
    corpse->plus2     = 0;
    corpse->flags     = 0;
    corpse->inscription.clear();
    item_colour(*corpse); // sets special as well

    // Always identify said potion.
    set_ident_type(*corpse, ID_KNOWN_TYPE);

    mprf("You extract %s from the corpse.",
         corpse->name(DESC_A).c_str());

    // Try to move the potion to the player (for convenience);
    // they probably won't autopickup bad potions.
    // Treats potion as though it was being picked up manually (0005916).
    std::map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    if (move_item_to_player(corpse->index(), 1) != 1)
        mpr("Unfortunately, you can't carry it right now!");

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    if (was_orc)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
    if (was_holy)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);

    return SPRET_SUCCESS;
}

void remove_condensation_shield()
{
    mprf(MSGCH_DURATION, "Your icy shield evaporates.");
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
}

spret_type cast_condensation_shield(int pow, bool fail)
{
    if (you.shield() || you.duration[DUR_FIRE_SHIELD])
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        mpr("The disc of vapour around you crackles some more.");
    else
        mpr("A crackling disc of dense vapour forms in the air!");
    you.increase_duration(DUR_CONDENSATION_SHIELD, 15 + random2(pow), 40);
    you.redraw_armour_class = true;

    return SPRET_SUCCESS;
}

spret_type cast_stoneskin(int pow, bool fail)
{
    if (you.form != TRAN_NONE
        && you.form != TRAN_APPENDAGE
        && you.form != TRAN_STATUE
        && you.form != TRAN_BLADE_HANDS)
    {
        mpr("This spell does not affect your current form.");
        return SPRET_ABORT;
    }

    if (you.duration[DUR_ICY_ARMOUR])
    {
        mpr("This spell conflicts with another spell still in effect.");
        return SPRET_ABORT;
    }

    if (you.species == SP_LAVA_ORC)
    {
        // We can't get here from normal casting, and probably don't want
        // a message from the Helm card.
        // mpr("Your skin is naturally stony.");
        return SPRET_ABORT;
    }

    fail_check();

    if (you.duration[DUR_STONESKIN])
        mpr("Your skin feels harder.");
    else
    {
        if (you.form == TRAN_STATUE)
            mpr("Your stone body feels more resilient.");
        else
            mpr("Your skin hardens.");

        you.redraw_armour_class = true;
    }

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);

    return SPRET_SUCCESS;
}

spret_type cast_darkness(int pow, bool fail)
{
    if (you.haloed())
    {
        mpr("It would have no effect in that bright light!");
        return SPRET_ABORT;
    }

    fail_check();
    if (you.duration[DUR_DARKNESS])
        mprf(MSGCH_DURATION, "It gets a bit darker.");
    else
        mprf(MSGCH_DURATION, "It gets dark.");
    you.increase_duration(DUR_DARKNESS, 15 + random2(1 + pow/3), 100);
    update_vision_range();

    return SPRET_SUCCESS;
}
