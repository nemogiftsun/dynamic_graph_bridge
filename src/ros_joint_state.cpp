#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>

#include <dynamic-graph/command.h>
#include <dynamic-graph/factory.h>
#include <dynamic-graph/pool.h>
#include <sot-dynamic-pinocchio/dynamic-pinocchio.h>

#include "dynamic_graph_bridge/ros_init.hh"
#include "ros_joint_state.hh"
#include "sot_to_ros.hh"

namespace dynamicgraph
{
  DYNAMICGRAPH_FACTORY_ENTITY_PLUGIN(RosJointState, "RosJointState");
  const double RosJointState::ROS_JOINT_STATE_PUBLISHER_RATE = 0.01;

  namespace command
  {
    using ::dynamicgraph::command::Command;
    using ::dynamicgraph::command::Value;

    class RetrieveJointNames : public Command
    {
    public:
      RetrieveJointNames (RosJointState& entity,
			  const std::string& docstring);
      virtual Value doExecute ();
    };

    RetrieveJointNames::RetrieveJointNames
    (RosJointState& entity, const std::string& docstring)
      : Command (entity, boost::assign::list_of (Value::STRING), docstring)
    {
}

    namespace {
      void  buildJointNames (sensor_msgs::JointState& jointState, se3::Model* robot_model) {
	int cnt = 0;
	for (int i=1;i<robot_model->njoints;i++) {
	  // Ignore anchors.
	  if (se3::nv(robot_model->joints[i]) != 0) {
	    // If we only have one dof, the dof name is the joint name.
	    if (se3::nv(robot_model->joints[i]) == 1) {
	      jointState.name[cnt] =  robot_model->names[i];
	      cnt++;
	    }
	    else {
	      // ...otherwise, the dof name is the joint name on which
	      // the dof id is appended.
	      int joint_dof = se3::nv(robot_model->joints[i]);
	      for(int j = 0; j<joint_dof; j++) {
		boost::format fmt("%1%_%2%");
		fmt % robot_model->names[i];
		fmt % j;
		jointState.name[cnt + j] =  fmt.str();
	      }
	      cnt+=joint_dof;
	    }
	  }
	}
      }
    } // end of anonymous namespace
	  
    Value RetrieveJointNames::doExecute ()
    {
      RosJointState& entity = static_cast<RosJointState&> (owner ());

      std::vector<Value> values = getParameterValues ();
      std::string name = values[0].value ();

      if (!dynamicgraph::PoolStorage::getInstance ()->existEntity (name))
	{
	  std::cerr << "invalid entity name" << std::endl;
	  return Value ();
	}

      dynamicgraph::sot::DynamicPinocchio* dynamic =
	dynamic_cast<dynamicgraph::sot::DynamicPinocchio*>
	(&dynamicgraph::PoolStorage::getInstance ()->getEntity (name));
      if (!dynamic)
	{
	  std::cerr << "entity is not a DynamicPinocchio entity" << std::endl;
	  return Value ();
	}

      se3::Model* robot_model = dynamic->m_model;
      if (robot_model->njoints == 1)
	{
	  std::cerr << "no robot in the dynamic entity" << std::endl;
	  return Value ();
	}

      entity.jointState ().name.resize (robot_model->nv);
      buildJointNames (entity.jointState (), robot_model);
      return Value ();
    }
  } // end of namespace command.

  RosJointState::RosJointState (const std::string& n)
    : Entity (n),
      // do not use callbacks, so do not create a useless spinner
      nh_ (rosInit (false)),
      state_ (0, MAKE_SIGNAL_STRING(name, true, "Vector", "state")),
      publisher_ (nh_, "/joint_states", 5),
      jointState_ (),
      trigger_ (boost::bind (&RosJointState::trigger, this, _1, _2),
		sotNOSIGNAL,
		MAKE_SIGNAL_STRING(name, true, "int", "trigger")),
      rate_ (ROS_JOINT_STATE_PUBLISHER_RATE),
      lastPublicated_ ()
  {
    try {
      lastPublicated_ = ros::Time::now ();
    } catch (const std::exception& exc) {
      throw std::runtime_error ("Failed to call ros::Time::now ():" +
				std::string (exc.what ()));
    }
    signalRegistration (state_ << trigger_);
    trigger_.setNeedUpdateFromAllChildren (true);

    // Fill header.
    jointState_.header.seq = 0;
    jointState_.header.stamp.sec = 0;
    jointState_.header.stamp.nsec = 0;
    jointState_.header.frame_id = "";

    std::string docstring =
      "\n"
      "  Retrieve joint names using robot model contained in a Dynamic entity\n"
      "\n"
      "  Input:\n"
      "    - dynamic entity name (i.e. robot.dynamic.name)\n"
      "\n";
    addCommand ("retrieveJointNames",
		new command::RetrieveJointNames (*this, docstring));
  }

  RosJointState::~RosJointState ()
  {}

  int&
  RosJointState::trigger (int& dummy, int t)
  {
    ros::Duration dt = ros::Time::now () - lastPublicated_;
    if (dt > rate_ && publisher_.trylock ())
      {
	lastPublicated_ = ros::Time::now ();

	// State size without the free floating.
	std::size_t s = state_.access (t).size ();

	// Safety check: if data are inconsistent, clear
	// the joint names to avoid sending erroneous data.
	// This should not happen unless you change
	// the robot model at run-time.
	if (s != jointState_.name.size())
	  jointState_.name.clear();

	// Update header.
	++jointState_.header.seq;

	ros::Time now = ros::Time::now ();
	jointState_.header.stamp.sec = now.sec;
	jointState_.header.stamp.nsec = now.nsec;

	// Fill position.
	jointState_.position.resize (s);
	for (std::size_t i = 0; i < s; ++i)
	  jointState_.position[i] = state_.access (t) ((unsigned int)i);
        jointStateCpy_.name.resize(s-6);
        jointStateCpy_.position.resize(s-6);
        jointStateCpy_.header = jointState_.header;

	for (std::size_t i = 0; i < s-6; i++){
	  jointStateCpy_.position[i] = jointState_.position[i+6];
          jointStateCpy_.name[i] = jointState_.name[i+6]; } 

	publisher_.msg_ = jointStateCpy_;
	publisher_.unlockAndPublish ();
      }
    return dummy;
  }
} // end of namespace dynamicgraph.
