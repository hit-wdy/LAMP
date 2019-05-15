/* 
Generic solver class 
author: Yun Chang, Luca Carlone
*/

#include <generic_solver/GenericSolver.h>

GenericSolver::GenericSolver(int solvertype): 
  nfg_(gtsam::NonlinearFactorGraph()),
  values_(gtsam::Values()),
  solver_type_(solvertype),
  nfg_odom_(gtsam::NonlinearFactorGraph()),
  nfg_lc_(gtsam::NonlinearFactorGraph()) {

  std::cout << "instantiated generic solver." << std::endl; 
}


void GenericSolver::regularUpdate(gtsam::NonlinearFactorGraph nfg, 
                           gtsam::Values values, 
                           gtsam::FactorIndices factorsToRemove) {
  // remove factors
  for (size_t index : factorsToRemove) {
    nfg_[index].reset();
  }

  // add new values and factors
  nfg_.add(nfg);
  values_.insert(values);
  bool do_optimize = true; 

  // print number of loop closures
  // std::cout << "number of loop closures so far: " << nfg_.size() - values_.size() << std::endl; 

  if (values.size() > 1) {ROS_WARN("Unexpected behavior: number of update poses greater than one.");}

  if (nfg.size() > 1) {ROS_WARN("Unexpected behavior: number of update factors greater than one.");}

  if (nfg.size() == 0 && values.size() > 0) {ROS_ERROR("Unexpected behavior: added values but no factors.");}

  // Do not optimize for just odometry additions 
  // odometry values would not have prefix 'l' unlike artifact values
  if (nfg.size() == 1 && values.size() == 1) {
    const gtsam::Symbol symb(values.keys()[0]); 
    if (symb.chr() != 'l') {do_optimize = false;}
  }

  // nothing added so no optimization 
  if (nfg.size() == 0 && values.size() == 0) {do_optimize = false;}

  if (factorsToRemove.size() > 0) 
    do_optimize = true;

  if (do_optimize) {
    ROS_INFO(">>>>>>>>>>>> Run Optimizer <<<<<<<<<<<<");
    // optimize
    if (solver_type_ == 1) {
      gtsam::LevenbergMarquardtParams params;
      params.setVerbosityLM("SUMMARY");
      std::cout << "Running LM" << std::endl; 
      params.diagonalDamping = true; 
      values_ = gtsam::LevenbergMarquardtOptimizer(nfg_, values_, params).optimize();
    }else if (solver_type_ == 2) {
      gtsam::GaussNewtonParams params;
      params.setVerbosity("ERROR");
      std::cout << "Running GN" << std::endl; 
      values_ = gtsam::GaussNewtonOptimizer(nfg_, values_, params).optimize();
    }else if (solver_type_ == 3) {
      // something
    }
  }
}

void GenericSolver::initializePrior(gtsam::PriorFactor<gtsam::Pose3> prior_factor) {
  gtsam::Pose3 initial_value = prior_factor.prior();
  gtsam::Matrix covar =
      gtsam::inverse(boost::dynamic_pointer_cast<gtsam::noiseModel::Gaussian>
      (prior_factor.get_noiseModel())->R()); // return covariance matrix
  gtsam::Key initial_key = prior_factor.front(); // CHECK if correct 

  // construct initial pose with covar 
  graph_utils::PoseWithCovariance initial_pose; 
  initial_pose.pose = initial_value;
  initial_pose.covariance_matrix = covar; 
  graph_utils::TrajectoryPose init_trajpose; 
  init_trajpose.pose = initial_pose; 
  init_trajpose.id = initial_key;

  // populate posesAndCovariances_odom_
  posesAndCovariances_odom_.trajectory_poses[initial_key].pose = initial_pose;
  posesAndCovariances_odom_.start_id = initial_key;
  posesAndCovariances_odom_.end_id = initial_key;
}

void GenericSolver::updateOdom(gtsam::BetweenFactor<gtsam::Pose3> odom_factor, 
                               graph_utils::PoseWithCovariance &new_pose){

  // update posesAndCovariances_odom_ (compose last value with new odom value)
  
  // first get measurement and covariance and key from factor
  gtsam::Pose3 delta = odom_factor.measured(); 
  gtsam::Matrix covar =
      gtsam::inverse(boost::dynamic_pointer_cast<gtsam::noiseModel::Gaussian>
      (odom_factor.get_noiseModel())->R()); // return covariance matrix
  gtsam::Key new_key = odom_factor.back();

  // construct pose with covariance for odometry measurement 
  graph_utils::PoseWithCovariance odom_delta; 
  odom_delta.pose = delta; 
  odom_delta.covariance_matrix = covar; 

  // Now get the latest pose in trajectory and compose 
  gtsam::Key latest_key = posesAndCovariances_odom_.end_id; 
  graph_utils::PoseWithCovariance last_pose = 
      posesAndCovariances_odom_.trajectory_poses.at(latest_key).pose; 
  // compose latest pose to odometry for new pose
  graph_utils::poseCompose(last_pose, odom_delta, new_pose);

  // update trajectory 
  posesAndCovariances_odom_.end_id = new_key; // update end key 
  // add to trajectory 
  graph_utils::TrajectoryPose new_trajectorypose; 
  new_trajectorypose.pose = new_pose;
  new_trajectorypose.id = new_key;
  posesAndCovariances_odom_.trajectory_poses[new_key] = new_trajectorypose; 
}

bool GenericSolver::isOdomConsistent(gtsam::BetweenFactor<gtsam::Pose3> lc_factor) {
  // assume loop is between pose i and j
  // extract the keys 
  gtsam::Key key_i = lc_factor.front();
  gtsam::Key key_j = lc_factor.back();
  
  graph_utils::PoseWithCovariance pij_odom, pij_lc, result;

  // access (T_i,Cov_i) and (T_j, Cov_j) from trajectory_
  graph_utils::PoseWithCovariance pi_odom, pj_odom; 
  pi_odom = posesAndCovariances_odom_.trajectory_poses[key_i].pose;
  pj_odom = posesAndCovariances_odom_.trajectory_poses[key_j].pose;

  // compute Tij_odom = T_i.between(T_j); compute Covij_odom = Cov_j - Cov_i (Yun: verify if true)  
  // compute pij_odom = (Tij_odom, Covij_odom)
  graph_utils::poseBetween(pi_odom, pj_odom, pij_odom);

  // get pij_lc = (Tij_lc, Covij_lc) from factor
  pij_lc.pose = lc_factor.measured(); 
  pij_lc.covariance_matrix =
      gtsam::inverse(boost::dynamic_pointer_cast<gtsam::noiseModel::Gaussian>
      (lc_factor.get_noiseModel())->R()); // return covariance matrix

  // check consistency (Tij_odom,Cov_ij_odom, Tij_lc, Cov_ij_lc)
  graph_utils::poseBetween(pij_odom, pij_lc, result);

  result.pose.print("odom consistency check ");
  gtsam::Vector6 consistency_error = gtsam::Pose3::Logmap(result.pose);
  ROS_INFO_STREAM("odometry consistency error: " << consistency_error); 

  // check with threshold
  return true; // place holder
}

void GenericSolver::findInliers(gtsam::NonlinearFactorGraph &inliers) {
  // * pairwise consistency check (will also compare other loops - if loop fails we still store it, but not include in the optimization)
  // -- add 1 row and 1 column to lc_adjacency_matrix_;
  // -- populate extra row and column by testing pairwise consistency of new lc against all previous ones
  // -- compute max clique
  // -- add loops in max clique to a local variable nfg_good_lc
  // NOTE: this will require a map from rowId (size_t, in adjacency matrix) to slot id (size_t, id of that lc in nfg_lc)
  inliers = nfg_lc_;
}

void GenericSolver::update(gtsam::NonlinearFactorGraph nfg, 
                                 gtsam::Values values, 
                                 gtsam::FactorIndices factorsToRemove){
  // TODO: deal with factorsToRemove
  // check if odometry (compare factor keys) TODO: do we want to give odometry a specific prefix?
  // TODO: right now cant do between chordal factor 
  bool odometry = false; 
  bool loop_closure = false; 
  // test if odometry of loop closure (or neither in which case just do regular update)
  if (nfg.size() == 1 && values.size() == 1) {
    const gtsam::Symbol symb(values.keys()[0]); 
    if (symb.chr() != 'l') {
      boost::shared_ptr<gtsam::BetweenFactor<gtsam::Pose3> > pose3Between =
            boost::dynamic_pointer_cast<gtsam::BetweenFactor<gtsam::Pose3> >(nfg[0]);
      if (pose3Between) {
        odometry = true;
      } else if (posesAndCovariances_odom_.trajectory_poses.size() == 0) {
        // probably a prior factor and initializing CHECK
        gtsam::PriorFactor<gtsam::Pose3> prior_factor =
            *boost::dynamic_pointer_cast<gtsam::PriorFactor<gtsam::Pose3> >(nfg[0]);
        initializePrior(prior_factor);
        ROS_INFO("Initialized prior and trajectory");
      }
    }
  } else if (nfg.size() == 1 && values.size() == 0){
    loop_closure = true; 
  }

  if (odometry) {
    // update posesAndCovariances_odom_;
    graph_utils::PoseWithCovariance new_pose;
    // extract between factor 
    gtsam::BetweenFactor<gtsam::Pose3> nfg_factor =
            *boost::dynamic_pointer_cast<gtsam::BetweenFactor<gtsam::Pose3> >(nfg[0]);

    updateOdom(nfg_factor, new_pose);
    // TODO: compare the new pose from out pose_compose with values pose 
    // should be the same 

    // - store factor in nfg_odom_
    nfg_odom_.add(nfg);

    // - store latest pose in values_ (note: values_ is the optimized estimate, while trajectory is the odom estimate)
    values_.insert(values.keys()[0], new_pose.pose);

    return;

  } else if (loop_closure) { // in this case we should run consistency check to see if loop closure is good
    // * odometric consistency check (will only compare against odometry - if loop fails this, we can just drop it)
    // extract between factor 
    gtsam::BetweenFactor<gtsam::Pose3> nfg_factor =
            *boost::dynamic_pointer_cast<gtsam::BetweenFactor<gtsam::Pose3> >(nfg[0]);

    if (isOdomConsistent(nfg_factor)) {
      nfg_lc_.add(nfg); // add factor to nfg_lc_
    } else {
      return; // discontinue since loop closure not consistent with odometry 
    }

    // // Find inliers with Pairwise consistent measurement set maximization
    gtsam::NonlinearFactorGraph nfg_good_lc; 
    findInliers(nfg_good_lc);

    // * optimize and update values (for now just LM add others later)
    nfg_ = gtsam::NonlinearFactorGraph(); // reset 
    nfg_.add(nfg_odom_);
    nfg_.add(nfg_good_lc);
    gtsam::LevenbergMarquardtParams params;
    params.setVerbosityLM("SUMMARY");
    std::cout << "Running LM" << std::endl; 
    params.diagonalDamping = true; 
    values_ = gtsam::LevenbergMarquardtOptimizer(nfg_, values_, params).optimize();
    return; 

  } else {
    // NOTE this case 
    regularUpdate(nfg, values, factorsToRemove);
  }
}
