#ifndef _CS_MERGE_CONTROLLER_H_
#define _CS_MERGE_CONTROLLER_H_

using namespace std;

class ConnectionHandler
{
public:
  ConnectionHandler();
  void getMap(const nav_msgs::OccupancyGridConstPtr& new_map);
  void updateMaps(int timeout);
  void updateTransformations();
  void buildWorld();
  void publishWorld()
private:

  ros::NodeHandle node;

  //own map and world
  nav_msgs::OccupancyGrid map;
  nav_msgs::OccupancyGrid world;

	std::vector<std::string> agents;

  std::vector<Connection> connections;

  string merging_method;

  ros::Publisher world_pub;
  tf::TransformBroadcaster tf_br;
  tf::Transform transform;

  string worldTopic;
  string mapTopic;


  ros::Subscriber map_sub;
  bool map_updated;
  bool map_available;
  bool world_locked;

/*
  std::string ownName;
  ros::Subscriber mapSub;

  string method;
  std::vector<Connection> Connections;

  nav_msgs::OccupancyGrid map;
  std::vector<point> own_occ;
  bool ownMapUpdated;


   timeout;

  nav_msgs::OccupancyGrid world;

  ros::ServiceServer service;
  */
}


#endif _CS_MERGE_CONTROLLER_H_
