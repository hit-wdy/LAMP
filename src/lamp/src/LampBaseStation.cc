/*
 * Copyright Notes
 *
 * Authors: Benjamin Morrell    (benjamin.morrell@jpl.nasa.gov)
 */


// Includes
#include <lamp/LampBaseStation.h>

// #include <math.h>
// #include <ctime>

namespace pu = parameter_utils;
namespace gu = geometry_utils;

// Constructor (if there is override)
LampBaseStation::LampBaseStation() {}

//Destructor
LampBaseStation::~LampBaseStation() {}

// Initialization - override for Base Station Setup
bool LampBaseStation::Initialize(const ros::NodeHandle& n, bool from_log) {

  // Get the name of the process
  name_ = ros::names::append(n.getNamespace(), "LampBaseStation");

  if (!mapper_.Initialize(n)) {
    ROS_ERROR("%s: Failed to initialize mapper.", name_.c_str());
    return false;
  }

  // Add load params etc
  if (!LoadParameters(n)) {
    ROS_ERROR("%s: Failed to load parameters.", name_.c_str());
    return false;
  }

  // Register Callbacks
  if (!RegisterCallbacks(n)) {
    ROS_ERROR("%s: Failed to register callbacks.", name_.c_str());
    return false;
  }

  // Publishers
  if (!CreatePublishers(n)) {
    ROS_ERROR("%s: Failed to create publishers.", name_.c_str());
    return false;
  }

  // Init Handlers
  if (!InitializeHandlers(n)){
    ROS_ERROR("%s: Failed to initialize handlers.", name_.c_str());
    return false;
  }

}

bool LampBaseStation::LoadParameters(const ros::NodeHandle& n) {

    if (!pu::Get("robot_names", robot_names_)) {
      return false;
    }
    ROS_INFO_STREAM("Robots registered at base station: ");
    for (auto s : robot_names_) {
      ROS_INFO_STREAM("\t\t\t" << s);
    }


  return true;
}

bool LampBaseStation::RegisterCallbacks(const ros::NodeHandle& n) {


  return true; 
}

bool LampBaseStation::CreatePublishers(const ros::NodeHandle& n) {

  // Creates pose graph publishers in base class
  LampBase::CreatePublishers(n);

  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  return true; 
}

bool LampBaseStation::InitializeHandlers(const ros::NodeHandle& n){
  if (!manual_loop_closure_handler_.Initialize(n)) {
    ROS_ERROR("%s: Failed to initialize the manual loop closure handler.", name_.c_str());
    return false;
  }

  return true; 
}

void LampBaseStation::ProcessTimerCallback(const ros::TimerEvent& ev) {

  // Check the handlers
  CheckHandlers();

  // Publish the pose graph
  if (b_has_new_factor_) {
    ROS_INFO_STREAM("Publishing pose graph with new factor");
    PublishPoseGraph();

    // Update and publish the map
    // GenerateMapPointCloud();
    mapper_.PublishMap();
    ROS_INFO_STREAM("Published new map");

    b_has_new_factor_ = false;
  }

  // Start optimize, if needed
  if (b_run_optimization_) {
      ROS_INFO_STREAM("Publishing pose graph to optimizer");
      PublishPoseGraphForOptimizer();

      b_run_optimization_ = false; 
  }

  // Publish anything that is needed 
}



// Check for data from all of the handlers
bool LampBaseStation::CheckHandlers() {

  return true;
}
