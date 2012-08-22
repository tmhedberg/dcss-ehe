/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"
#include "externs.h"

#include <algorithm>

#include "act-iter.h"
#include "beam.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "viewchar.h"
#include "shout.h"

static void _burn_tree(coord_def pos)
{
    bolt beam;
    beam.origin_spell = SPELL_CONJURE_FLAME;
    beam.range = 1;
    beam.flavour = BEAM_FIRE;
    beam.name = "fireball"; // yay doing this by name
    beam.source = beam.target = pos;
    beam.set_agent(&you);
    beam.fire();
}

spret_type conjure_flame(int pow, const coord_def& where, bool fail)
{
    // FIXME: This would be better handled by a flag to enforce max range.
    if (distance2(where, you.pos()) > dist_range(spell_range(SPELL_CONJURE_FLAME,
                                                      pow))
        || !in_bounds(where))
    {
        mpr("That's too far away.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(where))
    {
        switch (grd(where))
        {
        case DNGN_METAL_WALL:
            mpr("You can't ignite solid metal!");
            break;
        case DNGN_GREEN_CRYSTAL_WALL:
            mpr("You can't ignite solid crystal!");
            break;
        case DNGN_TREE:
        case DNGN_MANGROVE:
            fail_check();
            _burn_tree(where);
            return SPRET_SUCCESS;
        default:
            mpr("You can't ignite solid rock!");
            break;
        }
        return SPRET_ABORT;
    }

    const int cloud = env.cgrid(where);

    if (cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
    {
        mpr("There's already a cloud there!");
        return SPRET_ABORT;
    }

    // Note that self-targetting is handled by SPFLAG_NOT_SELF.
    monster* mons = monster_at(where);
    if (mons)
    {
        if (you.can_see(mons))
        {
            mpr("You can't place the cloud on a creature.");
            return SPRET_ABORT;
        }

        fail_check();

        // FIXME: maybe should do _paranoid_option_disable() here?
        mpr("You see a ghostly outline there, and the spell fizzles.");
        return SPRET_SUCCESS;      // Don't give free detection!
    }

    fail_check();

    if (cloud != EMPTY_CLOUD)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr("The fire roars with new energy!");
        const int extra_dur = 2 + min(random2(pow) / 2, 20);
        env.cloud[cloud].decay += extra_dur * 5;
        env.cloud[cloud].set_whose(KC_YOU);
    }
    else
    {
        const int durat = min(5 + (random2(pow)/2) + (random2(pow)/2), 23);
        place_cloud(CLOUD_FIRE, where, durat, &you);
        mpr("The fire roars!");
    }
    noisy(2, where);

    return SPRET_SUCCESS;
}

// Assumes beem.range has already been set. -cao
spret_type stinking_cloud(int pow, bolt &beem, bool fail)
{
    beem.name        = "stinking cloud";
    beem.colour      = GREEN;
    beem.damage      = dice_def(1, 0);
    beem.hit         = 20;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
    beem.flavour     = BEAM_POTION_MEPHITIC;
    beem.ench_power  = pow;
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU;
    beem.is_beam     = false;
    beem.is_explosion = true;
    beem.aux_source.clear();

    // Fire tracer.
    beem.source            = you.pos();
    beem.can_see_invis     = you.can_see_invisible();
    beem.smart_monster     = true;
    beem.attitude          = ATT_FRIENDLY;
    beem.friend_info.count = 0;
    beem.is_tracer         = true;
    beem.fire();

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies.
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();

    // Really fire.
    beem.is_tracer = false;
    beem.fire();

    return SPRET_SUCCESS;
}

spret_type cast_big_c(int pow, cloud_type cty, const actor *caster, bolt &beam,
                      bool fail)
{
    if (distance2(beam.target, you.pos()) > dist_range(beam.range)
        || !in_bounds(beam.target))
    {
        mpr("That is beyond the maximum range.");
        return SPRET_ABORT;
    }

    if (cell_is_solid(beam.target))
    {
        mpr("You can't place clouds on a wall.");
        return SPRET_ABORT;
    }

    //XXX: there should be a better way to specify beam cloud types
    switch(cty)
    {
        case CLOUD_POISON:
            beam.flavour = BEAM_POISON;
            beam.name = "blast of poison";
            break;
        case CLOUD_HOLY_FLAMES:
            beam.flavour = BEAM_HOLY;
            beam.origin_spell = SPELL_HOLY_BREATH;
            break;
        case CLOUD_COLD:
            beam.flavour = BEAM_COLD;
            beam.name = "freezing blast";
            break;
        default:
            mpr("That kind of cloud doesn't exist!");
            return SPRET_ABORT;
    }

    beam.thrower           = KILL_YOU;
    beam.hit               = AUTOMATIC_HIT;
    beam.damage            = dice_def(42, 1); // just a convenient non-zero
    beam.is_big_cloud      = true;
    beam.is_tracer         = true;
    beam.use_target_as_pos = true;
    beam.affect_endpoint();
    if (beam.beam_cancelled)
        return SPRET_ABORT;

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(2, beam.target);
    return SPRET_SUCCESS;
}

static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent, int colour,
                                string name, string tile, int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, colour, name, tile, excl_rad);

    return 1;
}

void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate,
               int colour, string name, string tile)
{
    // The starting point _may_ be a place no cloud can be placed on.
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, colour, name, tile,
                     -1);
}

spret_type cast_ring_of_flames(int power, bool fail)
{
    // You shouldn't be able to cast this in the rain. {due}
    if (in_what_cloud(CLOUD_RAIN))
    {
        mpr("Your spell sizzles in the rain.");
        return SPRET_ABORT;
    }

    fail_check();
    you.increase_duration(DUR_FIRE_SHIELD,
                          5 + (power / 10) + (random2(power) / 5), 50,
                          "The air around you leaps into flame!");
    manage_fire_shield(1);
    return SPRET_SUCCESS;
}

void manage_fire_shield(int delay)
{
    ASSERT(you.duration[DUR_FIRE_SHIELD]);

    int old_dur = you.duration[DUR_FIRE_SHIELD];

    you.duration[DUR_FIRE_SHIELD]-= delay;
    if (you.duration[DUR_FIRE_SHIELD] < 0)
        you.duration[DUR_FIRE_SHIELD] = 0;

    // Remove fire clouds on top of you
    if (env.cgrid(you.pos()) != EMPTY_CLOUD
        && env.cloud[env.cgrid(you.pos())].type == CLOUD_FIRE)
    {
        delete_cloud_at(you.pos());
    }

    if (!you.duration[DUR_FIRE_SHIELD])
    {
        mprf(MSGCH_DURATION, "Your ring of flames gutters out.");
        return;
    }

    int threshold = get_expiration_threshold(DUR_FIRE_SHIELD);


    if (old_dur > threshold && you.duration[DUR_FIRE_SHIELD] < threshold)
        mprf(MSGCH_WARN, "Your ring of flames is guttering out.");

    // Place fire clouds all around you
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!cell_is_solid(*ai) && env.cgrid(*ai) == EMPTY_CLOUD)
            place_cloud(CLOUD_FIRE, *ai, 1 + random2(6), &you);
}

spret_type cast_corpse_rot(bool fail)
{
    if (!you.res_rotting())
    {
        for (stack_iterator si(you.pos()); si; ++si)
        {
            if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
            {
                if (!yesno(("Really cast Corpse Rot while standing on " + si->name(DESC_A) + "?").c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return SPRET_ABORT;
                }
                break;
            }
        }
    }
    fail_check();
    corpse_rot(&you);
    return SPRET_SUCCESS;
}

void corpse_rot(actor* caster)
{
    for (radius_iterator ri(caster->pos(), 6, C_ROUND, LOS_NO_TRANS); ri; ++ri)
    {
        if (!is_sanctuary(*ri) && env.cgrid(*ri) == EMPTY_CLOUD)
            for (stack_iterator si(*ri); si; ++si)
                if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
                {
                    // Found a corpse.  Skeletonise it if possible.
                    if (!mons_skeleton(si->mon_type))
                    {
                        item_was_destroyed(*si);
                        destroy_item(si->index());
                    }
                    else
                        turn_corpse_into_skeleton(*si);

                    place_cloud(CLOUD_MIASMA, *ri, 4+random2avg(16, 3),caster);

                    // Don't look for more corpses here.
                    break;
                }
    }

    if (you.can_smell() && you.can_see(caster))
        mpr("You smell decay.");

    // Should make zombies decay into skeletons?
}

// Returns a vector of cloud types created by this potion type.
// FIXME: Heavily duplicated code.
static std::vector<int> _get_evaporate_result(int potion)
{
    std::vector <int> beams;
    bool random_potion = false;
    switch (potion)
    {
    case POT_STRONG_POISON:
    case POT_POISON:
        beams.push_back(BEAM_POTION_POISON);
        break;

    case POT_DEGENERATION:
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_DECAY:
        beams.push_back(BEAM_POTION_MIASMA);
        break;

    case POT_PARALYSIS:
    case POT_CONFUSION:
    case POT_SLOWING:
        beams.push_back(BEAM_POTION_MEPHITIC);
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        beams.push_back(BEAM_POTION_MEPHITIC);
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_STEAM);
        break;

    case POT_MUTATION:
        beams.push_back(BEAM_POTION_MUTAGENIC);
        // deliberate fall-through
    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_MIASMA);
        random_potion = true;
        break;

    default:
        beams.push_back(BEAM_POTION_FIRE);
        beams.push_back(BEAM_POTION_MEPHITIC);
        beams.push_back(BEAM_POTION_COLD);
        beams.push_back(BEAM_POTION_POISON);
        beams.push_back(BEAM_POTION_BLUE_SMOKE);
        beams.push_back(BEAM_POTION_STEAM);
        random_potion = true;
        break;
    }

    std::vector<int> clouds;
    for (unsigned int k = 0; k < beams.size(); ++k)
        clouds.push_back(beam2cloud((beam_type) beams[k]));

    if (random_potion)
    {
        // handled in beam.cc
        clouds.push_back(CLOUD_FIRE);
        clouds.push_back(CLOUD_MEPHITIC);
        clouds.push_back(CLOUD_COLD);
        clouds.push_back(CLOUD_POISON);
        clouds.push_back(CLOUD_BLUE_SMOKE);
        clouds.push_back(CLOUD_STEAM);
    }

    return clouds;
}

// Returns a comma-separated list of all cloud types potentially created
// by this potion type. Doesn't respect the different probabilities.
std::string get_evaporate_result_list(int potion)
{
    std::vector<int> clouds = _get_evaporate_result(potion);
    std::sort(clouds.begin(), clouds.end());

    std::vector<std::string> clouds_list;

    int old_cloud = -1;
    for (unsigned int k = 0; k < clouds.size(); ++k)
    {
        const int new_cloud = clouds[k];
        if (new_cloud == old_cloud)
            continue;

        // This relies on all smoke types being handled as blue.
        if (new_cloud == CLOUD_BLUE_SMOKE)
            clouds_list.push_back("coloured smoke");
        else
            clouds_list.push_back(cloud_type_name((cloud_type) new_cloud));

        old_cloud = new_cloud;
    }

    return comma_separated_line(clouds_list.begin(), clouds_list.end(),
                                " or ", ", ");
}


// Assumes beem.range is already set -cao
spret_type cast_evaporate(int pow, bolt& beem, int pot_idx, bool fail)
{
    ASSERT(you.inv[pot_idx].base_type == OBJ_POTIONS);
    item_def& potion = you.inv[pot_idx];

    beem.name        = "potion";
    beem.colour      = potion.colour;
    beem.glyph       = dchar_glyph(DCHAR_FIRED_FLASK);
    beem.beam_source = MHITYOU;
    beem.thrower     = KILL_YOU_MISSILE;
    beem.is_beam     = false;
    beem.aux_source.clear();

    beem.auto_hit   = true;
    beem.damage     = dice_def(1, 0);  // no damage, just producing clouds
    beem.ench_power = pow;               // used for duration only?
    beem.is_explosion = true;

    beem.flavour    = BEAM_POTION_MEPHITIC;
    beam_type tracer_flavour = BEAM_MMISSILE;

    switch (potion.sub_type)
    {
    case POT_STRONG_POISON:
        beem.ench_power *= 2;
        // deliberate fall-through
    case POT_POISON:
        beem.flavour   = BEAM_POTION_POISON;
        tracer_flavour = BEAM_POISON;
        break;

    case POT_DEGENERATION:
        beem.effect_known = false;
        beem.flavour   = (coinflip() ? BEAM_POTION_POISON : BEAM_POTION_MIASMA);
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_DECAY:
        beem.flavour   = BEAM_POTION_MIASMA;
        tracer_flavour = BEAM_MIASMA;
        beem.ench_power *= 2;
        break;

    case POT_PARALYSIS:
        beem.ench_power *= 2;
        // fall through
    case POT_CONFUSION:
    case POT_SLOWING:
        tracer_flavour = beem.flavour = BEAM_POTION_MEPHITIC;
        break;

    case POT_WATER:
    case POT_PORRIDGE:
        tracer_flavour = beem.flavour = BEAM_POTION_STEAM;
        break;

    case POT_BLOOD:
    case POT_BLOOD_COAGULATED:
        if (one_chance_in(3))
            break; // stinking cloud
        // deliberate fall through
    case POT_BERSERK_RAGE:
        beem.effect_known = false;
        beem.flavour = (coinflip() ? BEAM_POTION_FIRE : BEAM_POTION_STEAM);
        if (potion.sub_type == POT_BERSERK_RAGE)
            tracer_flavour = BEAM_FIRE;
        else
            tracer_flavour = BEAM_RANDOM;
        break;

    case POT_MUTATION:
        // Maybe we'll get a mutagenic cloud.
        if (coinflip())
        {
            beem.effect_known = true;
            tracer_flavour = beem.flavour = BEAM_POTION_MUTAGENIC;
            break;
        }
        // if not, deliberate fall through for something random

    case POT_GAIN_STRENGTH:
    case POT_GAIN_DEXTERITY:
    case POT_GAIN_INTELLIGENCE:
    case POT_EXPERIENCE:
    case POT_MAGIC:
        beem.effect_known = false;
        switch (random2(5))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;   break;
        case 1:   beem.flavour = BEAM_POTION_COLD;   break;
        case 2:   beem.flavour = BEAM_POTION_POISON; break;
        case 3:   beem.flavour = BEAM_POTION_MIASMA; break;
        default:  beem.flavour = BEAM_POTION_RANDOM; break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;

    default:
        beem.effect_known = false;
        switch (random2(12))
        {
        case 0:   beem.flavour = BEAM_POTION_FIRE;            break;
        case 1:   beem.flavour = BEAM_POTION_MEPHITIC;        break;
        case 2:   beem.flavour = BEAM_POTION_COLD;            break;
        case 3:   beem.flavour = BEAM_POTION_POISON;          break;
        case 4:   beem.flavour = BEAM_POTION_RANDOM;          break;
        case 5:   beem.flavour = BEAM_POTION_BLUE_SMOKE;      break;
        case 6:   beem.flavour = BEAM_POTION_BLACK_SMOKE;     break;
        default:  beem.flavour = BEAM_POTION_STEAM;           break;
        }
        tracer_flavour = BEAM_RANDOM;
        break;
    }

    // Fire tracer. FIXME: use player_tracer() here!
    beem.source         = you.pos();
    beem.can_see_invis  = you.can_see_invisible();
    beem.smart_monster  = true;
    beem.attitude       = ATT_FRIENDLY;
    beem.beam_cancelled = false;
    beem.is_tracer      = true;
    beem.friend_info.reset();

    beam_type real_flavour = beem.flavour;
    beem.flavour           = tracer_flavour;
    beem.fire();

    if (beem.beam_cancelled)
    {
        // We don't want to fire through friendlies or at ourselves.
        canned_msg(MSG_OK);
        return SPRET_ABORT;
    }

    fail_check();
    // Really fire.
    beem.flavour = real_flavour;
    beem.is_tracer = false;
    beem.fire();

    // Use up a potion.
    if (is_blood_potion(potion))
        remove_oldest_blood_potion(potion);

    dec_inv_item_quantity(pot_idx, 1);

    return SPRET_SUCCESS;
}

int holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || env.cgrid(*ai) != EMPTY_CLOUD
            || cell_is_solid(*ai)
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY_FLAMES, *ai, caster->hit_dice * 5, caster);

        cloud_count++;
    }

    return cloud_count;
}

struct dist2_sorter
{
    coord_def pos;
    bool operator()(const actor* a, const actor* b)
    {
        return distance2(a->pos(), pos) > distance2(b->pos(), pos);
    }
};

static bool _safe_cloud_spot(const monster* mon, coord_def p)
{
    if (cell_is_solid(p) || env.cgrid(p) != EMPTY_CLOUD)
        return false;

    if (actor_at(p) && mons_aligned(mon, actor_at(p)))
        return false;

    return true;
}

void apply_control_winds(const monster* mon)
{
    vector<int> cloud_list;
    for (distance_iterator di(mon->pos(), true, false, LOS_RADIUS); di; ++di)
    {
        if (env.cgrid(*di) != EMPTY_CLOUD
            && cell_see_cell(mon->pos(), *di, LOS_SOLID)
            && (di.radius() < 6 || env.cloud[env.cgrid(*di)].type == CLOUD_FOREST_FIRE
                                || (actor_at(*di) && mons_aligned(mon, actor_at(*di)))))
        {
            cloud_list.push_back(env.cgrid(*di));
        }
    }

    bolt wind_beam;
    wind_beam.hit = AUTOMATIC_HIT;
    wind_beam.is_beam = true;
    wind_beam.affects_nothing = true;
    wind_beam.source = mon->pos();
    wind_beam.range = LOS_RADIUS;
    wind_beam.is_tracer = true;

    for (int i = cloud_list.size() - 1; i >= 0; --i)
    {
        cloud_struct* cl = &env.cloud[cloud_list[i]];
        if (cl->type == CLOUD_FOREST_FIRE)
        {
            if (you.see_cell(cl->pos))
                mpr("The forest fire is smothered by the winds.");
            delete_cloud(cloud_list[i]);
            continue;
        }

        // Leave clouds engulfing hostiles alone
        if (actor_at(cl->pos) && !mons_aligned(actor_at(cl->pos), mon))
            continue;

        bool pushed = false;

        coord_def newpos;
        if (cl->pos != mon->pos())
        {
            wind_beam.target = cl->pos;
            wind_beam.fire();
            for (unsigned int j = 0; j < wind_beam.path_taken.size() - 1; ++j)
            {
                if (env.cgrid(wind_beam.path_taken[j]) == cloud_list[i])
                {
                    newpos = wind_beam.path_taken[j+1];
                    if (_safe_cloud_spot(mon, newpos))
                    {
                        swap_clouds(newpos, wind_beam.path_taken[j]);
                        pushed = true;
                        break;
                    }
                }
            }
        }

        if (!pushed)
        {
            for (distance_iterator di(cl->pos, true, true, 1); di; ++di)
            {
                if ((newpos.origin() || adjacent(*di, newpos))
                    && di->distance_from(mon->pos())
                        == (cl->pos.distance_from(mon->pos()) + 1)
                    && _safe_cloud_spot(mon, *di))
                {
                    swap_clouds(*di, cl->pos);
                    pushed = true;
                    break;
                }
            }

            if (!pushed && actor_at(cl->pos) && mons_aligned(mon, actor_at(cl->pos)))
            {
                env.cloud[cloud_list[i]].decay =
                        env.cloud[cloud_list[i]].decay / 2 - 20;
            }
        }
    }

    // Now give a ranged accuracy boost to nearby allies
    for (monster_near_iterator mi(mon, LOS_NO_TRANS); mi; ++mi)
    {
        if (distance2(mon->pos(), mi->pos()) >= 33 || !mons_aligned(mon, *mi))
            continue;

        if (!mi->has_ench(ENCH_WIND_AIDED))
            mi->add_ench(mon_enchant(ENCH_WIND_AIDED, 1, mon, 20));
        else
        {
            mon_enchant aid = mi->get_ench(ENCH_WIND_AIDED);
            aid.duration = 20;
            mi->update_ench(aid);
        }
    }
}
