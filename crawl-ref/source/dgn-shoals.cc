#include "AppHdr.h"

#include "act-iter.h"
#include "cio.h"
#include "colour.h"
#include "coordit.h"
#include "dungeon.h"
#include "dgn-shoals.h"
#include "dgn-height.h"
#include "env.h"
#include "flood_find.h"
#include "fprop.h"
#include "items.h"
#include "itemprop.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-place.h"
#include "mon-util.h"
#include "random.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"

#include <algorithm>
#include <vector>
#include <cmath>

static const char *PROPS_SHOALS_TIDE_KEY = "shoals-tide-height";
static const char *PROPS_SHOALS_TIDE_VEL = "shoals-tide-velocity";
static const char *PROPS_SHOALS_TIDE_UPDATE_TIME = "shoals-tide-update-time";

static dgn_island_plan _shoals_islands;

static const int SHOALS_ISLAND_COLLIDE_DIST2 = 5 * 5;

// The raw tide height / TIDE_MULTIPLIER is the actual tide height. The higher
// the tide multiplier, the slower the tide advances and recedes. A multiplier
// of X implies that the tide will advance visibly about once in X turns.
static int TIDE_MULTIPLIER = 30;

static int LOW_TIDE = -18 * TIDE_MULTIPLIER;
static int HIGH_TIDE = 25 * TIDE_MULTIPLIER;

// The highest a tide can be called by a tide caller such as Ilsuiw.
static const int HIGH_CALLED_TIDE = 50;
static const int TIDE_DECEL_MARGIN = 8;
static const int PEAK_TIDE_VELOCITY = 2;
static const int CALL_TIDE_VELOCITY = 21;

// The area around the user of a call tide spell that is subject to
// local tide elevation.
static const int TIDE_CALL_RADIUS = 8;
static const int MAX_SHOAL_PLANTS = 180;

static const int _shoals_margin = 6;

enum shoals_height_thresholds
{
    SHT_UNDEFINED = -10000,
    SHT_STONE = 400,
    SHT_ROCK  = 135,
    SHT_FLOOR = 0,
    SHT_SHALLOW_WATER = -30,
};

enum tide_direction
{
    TIDE_RISING,
    TIDE_FALLING,
};

static tide_direction _shoals_tide_direction;
static monster* tide_caller = NULL;
static coord_def tide_caller_pos;
static int tide_called_turns = 0;
static int tide_called_peak = 0;
static int shoals_plant_quota = 0;

static dungeon_feature_type _shoals_feature_by_height(int height)
{
    return height >= SHT_STONE ? DNGN_STONE_WALL :
        height >= SHT_ROCK ? DNGN_ROCK_WALL :
        height >= SHT_FLOOR ? DNGN_FLOOR :
        height >= SHT_SHALLOW_WATER ? DNGN_SHALLOW_WATER
        : DNGN_DEEP_WATER;
}

static dungeon_feature_type _shoals_feature_at(const coord_def &c)
{
    const int height = dgn_height_at(c);
    return _shoals_feature_by_height(height);
}

static int _shoals_feature_height(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_STONE_WALL:
        return SHT_STONE;
    case DNGN_FLOOR:
        return SHT_FLOOR;
    case DNGN_SHALLOW_WATER:
        return SHT_SHALLOW_WATER;
    case DNGN_DEEP_WATER:
        return SHT_SHALLOW_WATER - 1;
    default:
        return feat_is_solid(feat) ? SHT_ROCK : SHT_FLOOR;
    }
}

static int _shoals_feature_sequence_number(dungeon_feature_type feat)
{
    switch (feat)
    {
    case DNGN_SHALLOW_WATER:
        return 1;
    case DNGN_DEEP_WATER:
        return 2;
    default:
    case DNGN_FLOOR:
        return 0;
    }
}

// Returns true if the given feature can be affected by Shoals tides.
static inline bool _shoals_tide_susceptible_feat(dungeon_feature_type feat)
{
    return feat_is_water(feat) || feat == DNGN_FLOOR;
}

// Return true if tide effects can propagate through this square.
// NOTE: uses RNG!
static inline bool _shoals_tide_passable_feat(dungeon_feature_type feat)
{
    return feat_is_watery(feat)
           // The Shoals tide can sometimes lap past the doorways of rooms
           // near the water. Note that the actual probability of the tide
           // getting through a doorway is this probability * 0.5 --
           // see _shoals_apply_tide.
           || feat == DNGN_OPEN_DOOR
           || feat_is_closed_door(feat) && one_chance_in(3);
}

static void _shoals_init_heights()
{
    dgn_initialise_heightmap(SHT_SHALLOW_WATER - 3);
}

static dgn_island_plan _shoals_island_plan()
{
    dgn_island_plan plan;
    plan.level_border_depth = _shoals_margin;
    plan.n_aux_centres = int_range(0, 3);
    plan.aux_centre_offset_range = int_range(2, 10);

    plan.atoll_roll = 10;
    plan.island_separation_dist2 = SHOALS_ISLAND_COLLIDE_DIST2;

    plan.n_island_centre_delta_points = int_range(50, 60);
    plan.island_centre_radius_range = int_range(3, 10);
    plan.island_centre_point_height_increment = int_range(80, 110);

    plan.n_island_aux_delta_points = int_range(25, 45);
    plan.island_aux_radius_range = int_range(2, 7);
    plan.island_aux_point_height_increment = int_range(50, 65);

    return plan;
}

static void _shoals_init_islands(int depth)
{
    const int nislands = 20 - depth * 2;
    _shoals_islands = _shoals_island_plan();
    _shoals_islands.build(nislands);
}

static void _shoals_build_cliff()
{
    const coord_def cliffc = dgn_random_point_in_margin(_shoals_margin * 2);
    const int length = random_range(6, 15);
    const double angle = dgn_degrees_to_radians(random2(360));
    const int_range n_cliff_points(40, 60);
    const int cliff_point_radius = 3;
    const int_range cliff_height_increment(100, 130);

    for (int i = 0; i < length; i += 3)
    {
        const int distance = i - length / 2;
        coord_def place =
            cliffc + coord_def(static_cast<int>(distance * cos(angle)),
                               static_cast<int>(distance * sin(angle)));
        const coord_def fuzz = coord_def(random_range(-2, 2),
                                         random_range(-2, 2));
        place += fuzz;
        dgn_island_centred_at(place, resolve_range(n_cliff_points),
                              cliff_point_radius, cliff_height_increment,
                              _shoals_margin);
    }
}

static void _shoals_cliffs()
{
    const int ncliffs = random_range(0, 6, 2);
    for (int i = 0; i < ncliffs; ++i)
        _shoals_build_cliff();
}

static void _shoals_smooth_water()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        dgn_smooth_height_at(*ri, 1, SHT_SHALLOW_WATER - 1);
}

static void _shoals_apply_level()
{
    for (rectangle_iterator ri(1); ri; ++ri)
        grd(*ri) = _shoals_feature_at(*ri);
}

static void _shoals_postbuild_apply_level()
{
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (!map_masked(*ri, MMT_VAULT))
        {
            const dungeon_feature_type feat = grd(*ri);
            if (feat_is_water(feat) || feat == DNGN_ROCK_WALL
                || feat == DNGN_STONE_WALL || feat == DNGN_FLOOR)
            {
                grd(*ri) = _shoals_feature_at(*ri);
            }
        }
    }
}

// Returns all points in deep water with an adjacent square in shallow water.
static vector<coord_def> _shoals_water_depth_change_points()
{
    vector<coord_def> points;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        coord_def c(*ri);
        if (grd(c) == DNGN_DEEP_WATER
            && dgn_has_adjacent_feat(c, DNGN_SHALLOW_WATER))
        {
            points.push_back(c);
        }
    }
    return points;
}

static inline void _shoals_deepen_water_at(coord_def p, int distance)
{
    dgn_height_at(p) -= distance * 7;
}

static void _shoals_deepen_water()
{
    vector<coord_def> pages[2];
    int current_page = 0;
    pages[current_page] = _shoals_water_depth_change_points();
    FixedArray<bool, GXM, GYM> seen_points(false);

    for (int i = 0, size = pages[current_page].size(); i < size; ++i)
        seen_points(pages[current_page][i]) = true;

    int distance = 0;
    while (!pages[current_page].empty())
    {
        const int next_page = !current_page;
        vector<coord_def> &cpage(pages[current_page]);
        vector<coord_def> &npage(pages[next_page]);
        for (int i = 0, size = cpage.size(); i < size; ++i)
        {
            coord_def c(cpage[i]);
            if (distance)
                _shoals_deepen_water_at(c, distance);

            for (adjacent_iterator ai(c); ai; ++ai)
            {
                const coord_def adj(*ai);
                if (!seen_points(adj)
                    && (adj - c).abs() == 1
                    && grd(adj) == DNGN_DEEP_WATER)
                {
                    npage.push_back(adj);
                    seen_points(adj) = true;
                }
            }
        }
        cpage.clear();
        current_page = next_page;
        distance++;
    }
}

static void _shoals_furniture(int margin)
{
    dgn_place_stone_stairs();
}

static void _shoals_deepen_edges()
{
    const int edge = 1;
    const int deepen_by = 1000;
    // Water of the edge of the screen is too deep to be exposed by tides.
    for (int y = 1; y < GYM - 2; ++y)
    {
        for (int x = 1; x <= edge; ++x)
        {
            dgn_height_at(coord_def(x, y)) -= deepen_by;
            dgn_height_at(coord_def(GXM - 1 - x, y)) -= deepen_by;
        }
    }
    for (int x = 1; x < GXM - 2; ++x)
    {
        for (int y = 1; y <= edge; ++y)
        {
            dgn_height_at(coord_def(x, y)) -= deepen_by;
            dgn_height_at(coord_def(x, GYM - 1 - y)) -= deepen_by;
        }
    }
}

static int _shoals_contiguous_feature_flood(
    FixedArray<short, GXM, GYM> &rmap,
    coord_def c,
    dungeon_feature_type feat,
    int nregion,
    int size_limit)
{
    vector<coord_def> visit;
    visit.push_back(c);
    int npoints = 1;
    for (size_t i = 0; i < visit.size() && npoints < size_limit; ++i)
    {
        const coord_def p(visit[i]);
        rmap(p) = nregion;

        if (npoints < size_limit)
        {
            for (adjacent_iterator ai(p); ai && npoints < size_limit; ++ai)
            {
                const coord_def adj(*ai);
                if (in_bounds(adj) && !rmap(adj) && grd(adj) == feat
                    && !map_masked(adj, MMT_VAULT))
                {
                    rmap(adj) = nregion;
                    visit.push_back(adj);
                    ++npoints;
                }
            }
        }
    }
    return npoints;
}

static coord_def _shoals_region_center(
    FixedArray<short, GXM, GYM> &rmap,
    coord_def c)
{
    const int nregion(rmap(c));
    int nseen = 0;

    double cx = 0.0, cy = 0.0;
    vector<coord_def> visit;
    visit.push_back(c);
    FixedArray<bool, GXM, GYM> visited(false);
    for (size_t i = 0; i < visit.size(); ++i)
    {
        const coord_def p(visit[i]);
        visited(p) = true;

        ++nseen;
        if (nseen == 1)
        {
            cx = p.x;
            cy = p.y;
        }
        else
        {
            cx = (cx * (nseen - 1) + p.x) / nseen;
            cy = (cy * (nseen - 1) + p.y) / nseen;
        }

        for (adjacent_iterator ai(p); ai; ++ai)
        {
            const coord_def adj(*ai);
            if (in_bounds(adj) && !visited(adj) && rmap(adj) == nregion)
            {
                visited(adj) = true;
                visit.push_back(adj);
            }
        }
    }

    const coord_def cgravity(static_cast<int>(cx), static_cast<int>(cy));
    coord_def closest_to_center;
    int closest_distance = 0;
    for (int i = 0, size = visit.size(); i < size; ++i)
    {
        const coord_def p(visit[i]);
        const int dist2 = (p - cgravity).abs();
        if (closest_to_center.origin() || closest_distance > dist2)
        {
            closest_to_center = p;
            closest_distance = dist2;
        }
    }
    return closest_to_center;
}

struct weighted_region
{
    int weight;
    coord_def pos;

    weighted_region(int _weight, coord_def _pos) : weight(_weight), pos(_pos)
    {
    }
};

static vector<weighted_region>
_shoals_point_feat_cluster(dungeon_feature_type feat,
                           const int wanted_count,
                           grid_short &region_map)
{
    vector<weighted_region> regions;
    int region = 1;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        coord_def c(*ri);
        if (!region_map(c) && grd(c) == feat
            && !map_masked(c, MMT_VAULT))
        {
            const int featcount =
                _shoals_contiguous_feature_flood(region_map,
                                                 c,
                                                 feat,
                                                 region++,
                                                 wanted_count * 3 / 2);
            if (featcount >= wanted_count)
                regions.push_back(weighted_region(featcount, c));
        }
    }
    return regions;
}

static coord_def _shoals_pick_region(
    grid_short &region_map,
    const vector<weighted_region> &regions)
{
    if (regions.empty())
        return coord_def();
    return _shoals_region_center(region_map,
                                 regions[random2(regions.size())].pos);
}

static void _shoals_make_plant_at(coord_def p)
{
    if (shoals_plant_quota > 0)
    {
        // [ds] Why is hostile_at() saddled with unnecessary parameters
        // related to summoning?
        mons_place(mgen_data::hostile_at(MONS_PLANT, "", false, 0, 0, p));
        --shoals_plant_quota;
    }
}

static bool _shoals_plantworthy_feat(dungeon_feature_type feat)
{
    return feat == DNGN_SHALLOW_WATER || feat == DNGN_FLOOR;
}

static void _shoals_make_plant_near(coord_def c, int radius,
                                    dungeon_feature_type preferred_feat,
                                    grid_bool *verboten)
{
    if (shoals_plant_quota <= 0)
        return;

    const int ntries = 5;
    for (int i = 0; i < ntries; ++i)
    {
        const coord_def plant_place(
            dgn_random_point_from(c, random2(1 + radius), _shoals_margin));
        if (!plant_place.origin()
            && !monster_at(plant_place)
            && !map_masked(plant_place, MMT_VAULT))
        {
            const dungeon_feature_type feat(grd(plant_place));
            if (_shoals_plantworthy_feat(feat)
                && (feat == preferred_feat || coinflip())
                && (!verboten || !(*verboten)(plant_place)))
            {
                _shoals_make_plant_at(plant_place);
                return;
            }
        }
    }
}

static void _shoals_plant_cluster(coord_def c, int nplants, int radius,
                                  dungeon_feature_type favoured_feat,
                                  grid_bool *verboten)
{
    for (int i = 0; i < nplants; ++i)
        _shoals_make_plant_near(c, radius, favoured_feat, verboten);
}

static void _shoals_plant_supercluster(coord_def c,
                                       dungeon_feature_type favoured_feat,
                                       grid_bool *verboten = NULL)
{
    _shoals_plant_cluster(c, random_range(10, 17, 2),
                          random_range(3, 9), favoured_feat,
                          verboten);

    const int nadditional_clusters(max(0, random_range(-1, 4, 2)));
    for (int i = 0; i < nadditional_clusters; ++i)
    {
        const coord_def satellite(
            dgn_random_point_from(c, random_range(2, 12), _shoals_margin));
        if (!satellite.origin())
            _shoals_plant_cluster(satellite, random_range(5, 12, 2),
                                  random_range(2, 7),
                                  favoured_feat,
                                  verboten);
    }
}

static void _shoals_generate_water_plants(coord_def mangrove_central)
{
    if (!mangrove_central.origin())
        _shoals_plant_supercluster(mangrove_central, DNGN_SHALLOW_WATER);
}

struct coord_dbl
{
    double x, y;

    coord_dbl(double _x, double _y) : x(_x), y(_y) { }
    coord_dbl operator + (const coord_dbl &o) const
    {
        return coord_dbl(x + o.x, y + o.y);
    }
    coord_dbl &operator += (const coord_dbl &o)
    {
        x += o.x;
        y += o.y;
        return *this;
    }
};

static coord_def _int_coord(const coord_dbl &c)
{
    return coord_def(static_cast<int>(c.x), static_cast<int>(c.y));
}

static vector<coord_def> _shoals_windshadows(grid_bool &windy)
{
    const int wind_angle_degrees = random2(360);
    const double wind_angle(dgn_degrees_to_radians(wind_angle_degrees));
    const coord_dbl wi(cos(wind_angle), sin(wind_angle));
    const double epsilon = 1e-5;

    vector<coord_dbl> wind_points;
    if (wi.x > epsilon || wi.x < -epsilon)
    {
        for (int y = 1; y < GYM - 1; ++y)
            wind_points.push_back(coord_dbl(wi.x > epsilon ? 1 : GXM - 2, y));
    }
    if (wi.y > epsilon || wi.y < -epsilon)
    {
        for (int x = 1; x < GXM - 1; ++x)
            wind_points.push_back(coord_dbl(x, wi.y > epsilon ? 1 : GYM - 2));
    }

    for (size_t i = 0; i < wind_points.size(); ++i)
    {
        const coord_def here(_int_coord(wind_points[i]));
        windy(here) = true;

        coord_dbl next = wind_points[i] + wi;
        while (_int_coord(next) == here)
            next += wi;

        const coord_def nextp(_int_coord(next));
        if (in_bounds(nextp) && !windy(nextp) && !cell_is_solid(nextp))
        {
            windy(nextp) = true;
            wind_points.push_back(next);
        }
    }

    // To avoid plants cropping up inside vaults, mark everything inside
    // vaults as "windy".
    for (rectangle_iterator ri(1); ri; ++ri)
        if (map_masked(*ri, MMT_VAULT))
            windy(*ri) = true;

    // Now we know the places in the wind shadow:
    vector<coord_def> wind_shadows;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def p(*ri);
        if (!windy(p) && grd(p) == DNGN_FLOOR
            && (dgn_has_adjacent_feat(p, DNGN_STONE_WALL)
                || dgn_has_adjacent_feat(p, DNGN_ROCK_WALL)))
        {
            wind_shadows.push_back(p);
        }
    }
    return wind_shadows;
}

static void _shoals_generate_wind_sheltered_plants(vector<coord_def> &places,
                                                   grid_bool &windy)
{
    if (places.empty())
        return;

    const int chosen = random2(places.size());
    const coord_def spot = places[random2(places.size())];
    places.erase(places.begin() + chosen);

    _shoals_plant_supercluster(spot, DNGN_FLOOR, &windy);
}

void dgn_shoals_generate_flora()
{
    // Water clusters are groups of plants clustered near the water.
    // Wind clusters are groups of plants clustered in wind shadow --
    // possibly because they can grow better without being exposed to the
    // strong winds of the Shoals.
    //
    // Yeah, the strong winds aren't there yet, but they could be!
    //
    const int n_water_clusters = max(0, random_range(-1, 6, 2));
    const int n_wind_clusters = max(0, random_range(-2, 2, 2));

    shoals_plant_quota = MAX_SHOAL_PLANTS;

    if (n_water_clusters)
    {
        grid_short region_map(0);
        vector<weighted_region> regions(
            _shoals_point_feat_cluster(DNGN_SHALLOW_WATER, 6, region_map));

        for (int i = 0; i < n_water_clusters; ++i)
        {
            const coord_def p(_shoals_pick_region(region_map, regions));
            _shoals_generate_water_plants(p);
        }
    }

    if (n_wind_clusters)
    {
        grid_bool windy(false);
        vector<coord_def> wind_shadows = _shoals_windshadows(windy);
        for (int i = 0; i < n_wind_clusters; ++i)
            _shoals_generate_wind_sheltered_plants(wind_shadows, windy);
    }
}

void dgn_build_shoals_level()
{
    env.level_build_method += make_stringf(" shoals+ [%d]", you.depth);
    env.level_layout_types.insert("shoals");

    const int shoals_depth = you.depth - 1;
    dgn_replace_area(0, 0, GXM-1, GYM-1, DNGN_ROCK_WALL, DNGN_OPEN_SEA);
    _shoals_init_heights();
    _shoals_init_islands(shoals_depth);
    _shoals_cliffs();
    dgn_smooth_heights();
    _shoals_apply_level();
    _shoals_deepen_water();
    _shoals_deepen_edges();
    _shoals_smooth_water();
    _shoals_furniture(_shoals_margin);
}

// Search the map for vaults and set the terrain heights for features
// in the vault to reasonable levels.
void shoals_postprocess_level()
{
    if (!player_in_branch(BRANCH_SHOALS) || !env.heightmap.get())
        return;

    for (rectangle_iterator ri(1); ri; ++ri)
    {
        const coord_def c(*ri);
        if (!(env.level_map_mask(c) & MMT_VAULT))
            continue;

        // Don't mess with tide immune squares at all.
        if (is_tide_immune(c))
            continue;

        const dungeon_feature_type feat(grd(c));
        if (!_shoals_tide_susceptible_feat(feat) && !feat_is_solid(feat))
            continue;

        const dungeon_feature_type expected_feat(_shoals_feature_at(c));
        // It would be nice to do actual height contours within
        // vaults, but for now, keep it simple.
        if (feat != expected_feat)
            dgn_height_at(c) = _shoals_feature_height(feat);
    }

    // Apply tide now, since the tide is likely to be nonzero unless
    // this is Shoals:1
    shoals_apply_tides(0, true, false);
}

static void _shoals_clamp_height_at(const coord_def &c,
                                     int clamp_height = SHT_ROCK - 1)
{
    if (!in_bounds(c))
        return;

    if (dgn_height_at(c) > clamp_height)
        dgn_height_at(c) = clamp_height;
}

static void _shoals_connect_smooth_height_at(const coord_def &c)
{
    if (map_bounds_with_margin(c, 3))
        dgn_smooth_height_at(c, 1);
}

static void _shoals_connecting_point_smooth(const coord_def &c, int radius)
{
    for (int dy = 0; dy < radius; ++dy)
    {
        for (int dx = 0; dx < radius; ++dx)
        {
            _shoals_connect_smooth_height_at(c + coord_def(dy, dx));
            if (dy)
                _shoals_connect_smooth_height_at(c + coord_def(-dy, dx));
            if (dx)
                _shoals_connect_smooth_height_at(c + coord_def(dy, -dx));
            if (dx && dy)
                _shoals_connect_smooth_height_at(c + coord_def(-dy, -dx));
        }
    }
}

static void _shoals_connecting_point_clamp_height(
    const coord_def &c, int radius)
{
    if (!in_bounds(c))
        return;

    for (rectangle_iterator ri(c - coord_def(radius, radius),
                               c + coord_def(radius, radius)); ri; ++ri)
    {
        _shoals_clamp_height_at(*ri);
    }

    const int min_height_threshold = (SHT_SHALLOW_WATER + SHT_FLOOR) / 2;
    if (dgn_height_at(c) < min_height_threshold)
        dgn_height_at(c) = min_height_threshold;
}

bool dgn_shoals_connect_point(const coord_def &point,
                              bool (*overwriteable)(dungeon_feature_type))
{
    flood_find<feature_grid, coord_predicate> ff(env.grid, in_bounds, true,
                                                 false);
    ff.add_feat(DNGN_FLOOR);

    const coord_def target = ff.find_first_from(point, env.level_map_mask);
    if (!in_bounds(target))
        return false;

    const vector<coord_def> track =
        dgn_join_the_dots_pathfind(point, target, MMT_VAULT);

    if (!track.empty())
    {
        const int n_points = 15;
        const int radius = 4;

        for (vector<coord_def>::const_iterator i = track.begin();
             i != track.end(); ++i)
        {
            int height = 0, npoints = 0;
            for (radius_iterator ri(*i, radius, C_POINTY); ri; ++ri)
            {
                if (in_bounds(*ri))
                {
                    height += dgn_height_at(*ri);
                    ++npoints;
                }
            }

            const int target_height = SHT_FLOOR;
            if (height < target_height)
            {
                const int elevation_change = target_height - height;
                const int elevation_change_per_dot =
                    max(1, elevation_change / n_points + 1);

                dgn_island_centred_at(*i, n_points, radius,
                                      int_range(elevation_change_per_dot,
                                                elevation_change_per_dot + 20),
                                      3);
            }
        }

        for (int i = track.size() - 1; i >= 0; --i)
        {
            const coord_def &p(track[i]);
            _shoals_connecting_point_smooth(p, radius + 2);
        }
        for (int i = track.size() - 1; i >= 0; --i)
        {
            const coord_def &p(track[i]);
            _shoals_connecting_point_clamp_height(p, radius + 2);
        }

        _shoals_postbuild_apply_level();
    }
    return !track.empty();
}

static void _shoals_run_tide(int &tide, int &acc)
{
    // If someone is calling the tide, the tide velocity is clamped high.
    if (tide_caller)
        acc = CALL_TIDE_VELOCITY;
    // If there's no tide caller and our velocity is suspiciously high,
    // reset it to a falling tide at peak velocity.
    else if (abs(acc) > PEAK_TIDE_VELOCITY)
        acc = -PEAK_TIDE_VELOCITY;

    tide += acc;
    tide = max(min(tide, HIGH_TIDE), LOW_TIDE);
    if ((tide == HIGH_TIDE && acc > 0) || (tide == LOW_TIDE && acc < 0))
        acc = -acc;
    bool in_decel_margin =
        (abs(tide - HIGH_TIDE) < TIDE_DECEL_MARGIN)
        || (abs(tide - LOW_TIDE) < TIDE_DECEL_MARGIN);
    if ((abs(acc) > 1) == in_decel_margin)
        acc = in_decel_margin? acc / 2 : acc * 2;
}

static coord_def _shoals_escape_place_from(coord_def bad_place,
                                           actor *act, item_def *it)
{
    int best_height = -1000;
    coord_def chosen;
    for (adjacent_iterator ai(bad_place); ai; ++ai)
    {
        coord_def p(*ai);
        const dungeon_feature_type feat(grd(p));
        if (!feat_has_solid_floor(feat))
            continue;

        if (!act || !actor_at(p))
        {
            if (best_height == -1000 || dgn_height_at(p) > best_height)
            {
                best_height = dgn_height_at(p);
                chosen = p;
            }
        }
    }
    return chosen;
}

static void _clear_net_trapping_status(coord_def c)
{
    actor *victim = actor_at(c);
    if (!victim)
        return;

    if (victim->is_monster())
    {
        monster* mvictim = victim->as_monster();
        if (you.can_see(mvictim))
            mprf("The net is swept off %s.", mvictim->name(DESC_THE).c_str());
        mons_clear_trapping_net(mvictim);
    }
    else
    {
        mpr("The tide washes the net away!");
        clear_trapping_net();
    }
}

static bool _shoals_tide_sweep_items_clear(coord_def c)
{
    if (igrd(c) == NON_ITEM)
        return true;

    for (stack_iterator si(c); si; ++si)
    {
        // Don't abort tide entry because of items. If we can't sweep the
        // item clear here, let dungeon_terrain_changed teleport the item
        // to the nearest safe square.
        item_def &item(*si);

        // Let the tide break up stacks
        if (!item_is_rune(item) && coinflip())
            continue;

        if (item_is_stationary(item) && !one_chance_in(5))
            continue;

        const coord_def target(_shoals_escape_place_from(c, NULL, &item));
        if (!target.origin())
        {
            if (item_is_stationary(item))
                _clear_net_trapping_status(c);

            int id = si.link();
            move_item_to_grid(&id, target);
        }
    }

    return true;
}

static bool _shoals_tide_sweep_actors_clear(coord_def c)
{
    actor *victim = actor_at(c);
    if (!victim || !victim->ground_level() || victim->swimming())
        return true;

    if (victim->is_monster())
    {
        const monster* mvictim = victim->as_monster();
        // Plants and statues cannot be moved away; the tide cannot
        // drown them.
        if (mons_class_is_stationary(mvictim->type))
            return false;

        // If the monster doesn't need help, move along.
        if (monster_habitable_grid(mvictim, DNGN_DEEP_WATER))
            return true;
    }
    coord_def evacuation_point(_shoals_escape_place_from(c, victim, NULL));
    // The tide no longer drowns monster/player if it cannot push them
    // out of the way.
    if (evacuation_point.origin())
        return false;

    bool clear_net = false;
    if (victim->caught())
    {
        int net = get_trapping_net(c);
        if (net != NON_ITEM)
            clear_net = !move_item_to_grid(&net, evacuation_point);
    }
    victim->move_to_pos(evacuation_point, clear_net);
    return true;
}

// The tide will attempt to push items and non-water-capable monsters to
// adjacent squares.
static bool _shoals_tide_sweep_clear(coord_def c)
{
    return _shoals_tide_sweep_items_clear(c)
        && _shoals_tide_sweep_actors_clear(c);
}

static void _shoals_tide_wash_blood_away_at(coord_def c)
{
    env.pgrid(c) &= ~FPROP_BLOODY;
}

static dungeon_feature_type _shoals_apply_tide_feature_at(
    coord_def c,
    dungeon_feature_type feat)
{
    if (feat == DNGN_DEEP_WATER && !_shoals_tide_sweep_clear(c))
        feat = DNGN_SHALLOW_WATER;

    const dungeon_feature_type current_feat = grd(c);

    // Return DNGN_UNSEEN if the feature isn't changed.
    if (feat == current_feat)
        return DNGN_UNSEEN;

    if (crawl_state.generating_level)
        grd(c) = feat;
    else
        dungeon_terrain_changed(c, feat, true, false, true);

    return feat;
}

// Determines if the tide is rising or falling based on before and
// after features at the same square.
static tide_direction _shoals_feature_tide_height_change(
    dungeon_feature_type oldfeat,
    dungeon_feature_type newfeat)
{
    const int height_delta =
        _shoals_feature_height(newfeat) - _shoals_feature_height(oldfeat);
    // If the apparent height of the new feature is greater (floor vs water),
    // the tide is receding.
    return height_delta < 0 ? TIDE_RISING : TIDE_FALLING;
}

static void _shoals_apply_tide_at(coord_def c, int tide, bool incremental_tide)
{
    if (is_tide_immune(c))
        return;

    const int effective_height = dgn_height_at(c) - tide;
    dungeon_feature_type newfeat =
        _shoals_feature_by_height(effective_height);
    // Make sure we're not sprouting new walls.
    if (feat_is_wall(newfeat))
        newfeat = DNGN_FLOOR;
    const dungeon_feature_type oldfeat = grd(c);

    // If the tide is affecting squares incrementally, never go
    // straight from floor -> deep water or vice versa, always force
    // an intermediate shallow water step.
    if (incremental_tide)
    {
        const int oldfeat_seq = _shoals_feature_sequence_number(oldfeat);
        const int newfeat_seq = _shoals_feature_sequence_number(newfeat);
        if (abs(oldfeat_seq - newfeat_seq) >= 2)
            newfeat = DNGN_SHALLOW_WATER;
    }

    if (oldfeat == newfeat
        || (_shoals_feature_tide_height_change(oldfeat, newfeat) !=
            _shoals_tide_direction))
    {
        return;
    }

    const dungeon_feature_type final_feature =
        _shoals_apply_tide_feature_at(c, newfeat);

    if (incremental_tide
        && final_feature == DNGN_DEEP_WATER
        && c == you.pos()
        && !you.ground_level()
        && !you.permanent_flight())
    {
        mprf(MSGCH_WARN, "The tide rushes in under you.");
    }
}

static int _shoals_tide_at(coord_def pos, int base_tide)
{
    if (!tide_caller)
        return base_tide;

    pos -= tide_caller->pos();
    if (pos.abs() > sqr(TIDE_CALL_RADIUS) + 1)
        return base_tide;

    return base_tide + max(0, tide_called_peak - pos.range() * 3);
}

static vector<coord_def> _shoals_extra_tide_seeds()
{
    return find_marker_positions_by_prop("tide_seed");
}

static void _shoals_apply_tide(int tide, bool incremental_tide)
{
    vector<coord_def> pages[2];
    int current_page = 0;

    // Start from corners of the map.
    pages[current_page].push_back(coord_def(1,1));
    pages[current_page].push_back(coord_def(GXM - 2, 1));
    pages[current_page].push_back(coord_def(1, GYM - 2));
    pages[current_page].push_back(coord_def(GXM - 2, GYM - 2));

    // Find any extra seeds -- markers with tide_seed="y".
    const vector<coord_def> extra_seeds(_shoals_extra_tide_seeds());
    pages[current_page].insert(pages[current_page].end(),
                               extra_seeds.begin(), extra_seeds.end());

    FixedArray<bool, GXM, GYM> seen_points(false);

    while (!pages[current_page].empty())
    {
        int next_page = !current_page;
        vector<coord_def> &cpage(pages[current_page]);
        vector<coord_def> &npage(pages[next_page]);

        for (int i = 0, size = cpage.size(); i < size; ++i)
        {
            coord_def c(cpage[i]);
            const dungeon_feature_type herefeat(grd(c));
            const bool was_wet(_shoals_tide_passable_feat(herefeat));
            seen_points(c) = true;
            if (_shoals_tide_susceptible_feat(herefeat))
            {
                _shoals_apply_tide_at(c, _shoals_tide_at(c, tide),
                                      incremental_tide);
            }

            const bool is_wet(feat_is_water(grd(c)));

            // Only squares that were wet (before applying tide
            // effects!) can propagate the tide onwards. If the tide is
            // receding and just left the square dry, there's only a chance of
            // it continuing past and draining other squares through this one.
            if (was_wet && (is_wet || coinflip()))
            {
                for (adjacent_iterator ai(c); ai; ++ai)
                {
                    coord_def adj(*ai);
                    if (!in_bounds(adj))
                        continue;
                    if (!seen_points(adj))
                    {
                        const dungeon_feature_type feat = grd(adj);
                        if (_shoals_tide_passable_feat(feat)
                            || _shoals_tide_susceptible_feat(feat))
                        {
                            npage.push_back(adj);
                            seen_points(adj) = true;
                        }
                        // Squares that the tide cannot directly
                        // affect may still lose bloodspatter as the
                        // tide goes past.
                        else if (is_bloodcovered(adj)
                                 && one_chance_in(15))
                        {
                            _shoals_tide_wash_blood_away_at(adj);
                        }
                    }
                }
            }
        }

        cpage.clear();
        current_page = next_page;
    }
}

static void _shoals_init_tide()
{
    CrawlHashTable &props = you.props;
    if (!props.exists(PROPS_SHOALS_TIDE_KEY))
    {
        props[PROPS_SHOALS_TIDE_KEY].get_short() = 0;
        props[PROPS_SHOALS_TIDE_VEL].get_short() = PEAK_TIDE_VELOCITY;
        props[PROPS_SHOALS_TIDE_UPDATE_TIME].get_int() = 0;
    }
    if (!env.properties.exists(PROPS_SHOALS_TIDE_KEY))
        env.properties[PROPS_SHOALS_TIDE_KEY].get_short() = 0;
}

static monster* _shoals_find_tide_caller()
{
    for (monster_iterator mi; mi; ++mi)
        if (mi->has_ench(ENCH_TIDE))
            return *mi;
    return NULL;
}

void shoals_apply_tides(int turns_elapsed, bool force, bool incremental_tide)
{
    if (!player_in_branch(BRANCH_SHOALS)
        || (!turns_elapsed && !force)
        || !env.heightmap.get())
    {
        return;
    }

    CrawlHashTable &props(you.props);
    _shoals_init_tide();

    // Make sure we don't do too much catch-up if another Shoals level
    // has been updating the tide.
    if (turns_elapsed > 1)
    {
        const int last_updated_time =
            props[PROPS_SHOALS_TIDE_UPDATE_TIME].get_int();
        const int turn_delta = (you.elapsed_time - last_updated_time) / 10;
        turns_elapsed = min(turns_elapsed, turn_delta);
    }

    const int TIDE_UNIT = HIGH_TIDE - LOW_TIDE;
    // If we've been gone a long time, eliminate some unnecessary math.
    if (turns_elapsed > TIDE_UNIT * 2)
        turns_elapsed = turns_elapsed % TIDE_UNIT + TIDE_UNIT;

    unwind_var<monster* > tide_caller_unwind(tide_caller,
                                             _shoals_find_tide_caller());
    if (tide_caller)
    {
        tide_called_turns = tide_caller->props[TIDE_CALL_TURN].get_int();
        tide_called_turns = you.num_turns - tide_called_turns;
        if (tide_called_turns < 1L)
            tide_called_turns = 1L;
        tide_called_peak  = min(HIGH_CALLED_TIDE, int(tide_called_turns * 5));
        tide_caller_pos = tide_caller->pos();
    }

    int tide = props[PROPS_SHOALS_TIDE_KEY].get_short();
    int acc = props[PROPS_SHOALS_TIDE_VEL].get_short();
    const int old_tide = env.properties[PROPS_SHOALS_TIDE_KEY].get_short();
    while (turns_elapsed-- > 0)
        _shoals_run_tide(tide, acc);

    props[PROPS_SHOALS_TIDE_KEY].get_short() = tide;
    props[PROPS_SHOALS_TIDE_VEL].get_short() = acc;
    props[PROPS_SHOALS_TIDE_UPDATE_TIME].get_int() = you.elapsed_time;
    env.properties[PROPS_SHOALS_TIDE_KEY].get_short() = tide;

    if (force
        || tide_caller
        || old_tide / TIDE_MULTIPLIER != tide / TIDE_MULTIPLIER)
    {
        _shoals_tide_direction =
            tide > old_tide ? TIDE_RISING : TIDE_FALLING;
        _shoals_apply_tide(tide / TIDE_MULTIPLIER, incremental_tide);
    }
}

void shoals_release_tide(monster* mons)
{
    if (player_in_branch(BRANCH_SHOALS))
    {
        if (player_can_hear(mons->pos()))
        {
            mprf(MSGCH_SOUND, "The tide is released from %s call.",
                 apostrophise(mons->name(DESC_YOUR, true)).c_str());
            if (you.see_cell(mons->pos()))
                flash_view_delay(ETC_WATER, 150);
        }
        shoals_apply_tides(0, true, true);
    }
}

#ifdef WIZARD
static void _shoals_change_tide_granularity(int newval)
{
    LOW_TIDE        = LOW_TIDE * newval / TIDE_MULTIPLIER;
    HIGH_TIDE       = HIGH_TIDE * newval / TIDE_MULTIPLIER;
    TIDE_MULTIPLIER = newval;
}

static int _tidemod_keyfilter(int &c)
{
    return c == '+' || c == '-'? -1 : 1;
}

static void _shoals_force_tide(CrawlHashTable &props, int increment)
{
    int tide = props[PROPS_SHOALS_TIDE_KEY].get_short();
    tide += increment * TIDE_MULTIPLIER;
    tide = min(HIGH_TIDE, max(LOW_TIDE, tide));
    props[PROPS_SHOALS_TIDE_KEY] = short(tide);
    _shoals_tide_direction = increment > 0 ? TIDE_RISING : TIDE_FALLING;
    _shoals_apply_tide(tide / TIDE_MULTIPLIER, false);
}

void wizard_mod_tide()
{
    if (!player_in_branch(BRANCH_SHOALS) || !env.heightmap.get())
    {
        mprf(MSGCH_WARN, "Not in Shoals or no heightmap; tide not available.");
        return;
    }

    char buf[80];
    while (true)
    {
        mprf(MSGCH_PROMPT,
             "Tide inertia: %d. New value "
             "(smaller = faster tide) or use +/- to change tide: ",
             TIDE_MULTIPLIER);
        mpr("");
        const int res =
            cancellable_get_line(buf, sizeof buf, NULL, _tidemod_keyfilter);
        mesclr(true);
        if (key_is_escape(res))
            break;
        if (!res)
        {
            const int newgran = atoi(buf);
            if (newgran > 0 && newgran < 3000)
                _shoals_change_tide_granularity(newgran);
        }
        if (res == '+' || res == '-')
        {
            _shoals_force_tide(you.props, res == '+'? 2 : -2);
            viewwindow();
        }
    }
}
#endif
