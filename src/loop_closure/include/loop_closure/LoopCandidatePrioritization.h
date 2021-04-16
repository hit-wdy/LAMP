/**
 * @file   LoopCandidatePrioritization.h
 * @brief  Base class for classes to find "priority loop closures" from the
 * candidates
 * @author Yun Chang
 */
#pragma once

#include <deque>
#include <map>
#include <queue>
#include <vector>

#include <pose_graph_msgs/LoopCandidate.h>
#include <pose_graph_msgs/LoopCandidateArray.h>
#include <ros/console.h>
#include <ros/ros.h>

namespace lamp_loop_closure {

class LoopCandidatePrioritization {
 public:
  LoopCandidatePrioritization();
  ~LoopCandidatePrioritization();

  virtual bool Initialize(const ros::NodeHandle& n) = 0;

  virtual bool LoadParameters(const ros::NodeHandle& n);

  virtual bool CreatePublishers(const ros::NodeHandle& n);

  virtual bool RegisterCallbacks(const ros::NodeHandle& n);

 protected:
  // Use different priority metrics to populate output (priority) queue
  virtual bool PopulatePriorityQueue() = 0;

  virtual void PublishBestCandidates() = 0;

  void InputCallback(
      const pose_graph_msgs::LoopCandidateArray::ConstPtr& input_candidates);

  // Define publishers and subscribers
  ros::Publisher loop_candidate_pub_;
  ros::Subscriber loop_candidate_sub_;

  // Loop closure candidates priority queue (high to low)
  std::deque<pose_graph_msgs::LoopCandidate> priority_queue_;
  // Loop closure queue as received from candidate generation
  std::queue<pose_graph_msgs::LoopCandidate> candidate_queue_;

  std::string param_ns_;
};

}  // namespace lamp_loop_closure