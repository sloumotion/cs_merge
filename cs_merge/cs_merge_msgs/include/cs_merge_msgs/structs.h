#ifndef CS_MERGE_STRUCTS_H_
#define CS_MERGE_STRUCTS_H_

#include "nav_msgs/OccupancyGrid.h"
#include "geometry_msgs/Transform.h"


struct Point {
	double x;
	double y;

	Point(double x, double y) : x(x), y(y) { }

  Point() { x = 0; y = 0; }

	void add(Point& other) {
		x += other.x;
		y += other.y;
	}

	void subtract(Point& other) {
		x -= other.x;
		y -= other.y;
	}
	
	double squaredDistance(Point& other) {
		return  (x - other.x)*(x - other.x) + (y - other.y)*(y - other.y);
	}

	double distance(Point& other) {
		return  sqrt((x - other.x)*(x - other.x) + (y - other.y)*(y - other.y));
	}
};

struct Transformation {
	double rotation;
	Point translation;
	double evaluation;

  Transformation(double rotation, Point translation, double evaluation) :
  	rotation(rotation), translation(translation), evaluation(evaluation) { }

  Transformation() { rotation = 0; translation = Point(0,0); evaluation = -1; }
};


struct Connection {

	std::string agent_name;

 	Transformation transformation;
  nav_msgs::OccupancyGrid map;

	bool map_updated;
	bool map_available;
  //Connection(std::string partner, transformation transform) : agent(agent), transform(transform) {}

  Connection(std::string agent_name) : agent_name(agent_name) {
		map_updated = false;
		map_available = false;
	}

/*
	Connection(std::string partner, transformation transform) : this(partner) {
		this.transformation = transformation;
	}
*/

	//When a new map is available, save the map and extract the occupied Points
	void getMap(const nav_msgs::OccupancyGridConstPtr& new_map) {
		ROS_INFO("Save map from %s", agent_name.c_str());
    map = *new_map;
		map_updated = true;
		map_available = true;
	}
};



#endif //CS_MERGE_STRUCTS_H_
