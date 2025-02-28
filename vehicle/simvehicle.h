/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef VEHICLE_SIMVEHICLE_H
#define VEHICLE_SIMVEHICLE_H


#include "../simtypes.h"
#include "../obj/simobj.h"
#include "../halthandle_t.h"
#include "../convoihandle_t.h"
#include "../ifc/simtestdriver.h"
#include "../boden/grund.h"
#include "../descriptor/vehicle_desc.h"
#include "../vehicle/overtaker.h"
#include "../tpl/slist_tpl.h"
#include "../tpl/ptrhashtable_tpl.h"

class convoi_t;
class schedule_t;
class signal_t;
class ware_t;
class route_t;

/*----------------------- Movables ------------------------------------*/

/**
 * Base class for all vehicles
 */
class vehicle_base_t : public obj_t
{
public:
	static uint8 diagonal_vehicle_steps_per_tile;
	
protected:
	// offsets for different directions
	static sint8 dxdy[16];

	// to make the length on diagonals configurable
	// Number of vehicle steps along a diagonal...
	// remember to subtract one when stepping down to 0
	// static uint8 diagonal_vehicle_steps_per_tile;
	static uint8 old_diagonal_vehicle_steps_per_tile;
	static uint16 diagonal_multiplier;

	// [0]=xoff [1]=yoff
	static sint8 driveleft_base_offsets[8][2];
	static sint8 overtaking_base_offsets[8][2];

	/**
	 * Actual travel direction in screen coordinates
	 */
	ribi_t::ribi direction;

	// true on slope (make calc_height much faster)
	uint8 use_calc_height:1;

	/**
	 * Thing is moving on this lane.
	 * Possible values:
	 * (Back)
	 * 0 - sidewalk (going on the right side to w/sw/s)
	 * 1 - road     (going on the right side to w/sw/s)
	 * 2 - middle   (everything with waytype != road)
	 * 3 - road     (going on the right side to se/e/../nw)
	 * 4 - sidewalk (going on the right side to se/e/../nw)
	 * (Front)
	 */
	uint8 disp_lane:3;

	sint8 dx, dy;

	// number of steps in this tile (255 per tile)
	uint8 steps, steps_next;

	/**
	 * Next position on our path
	 */
	koord3d pos_next;

	/**
	 * Offsets for uphill/downhill.
	 * Have to be multiplied with -TILE_HEIGHT_STEP/2.
	 * To obtain real z-offset, interpolate using steps, steps_next.
	 */
	uint8 zoff_start:4, zoff_end:4;

	// cached image
	image_id image;

	/**
	 * this vehicle will enter passing lane in the next tile -> 1
	 * this vehicle will enter traffic lane in the next tile -> -1
	 * Unclear -> 0
	 * @author THLeaderH
	 */
	sint8 next_lane;

	/**
	 * Vehicle movement: check whether this vehicle can enter the next tile (pos_next).
	 * @returns NULL if check fails, otherwise pointer to the next tile
	 */
	virtual grund_t* hop_check() = 0;

	/**
	 * Vehicle movement: change tiles, calls leave_tile and enter_tile.
	 * @param gr pointer to ground of new position (never NULL)
	 */
	virtual void hop(grund_t* gr) = 0;

	void calc_image() OVERRIDE = 0;

	// check for road vehicle, if next tile is free
	vehicle_base_t *no_cars_blocking( const grund_t *gr, const convoi_t *cnv, const uint8 current_direction, const uint8 next_direction, const uint8 next_90direction, const private_car_t *pcar, sint8 lane_on_the_tile );

	// If true, two vehicles might crash by lane crossing.
	bool judge_lane_crossing( const uint8 current_direction, const uint8 next_direction, const uint8 other_next_direction, const bool is_overtaking, const bool forced_to_change_lane ) const;

	// only needed for old way of moving vehicles to determine position at loading time
	bool is_about_to_hop( const sint8 neu_xoff, const sint8 neu_yoff ) const;

public:
	// only called during load time: set some offsets
	static void set_diagonal_multiplier( uint32 multiplier, uint32 old_multiplier );
	static uint16 get_diagonal_multiplier() { return diagonal_multiplier; }
	static uint8 get_diagonal_vehicle_steps_per_tile() { return diagonal_vehicle_steps_per_tile; }

	static void set_overtaking_offsets( bool driving_on_the_left );

	// if true, this convoi needs to restart for correct alignment
	bool need_realignment() const;

	virtual uint32 do_drive(uint32 dist);	// basis movement code

	inline void set_image( image_id b ) { image = b; }
	image_id get_image() const OVERRIDE {return image;}

	sint16 get_hoff(const sint16 raster_width=1) const;
	uint8 get_steps() const {return steps;}

	uint8 get_disp_lane() const { return disp_lane; }

	// to make smaller steps than the tile granularity, we have to calculate our offsets ourselves!
	virtual void get_screen_offset( int &xoff, int &yoff, const sint16 raster_width ) const;

	/**
	 * Vehicle movement: calculates z-offset of vehicles on slopes,
	 * handles vehicles that are invisible in tunnels.
	 * @param gr vehicle is on this ground
	 * @note has to be called after loading to initialize z-offsets
	 */
	void calc_height(grund_t *gr = NULL);

	void rotate90() OVERRIDE;

	template<class K1, class K2>
	static ribi_t::ribi calc_direction(const K1& from, const K2& to)
	{
		return ribi_type(from, to);
	}

	ribi_t::ribi calc_set_direction(const koord3d& start, const koord3d& ende);

	ribi_t::ribi get_direction() const {return direction;}

	ribi_t::ribi get_90direction() const {return ribi_type(get_pos(), get_pos_next());}

	koord3d get_pos_next() const {return pos_next;}

	waytype_t get_waytype() const OVERRIDE = 0;

	// true, if this vehicle did not moved for some time
	virtual bool is_stuck() { return true; }

	/**
	 * Vehicle movement: enter tile, add this to the ground.
	 * @pre position (obj_t::pos) needs to be updated prior to calling this functions
	 * @param gr pointer to ground (never NULL)
	 */
	virtual void enter_tile(grund_t *gr);

	/**
	 * Vehicle movement: leave tile, release reserved crossing, remove vehicle from the ground.
	 */
	virtual void leave_tile();

	virtual overtaker_t *get_overtaker() { return NULL; }

	vehicle_base_t();

	vehicle_base_t(koord3d pos);

	virtual bool is_flying() const { return false; }
};


template<> inline vehicle_base_t* obj_cast<vehicle_base_t>(obj_t* const d)
{
	return d->is_moving() ? static_cast<vehicle_base_t*>(d) : 0;
}


/**
 * Class for all vehicles with route
 */
class vehicle_t : public vehicle_base_t, public test_driver_t
{
private:
	/**
	* Date of purchase in months
	*/
	sint32 purchase_time;

	/**
	* For the more physical acceleration model friction is introduced
	* frictionforce = gamma*speed*weight
	* since the total weight is needed a lot of times, we save it
	*/
	uint32 sum_weight;

	grund_t* hop_check() OVERRIDE;

	static ptrhashtable_tpl<const vehicle_desc_t*, uint32> full_load_weights; // key: name of vehicle desc

	/**
	 * Calculate friction caused by slopes and curves.
	 */
	virtual void calc_friction(const grund_t *gr);

	static uint32 calc_full_load_weight(const vehicle_desc_t*);

protected:
	void hop(grund_t*) OVERRIDE;

	// current limit (due to track etc.)
	sint32 speed_limit;

	ribi_t::ribi previous_direction;

	// for target reservation and search
	halthandle_t target_halt;

	/** The friction is calculated new every step, so we save it too
	*/
	sint16 current_friction;

	/**
	* Current index on the route
	*/
	uint16 route_index;

	uint16 total_freight; // since the sum is needed quite often, it is cached
	slist_tpl<ware_t> fracht;   // list of goods being transported

	const vehicle_desc_t *desc;

	convoi_t *cnv;  // != NULL if the vehicle is part of a Convoi

	bool leading:1; // true, if vehicle is first vehicle of a convoi
	bool last:1;    // true, if vehicle is last vehicle of a convoi
	bool smoke:1;
	bool check_for_finish:1; // true, if on the last tile
	bool has_driven:1;

	virtual bool check_next_tile(const grund_t* ) const OVERRIDE {return false;}

public:
	void calc_image() OVERRIDE;

	// the coordinates, where the vehicle was loaded the last time
	koord3d last_stop_pos;

	convoi_t *get_convoi() const { return cnv; }

	void rotate90() OVERRIDE;

	ribi_t::ribi get_previous_direction() const { return previous_direction; }


	/**
	 * Method checks whether next tile is free to move on.
	 * Looks up next tile, and calls @ref can_enter_tile(const grund_t*, sint32&, uint8).
	 */
	bool can_enter_tile(sint32 &restart_speed, uint8 second_check_count);

	/**
	 * Method checks whether next tile is free to move on.
	 * @param gr_next next tile, must not be NULL
	 */
	virtual bool can_enter_tile(const grund_t *gr_next, sint32 &restart_speed, uint8 second_check_count) = 0;

	void enter_tile(grund_t*) OVERRIDE;

	void leave_tile() OVERRIDE;

	waytype_t get_waytype() const OVERRIDE = 0;

	/**
	* Determine the direction bits for this kind of vehicle.
	*/
	ribi_t::ribi get_ribi(const grund_t* gr) const OVERRIDE { return gr->get_weg_ribi(get_waytype()); }

	sint32 get_purchase_time() const {return purchase_time;}

	void get_smoke(bool yesno ) { smoke = yesno;}

	virtual bool calc_route(koord3d start, koord3d ziel, sint32 max_speed, route_t* route);
	uint16 get_route_index() const {return route_index;}

	/**
	* Get the base image.
	*/
	image_id get_base_image() const { return desc->get_base_image(); }

	/**
	 * @return image with base direction and freight image taken from loaded cargo
	 */
	image_id get_loaded_image() const;

	/**
	* @return vehicle description object
	*/
	const vehicle_desc_t *get_desc() const {return desc; }

	/**
	* @return die running_cost in Cr/100Km
	*/
	int get_operating_cost() const { return desc->get_running_cost(); }

	/**
	* Play sound, when the vehicle is visible on screen
	*/
	void play_sound() const;

	/**
	 * Prepare vehicle for new ride.
	 * Sets route_index, pos_next, steps_next.
	 * If @p recalc is true this sets position and recalculates/resets movement parameters.
	 */
	void initialise_journey( uint16 start_route_index, bool recalc );

	vehicle_t();
	vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player);

	~vehicle_t();

	void make_smoke() const;

	void show_info() OVERRIDE;

	void info(cbuffer_t & buf) const OVERRIDE;

	/**
	 * return friction constant: changes in hill and curves; may even negative downhill *
	 */
	inline sint16 get_frictionfactor() const { return current_friction; }

	/*
	 * Return total weight including freight (in kg!)
	*/
	uint32 get_total_weight() const;

	// returns speedlimit of ways (and if convoi enters station etc)
	// the convoi takes care of the max_speed of the vehicle
	sint32 get_speed_limit() const { return speed_limit; }

	const slist_tpl<ware_t> & get_cargo() const { return fracht;}   // list of goods being transported

	/**
	 * Rotate freight target coordinates, has to be called after rotating factories.
	 */
	void rotate90_freight_destinations(const sint16 y_size);

	/**
	* Calculate the total quantity of goods moved
	*/
	uint16 get_total_cargo() const { return total_freight; }

	/**
	* Calculate transported cargo total weight in KG
	*/
	uint32 get_cargo_weight() const;

	/**
	* get the type of cargo this vehicle can transport
	*/
	const goods_desc_t* get_cargo_type() const { return desc->get_freight_type(); }

	/**
	* Get the maximum capacity
	*/
	uint16 get_cargo_max() const {return desc->get_capacity(); }

	ribi_t::ribi get_next_90direction() const;

	const char * get_cargo_mass() const;

	/**
	* create an info text for the freight
	* e.g. to display in a info window
	*/
	void get_cargo_info(cbuffer_t & buf) const;

	/**
	* Delete all vehicle load
	*/
	void discard_cargo();

	/**
	* Payment is done per hop. It iterates all goods and calculates
	* the income for the last hop. This method must be called upon
	* every stop.
	* @return income total for last hop
	*/
	sint64 calc_revenue(const koord3d& start, const koord3d& end) const;

	// sets or query begin and end of convois
	void set_leading(bool janein) {leading = janein;}
	bool is_leading() {return leading;}

	void set_last(bool janein) {last = janein;}
	bool is_last() {return last;}

	// marks the vehicle as really used
	void set_driven() { has_driven = true; }

	virtual void set_convoi(convoi_t *c);

	/**
	 * Unload freight to halt
	 * @return sum of unloaded goods
	 */
	uint16 unload_cargo(halthandle_t halt, bool all );

	/**
	 * Load freight from halt
	 * @return amount loaded
	 */
	uint16 load_cargo(halthandle_t halt, const vector_tpl<halthandle_t>& destination_halts);

	/**
	* Remove freight that no longer can reach it's destination
	* i.e. because of a changed schedule
	*/
	void remove_stale_cargo();

	/**
	* Generate a matching schedule for the vehicle type
	*/
	virtual schedule_t *generate_new_schedule() const = 0;

	const char *is_deletable(const player_t *player) OVERRIDE;

	void rdwr(loadsave_t *file) OVERRIDE;
	virtual void rdwr_from_convoi(loadsave_t *file);

	uint32 calc_sale_value() const;

	// true, if this vehicle did not moved for some time
	bool is_stuck() OVERRIDE;

	// this routine will display a tooltip for lost, on depot order, and stuck vehicles
#ifdef MULTI_THREAD
	void display_overlay(int xpos, int ypos) const OVERRIDE;
#else
	void display_after(int xpos, int ypos, bool dirty) const OVERRIDE;
#endif
};


template<> inline vehicle_t* obj_cast<vehicle_t>(obj_t* const d)
{
	return dynamic_cast<vehicle_t*>(d);
}


/**
 * A class for road vehicles. Manages the look of the vehicles
 * and the navigability of tiles.
 * @see vehicle_t
 */
class road_vehicle_t : public vehicle_t
{
private:
	// called internally only from ist_weg_frei()
	// returns true on success
	bool choose_route(sint32 &restart_speed, ribi_t::ribi start_direction, uint16 index);

	koord3d last_stop_for_intersection;

	vector_tpl<koord3d> reserving_tiles;

protected:
	bool check_next_tile(const grund_t *bd) const OVERRIDE;

	koord3d pos_prev; //used in enter_tile()

public:
	virtual void enter_tile(grund_t*) OVERRIDE;

	void leave_tile() OVERRIDE;

	void rotate90() OVERRIDE;

	void calc_disp_lane();

	waytype_t get_waytype() const OVERRIDE { return road_wt; }

	road_vehicle_t(loadsave_t *file, bool first, bool last);
	road_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv); // start and schedule

	~road_vehicle_t();

	void set_convoi(convoi_t *c) OVERRIDE;

	// how expensive to go here (for way search)
	int get_cost(const grund_t *gr, const weg_t *w, const sint32 max_speed, ribi_t::ribi from) const OVERRIDE;

	uint32 get_cost_upslope() const OVERRIDE { return 15; }

	bool calc_route(koord3d start, koord3d ziel, sint32 max_speed, route_t* route) OVERRIDE;

	bool can_enter_tile(const grund_t *gr_next, sint32 &restart_speed, uint8 second_check_count) OVERRIDE;

	// returns true for the way search to an unknown target.
	bool is_target(const grund_t *,const grund_t *) const OVERRIDE;

	// since we must consider overtaking, we use this for offset calculation
	virtual void get_screen_offset( int &xoff, int &yoff, const sint16 raster_width, bool prev_based ) const;
	virtual void get_screen_offset( int &xoff, int &yoff, const sint16 raster_width ) const OVERRIDE { get_screen_offset(xoff,yoff,raster_width,false); }

	obj_t::typ get_typ() const OVERRIDE { return road_vehicle; }

	koord3d get_pos_prev() const { return pos_prev; }

	schedule_t * generate_new_schedule() const OVERRIDE;

	virtual overtaker_t* get_overtaker() OVERRIDE;

	void rdwr_from_convoi(loadsave_t *file) OVERRIDE;

	virtual vehicle_base_t* other_lane_blocked(const bool only_search_top = false, sint8 offset = 0) const;
	virtual vehicle_base_t* other_lane_blocked_offset() const { return other_lane_blocked(false,1); }

	void refresh();

	void unreserve_all_tiles();
};


/**
 * A class for rail vehicles (trains). Manages the look of the vehicles
 * and the navigability of tiles.
 * @see vehicle_t
 */
class rail_vehicle_t : public vehicle_t
{
protected:
	bool check_next_tile(const grund_t *bd, bool coupling) const OVERRIDE;
	bool check_next_tile(const grund_t *bd) const OVERRIDE { return check_next_tile(bd,false); }

	void enter_tile(grund_t*) OVERRIDE;

	bool is_signal_clear(uint16 start_index, sint32 &restart_speed);
	bool is_pre_signal_clear(signal_t *sig, uint16 start_index, sint32 &restart_speed);
	bool is_priority_signal_clear(signal_t *sig, uint16 start_index, sint32 &restart_speed);
	bool is_longblock_signal_clear(signal_t *sig, uint16 start_index, sint32 &restart_speed);
	bool is_choose_signal_clear(signal_t *sig, uint16 start_index, sint32 &restart_speed);

public:
	waytype_t get_waytype() const OVERRIDE { return track_wt; }

	// since we might need to un-reserve previously used blocks, we must do this before calculation a new route
	bool calc_route(koord3d start, koord3d ziel, sint32 max_speed, route_t* route) OVERRIDE;

	// how expensive to go here (for way search)
	int get_cost(const grund_t *gr, const weg_t *w, const sint32 max_speed, ribi_t::ribi from) const OVERRIDE;

	uint32 get_cost_upslope() const OVERRIDE { return 25; }

	// returns true for the way search to an unknown target.
	bool is_target(const grund_t *,const grund_t *) const OVERRIDE;
	bool is_coupling_target(const grund_t *, const grund_t *) const OVERRIDE;

	// handles all block stuff and route choosing ...
	bool can_enter_tile(const grund_t *gr_next, sint32 &restart_speed, uint8) OVERRIDE;

	// reserves or un-reserves all blocks and returns the handle to the next block (if there)
	// returns true on successful reservation
	bool block_reserver(const route_t *route, uint16 start_index, uint16 &next_signal, uint16 &next_crossing, int signal_count, bool reserve, bool force_unreserve, bool use_vector = false ) const;

	bool can_couple(const route_t* route, uint16 start_index, uint16 &coupling_index, uint8 &coupling_steps, bool ignore_signals = false);

	void leave_tile() OVERRIDE;

	typ get_typ() const OVERRIDE { return rail_vehicle; }

	rail_vehicle_t(loadsave_t *file, bool is_first, bool is_last);
	rail_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t *cnv);
	~rail_vehicle_t();

	void set_convoi(convoi_t *c) OVERRIDE;

	virtual schedule_t * generate_new_schedule() const OVERRIDE;

	// step() routine called by convoy
	bool check_longblock_signal(signal_t *sig, uint16 start_index, sint32 &restart_speed);
};



/**
 * very similar to normal railroad, so we can implement it here completely ...
 * @see vehicle_t
 */
class monorail_vehicle_t : public rail_vehicle_t
{
public:
	waytype_t get_waytype() const OVERRIDE { return monorail_wt; }

	// all handled by rail_vehicle_t
	monorail_vehicle_t(loadsave_t *file, bool is_first, bool is_last) : rail_vehicle_t(file,is_first, is_last) {}
	monorail_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv) : rail_vehicle_t(pos, desc, player, cnv) {}

	typ get_typ() const OVERRIDE { return monorail_vehicle; }

	schedule_t * generate_new_schedule() const OVERRIDE;
};



/**
 * very similar to normal railroad, so we can implement it here completely ...
 * @see vehicle_t
 */
class maglev_vehicle_t : public rail_vehicle_t
{
public:
	waytype_t get_waytype() const OVERRIDE { return maglev_wt; }

	// all handled by rail_vehicle_t
	maglev_vehicle_t(loadsave_t *file, bool is_first, bool is_last) : rail_vehicle_t(file, is_first, is_last) {}
	maglev_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv) : rail_vehicle_t(pos, desc, player, cnv) {}

	typ get_typ() const OVERRIDE { return maglev_vehicle; }

	schedule_t * generate_new_schedule() const OVERRIDE;
};



/**
 * very similar to normal railroad, so we can implement it here completely
 * @see vehicle_t
 */
class narrowgauge_vehicle_t : public rail_vehicle_t
{
public:
	waytype_t get_waytype() const OVERRIDE { return narrowgauge_wt; }

	// all handled by rail_vehicle_t
	narrowgauge_vehicle_t(loadsave_t *file, bool is_first, bool is_last) : rail_vehicle_t(file, is_first, is_last) {}
	narrowgauge_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv) : rail_vehicle_t(pos, desc, player, cnv) {}

	typ get_typ() const OVERRIDE { return narrowgauge_vehicle; }

	schedule_t * generate_new_schedule() const OVERRIDE;
};



/**
 * A class for naval vehicles. Manages the look of the vehicles
 * and the navigability of tiles.
 * @see vehicle_t
 */
class water_vehicle_t : public vehicle_t
{
protected:
	// how expensive to go here (for way search)
	int get_cost(const grund_t *, const weg_t*, const sint32, ribi_t::ribi) const OVERRIDE { return 1; }

	void calc_friction(const grund_t *gr) OVERRIDE;

	bool check_next_tile(const grund_t *bd) const OVERRIDE;

	void enter_tile(grund_t*) OVERRIDE;

public:
	waytype_t get_waytype() const OVERRIDE { return water_wt; }

	bool can_enter_tile(const grund_t *gr_next, sint32 &restart_speed, uint8) OVERRIDE;

	// returns true for the way search to an unknown target.
	bool is_target(const grund_t *,const grund_t *) const OVERRIDE {return 0;}

	water_vehicle_t(loadsave_t *file, bool is_first, bool is_last);
	water_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv);

	obj_t::typ get_typ() const OVERRIDE { return water_vehicle; }

	schedule_t * generate_new_schedule() const OVERRIDE;
};


/**
 * A class for aircrafts. Manages the look of the vehicles
 * and the navigability of tiles.
 * @see vehicle_t
 */
class air_vehicle_t : public vehicle_t
{
public:
	enum flight_state {
		taxiing             = 0,
		departing           = 1,
		flying              = 2,
		landing             = 3,
		looking_for_parking = 4,
		circling            = 5,
		taxiing_to_halt     = 6
	};

private:
	// only used for is_target() (do not need saving)
	ribi_t::ribi approach_dir;
/* #ifdef USE_DIFFERENT_WIND
	static uint8 get_approach_ribi( koord3d start, koord3d ziel );
#endif */
	// only used for route search and approach vectors of get_ribi() (do not need saving)
	koord3d search_start;
	koord3d search_end;

	flight_state state; // functions needed for the search without destination from find_route

	sint16 flying_height;
	sint16 target_height;
	uint32 search_for_stop, touchdown, takeoff;

	sint16 altitude_level; // for AFHP
	sint16 landing_distance; // for AFHP

	void calc_altitude_level(sint32 speed_limit_kmh){
		altitude_level = max(5, speed_limit_kmh/33);
		altitude_level = min(altitude_level, 30);
		// landing_distance = altitude_level - 1;
		landing_distance = altitude_level - 2;
	}

protected:
	// jumps to next tile and correct the height ...
	void hop(grund_t*) OVERRIDE;

	bool check_next_tile(const grund_t *bd) const OVERRIDE;

	void enter_tile(grund_t*) OVERRIDE;

	bool block_reserver( uint32 start, uint32 end, bool reserve ) const;

	// find a route and reserve the stop position
	bool find_route_to_stop_position();

public:
	air_vehicle_t(loadsave_t *file, bool is_first, bool is_last);
	air_vehicle_t(koord3d pos, const vehicle_desc_t* desc, player_t* player, convoi_t* cnv); // start and schedule

	// to shift the events around properly
	void get_event_index( flight_state &state_, uint32 &takeoff_, uint32 &stopsearch_, uint32 &landing_ ) { state_ = state; takeoff_ = takeoff; stopsearch_ = search_for_stop; landing_ = touchdown; }
	void set_event_index( flight_state state_, uint32 takeoff_, uint32 stopsearch_, uint32 landing_ ) { state = state_; takeoff = takeoff_; search_for_stop = stopsearch_; touchdown = landing_; }

	// since we are drawing ourselves, we must mark ourselves dirty during deletion
	~air_vehicle_t();

	waytype_t get_waytype() const OVERRIDE { return air_wt; }

	// returns true for the way search to an unknown target.
	bool is_target(const grund_t *,const grund_t *) const OVERRIDE;

	// return valid direction
	ribi_t::ribi get_ribi(const grund_t* ) const OVERRIDE;

	// how expensive to go here (for way search)
	int get_cost(const grund_t *gr, const weg_t *w, const sint32 max_speed, ribi_t::ribi from) const OVERRIDE;

	bool can_enter_tile(const grund_t *gr_next, sint32 &restart_speed, uint8) OVERRIDE;

	void set_convoi(convoi_t *c) OVERRIDE;

	bool calc_route(koord3d start, koord3d ziel, sint32 max_speed, route_t* route) OVERRIDE;

	typ get_typ() const OVERRIDE { return air_vehicle; }

	schedule_t *generate_new_schedule() const OVERRIDE;

	void rdwr_from_convoi(loadsave_t *file) OVERRIDE;

	int get_flyingheight() const {return flying_height-get_hoff()-2;}

	// image: when flying empty, on ground the plane
	image_id get_image() const OVERRIDE {return !is_on_ground() ? IMG_EMPTY : image;}

	// image: when flying the shadow, on ground empty
	image_id get_outline_image() const OVERRIDE {return !is_on_ground() ? image : IMG_EMPTY;}

	// shadow has black color (when flying)
	FLAGGED_PIXVAL get_outline_colour() const OVERRIDE {return !is_on_ground() ? TRANSPARENT75_FLAG | OUTLINE_FLAG | color_idx_to_rgb(COL_BLACK) : 0;}

#ifdef MULTI_THREAD
	// this draws the "real" aircrafts (when flying)
	void display_after(int xpos, int ypos, const sint8 clip_num) const OVERRIDE;

	// this routine will display a tooltip for lost, on depot order, and stuck vehicles
	void display_overlay(int xpos, int ypos) const OVERRIDE;
#else
	// this draws the "real" aircrafts (when flying)
	void display_after(int xpos, int ypos, bool dirty) const OVERRIDE;
#endif

	void calc_friction(const grund_t*) OVERRIDE {}

	bool is_on_ground() const { return flying_height==0  &&  !(state==circling  ||  state==flying); }

	const char *is_deletable(const player_t *player) OVERRIDE;

	bool is_flying() const OVERRIDE { return !is_on_ground(); }
};

#endif
