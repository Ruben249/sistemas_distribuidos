#ifndef SDC_P4__BULLY_HPP_
#define SDC_P4__BULLY_HPP_

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/int32.hpp"
#include "rclcpp/macros.hpp"

#include <chrono>
#include <set>
#include <string>
#include <functional>
#include <memory>

namespace sdc_p4
{

class BullyNode : public rclcpp::Node
{
public:
  RCLCPP_SMART_PTR_DEFINITIONS(BullyNode)

  explicit BullyNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  enum class NodeState
  {
    FOLLOWER,
    LEADER,
    ELECTION
  };

  // Constants
  static constexpr int HEARTBEAT_PERIOD = 5000;
  static constexpr int LEADER_START_DELAY = 1000;
  static constexpr int FOLLOWER_START_DELAY = 7000;
  static constexpr int HEARTBEAT_TIMEOUT = 15000;
  static constexpr int ELECTION_DURATION = 5000;
  static constexpr int DEBOUNCE_TIME = 500;
  static constexpr int START_DELAY = 5000;

  int pid_;
  NodeState current_state_;
  std::string role_str_;
  std::string start_role_;
  rclcpp::Time last_heartbeat_time_;
  bool election_in_progress_;
  
  std::set<int> candidates_;
  int highest_candidate_pid_;
  bool i_started_election_;
  bool is_initializing_;

  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::TimerBase::SharedPtr timeout_timer_;
  rclcpp::TimerBase::SharedPtr election_timer_;
  rclcpp::TimerBase::SharedPtr debounce_timer_;
  rclcpp::TimerBase::SharedPtr leader_init_timer_;
  rclcpp::TimerBase::SharedPtr init_timer_;
  rclcpp::Time election_start_time_;

  // Publishers
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr election_start_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr candidates_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr new_leader_pub_;

  // Subscribers
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr heartbeat_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr election_start_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr candidates_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr new_leader_sub_;

  // Callback methods
  void heartbeat_callback();
  void timeout_callback();
  void election_timeout_callback();
  void debounce_callback();
  void leader_init_callback();

  // Message handlers
  void handle_heartbeat(const std_msgs::msg::Float64::SharedPtr msg);
  void handle_election_start(const std_msgs::msg::Int32::SharedPtr msg);
  void handle_candidate(const std_msgs::msg::Int32::SharedPtr msg);
  void handle_new_leader(const std_msgs::msg::Int32::SharedPtr msg);

  // State methods
  void change_state(NodeState new_state);
  void start_election();
  void end_election();
  void announce_new_leader();
  void reset_election();
  void start_heartbeat();


  std::string state_to_string(NodeState state);
  double get_current_epoch();
};

}  // namespace sdc_p4

#endif  // SDC_P4__BULLY_HPP_