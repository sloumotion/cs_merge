#include "ros/ros.h"
#include "std_msgs/String.h"

#include <sstream>

#include "nav_msgs/GetMap.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>          //for debug purposes
#include <cstdio>

#include <cs_merge_icp_svd/cs_merge_icp_svd.h>

#include <cs_merge_msgs/transformation.h>
#include <cs_merge_msgs/getTransformation.h>

#include <cs_merge_msgs/structs.h>

#define DEG2RAD 0.017453293f


MergingMethod::MergingMethod() {
	
	ROS_INFO("Starting Merging Method");

	initParams();

	method_service = node.advertiseService("cs_merge_icp_svd", &MergingMethod::getTransformation, this);

}

bool MergingMethod::getTransformation(cs_merge_msgs::getTransformation::Request &req,
                 								cs_merge_msgs::getTransformation::Response &res) {
	
	Transformation result = calculateTransformation(req.map_one, req.map_two);

	cs_merge_msgs::transformation transformation;

	transformation.rotation = result.rotation;
	transformation.translationX = result.translation.x;
	transformation.translationY = result.translation.y;

	res.transformation = transformation;
	res.evaluation = result.evaluation;

	return true;
}

void MergingMethod::initParams() {
	node.getParam("ransac_fraction", ransac_fraction);
  node.getParam("repetitions", repetitions);
	node.getParam("starting_positions_amnt", starting_positions_amnt);

	ROS_INFO("Using parameters:\nransac_fraction: %f\nrepetitions: %f\n starting_positions_amnt: %f", ransac_fraction, repetitions, starting_positions_amnt);
}

Transformation MergingMethod::calculateTransformation(nav_msgs::OccupancyGrid &map1,
																						nav_msgs::OccupancyGrid &map2) {

	vector<Point> points1;
	vector<Point> points2;
	

	//First, extract occupied Points of maps
	for(unsigned int y = 0; y < map1.info.height; y++) {
  	for(unsigned int x = 0; x < map1.info.width; x++) {
			if (map1.data[x + y * map1.info.width] >= 50) {
				points1.push_back(Point(x,y));
			}
		}
	}


	for(unsigned int y = 0; y < map2.info.height; y++) {
  	for(unsigned int x = 0; x < map2.info.width; x++) {
			if (map2.data[x + y * map2.info.width] >= 50) {
				points2.push_back(Point(x,y));
			}
		}
	}

	//Next, perform ICP

	//Amount of Points to be disregarded (since ransac only )
	unsigned int disregardAmnt1 = round((double) points1.size() * (1-ransac_fraction));
  unsigned int disregardAmnt2 = round((double) points2.size() * (1-ransac_fraction));

	Point center1;
	Point center2;

	vector<Point> points1_used;
	vector<Point> points2_used;

	vector<Point> points1_used_copy;
	vector<Point> points2_used_copy;

	vector<Point> PointMatch1;
	vector<Point> PointMatch2;
	
	double distance;
	double min_distance;
	int closestPoint_index;
	
	double new_error = -1;
	double old_error = -1;
	double smallest_error = -1;

	Point optimum1;
	Point optimum2;
	

	for(unsigned int epoch=0; epoch<repetitions; epoch++) {
		
		points1_used = points1;
		points2_used = points2;

		//throw out random Points until we are left with desired amount
    for(unsigned int k=0; k<disregardAmnt1; k++)
    {
        points1_used.erase(points1_used.begin() + (rand() % points1_used.size()));
    }
    for(unsigned int k=0; k<disregardAmnt2; k++)
    {
        points2_used.erase(points2_used.begin() + (rand() % points2_used.size()));
    }


		//next, calculate centers of remaining Points
		center1 = Point();
		center2 = Point();

		for(std::vector<Point>::iterator it = points1_used.begin(); it != points1_used.end(); it++) {
        center1.add(*it);
    }

		center1.x /= points1_used.size();
		center1.y /= points1_used.size();

		for(std::vector<Point>::iterator it = points2_used.begin(); it != points2_used.end(); it++) {
        center2.add(*it);
    }

		center2.x /= points2_used.size();
		center2.y /= points2_used.size();

		//use different starting positions (diffent rotation angles around the centers)
		for(int starting_rotation = 0; starting_rotation < 360; starting_rotation += 360/starting_positions_amnt) {

			//copy the Points
			points1_used_copy = points1_used;
			points2_used_copy = points2_used;

			
			//Rotate map2 around center and overlay centers
      for(std::vector<Point>::iterator it = points2_used_copy.begin(); it != points2_used_copy.end(); it++) {
				Point temp(it->x, it->y);
        it->x = center1.x + (temp.x - center2.x) * cos(DEG2RAD*starting_rotation) - (temp.y - center2.y) * sin(DEG2RAD*starting_rotation);
				it->y = center1.y + (temp.x - center2.x) * sin(DEG2RAD*starting_rotation) + (temp.y - center2.y) * cos(DEG2RAD*starting_rotation);
			}

			//the reference Points describe the Points (0,0) and (1,0) in map2 and will be used to calculate the resulting transformation
			Point reference_Point1(0,0);
			Point reference_Point2(1,0);

			Point temp = reference_Point1;
			reference_Point1.x = center1.x + (temp.x - center2.x) * cos(DEG2RAD*starting_rotation) - (temp.y - center2.y) * sin(DEG2RAD*starting_rotation);
			reference_Point1.y = center1.y + (temp.x - center2.x) * sin(DEG2RAD*starting_rotation) + (temp.y - center2.y) * cos(DEG2RAD*starting_rotation);

			temp = reference_Point2;
			reference_Point2.x = center1.x + (temp.x - center2.x) * cos(DEG2RAD*starting_rotation) - (temp.y - center2.y) * sin(DEG2RAD*starting_rotation);
			reference_Point2.y = center1.y + (temp.x - center2.x) * sin(DEG2RAD*starting_rotation) + (temp.y - center2.y) * cos(DEG2RAD*starting_rotation);

			//Failsafe, in case error bounces back and forth
      int count = 500;
			
			//ICP Steps
      while(count--) {

        PointMatch1.clear();
        PointMatch2.clear();

				PointMatch1.reserve(points1_used_copy.size());
				PointMatch2.reserve(points1_used_copy.size());

        new_error = 0;

        //Find pairs
        for(std::vector<Point>::iterator points1_iterator = points1_used_copy.begin(); points1_iterator != points1_used_copy.end(); points1_iterator++) {
					//Compare all Points from map1...
        
        	min_distance = -1;
	        closestPoint_index = -1;

          for(std::vector<Point>::iterator points2_iterator = points2_used_copy.begin(); points2_iterator != points2_used_copy.end(); points2_iterator++) {
						//...to all Points from map2
                    
            distance = points1_iterator->distance(*points2_iterator);
            if(distance < min_distance || min_distance == -1) {
							//When distance between Points is the closest so far save potential partners index
                 
              closestPoint_index = points2_iterator - points2_used_copy.begin();
            	min_distance = distance;
            }
          }

		      if(closestPoint_index != -1) {
						//a partner exists, so save the pair and calculate mean squared error
		      
		        PointMatch1.push_back(*points1_iterator);
		        PointMatch2.push_back(points2_used_copy[closestPoint_index]);
		      }
		    }

		    new_error = 0;

				for(int pp=0; pp<PointMatch1.size(); pp++) {
					new_error += PointMatch1[pp].squaredDistance(PointMatch2[pp]);
				}
			
				new_error /= ((double) PointMatch1.size());

		    if(new_error < smallest_error || smallest_error == -1) {
		      smallest_error = new_error;
		      optimum1 = reference_Point1;
		      optimum2 = reference_Point2;
		    }

		    if(abs(old_error - new_error) < .0001 && old_error != -1) //if local error minimum is reached, we're done
		        break;

		    old_error = new_error;

		    //Compute parameters
		    //calculate mean
		    center1 = Point(0,0);
		    center2 = Point(0,0);

		    for(unsigned int pp = 0; pp < PointMatch1.size(); pp++) {
	        center1.add(PointMatch1[pp]);
	        center2.add(PointMatch2[pp]);
		    }

				center1.x /= PointMatch1.size();
				center1.y /= PointMatch1.size();

				center2.x /= PointMatch1.size();
				center2.y /= PointMatch1.size();


				//center Points
				for(unsigned int pp=0; pp < PointMatch1.size(); pp++) {
				  PointMatch1[pp].subtract(center1);
				  PointMatch2[pp].subtract(center2);
				}

				//SVD - http://scicomp.stackexchange.com/questions/8899/robust-algorithm-for-2x2-svd
				double M[3] = {0, 0, 0};

				for(unsigned int i=0; i< PointMatch1.size(); i++) {

			    M[0] += PointMatch1[i].x * PointMatch2[i].x; //H[0][0]
			    M[1] += PointMatch1[i].x * PointMatch2[i].y; //H[0][1]
			    M[2] += PointMatch1[i].y * PointMatch2[i].y; //H[1][1]
				}
          
				double q1 = (M[0] + M[2])/2; //E
				double q2 = (M[0] - M[2])/2; //F
				double q3 = (M[1] + M[1])/2; //G
				double q4 = (M[1] - M[1])/2; //H


		    double a1 = atan2(q3,q2);
		    double a2 = atan2(q4,q1);

				double angle1 = (a2-a1)/2;
				double angle2 = (a2+a1)/2;

		    double U[2][2] = {{cos(angle2), -1*sin(angle2)}, {sin(angle2), cos(angle2)}}; //U
		    //double S[2][2] = {{sx, 0}, {0, sy}};
		    double V_T[2][2] = {{cos(angle1), -1*sin(angle1)}, {sin(angle1), cos(angle1)}}; //V*

		    //H = USV_T

		    double U_T[2][2] = {{U[0][0], U[1][0]}, {U[0][1], U[1][1]}};
		    double V[2][2] = {{V_T[0][0], V_T[1][0]}, {V_T[0][1], V_T[1][1]}};

		    double R[2][2] = {{V[0][0]*U_T[0][0] + V[0][1]*U_T[1][0], V[0][0]*U_T[0][1] + V[0][1]*U_T[1][1]},
		                      {V[1][0]*U_T[0][0] + V[1][1]*U_T[1][0], V[1][0]*U_T[0][1] + V[1][1]*U_T[1][1]}}; //R = VU_T


		    double translationX = center1.x - (R[0][0]*center2.x + R[0][1]*center2.y);
		    double translationY = center1.y - (R[1][0]*center2.x + R[1][1]*center2.y);


				//Transform map2 with calculated values
				for(std::vector<Point>::iterator it = points2_used_copy.begin(); it != points2_used_copy.end(); it++)
				{
				    temp = *it;

				    it->x = R[0][0]*(temp.x) + R[0][1]*temp.y + translationX;
				    it->y = R[1][0]*temp.x + R[1][1]*temp.y + translationY;
				}

		    //Transform reference Points
		    temp = reference_Point1;

		    reference_Point1.x = R[0][0]*temp.x + R[0][1]*temp.y + translationX;
		    reference_Point1.y = R[1][0]*temp.x + R[1][1]*temp.y + translationY;

		    temp = reference_Point2;

		    reference_Point2.x = R[0][0]*temp.x + R[0][1]*temp.y + translationX;
		    reference_Point2.y = R[1][0]*temp.x + R[1][1]*temp.y + translationY;
		  }
		}
	}

	optimum2.subtract(optimum1);

	return Transformation(atan2(optimum2.y, optimum2.x), optimum1, smallest_error);

}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "cs_merge_icp_svd");

		MergingMethod method;

    ros::spin();

    return 0;
}
