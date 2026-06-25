/************************************************************/
/*    NAME: Konstantinos Voskakis                                              */
/*    ORGN: MIT                                             */
/*    FILE: BHV_ZigLeg.cpp                                    */
/*    DATE:                                                 */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include "MBUtils.h"
#include "BuildUtils.h"
#include "BHV_ZigLeg.h"
#include "ZAIC_PEAK.h"

using namespace std;

//---------------------------------------------------------------
// Constructor
BHV_ZigLeg::BHV_ZigLeg(IvPDomain domain) : IvPBehavior(domain)
{
  IvPBehavior::setParam("name", "defaultname");

  // Subscribe to the variables we need
  addInfoVars("WPT_INDEX");
  addInfoVars("NAV_HEADING");

  // Initialize your variables
  m_zig_duration   = 10.0;
  m_zig_angle      = 45.0;
  m_zig_start_time = 0;
  m_zig_end_time   = 0;
  m_zig_heading    = 0;
  m_wpt_index      = -1;
}

//---------------------------------------------------------------
// Procedure: setParam()

  bool BHV_ZigLeg::setParam(string param, string value)
{
  // Convert the parameter to lower case for case-insensitivity
  param = tolower(param);

  if (param == "zig_duration") {
    // Convert the string value to a double
    double dval = atof(value.c_str());
    
    // Ensure the value is a valid number and greater than zero
    if (dval > 0 && isNumber(value)) {
      m_zig_duration = dval;
      return true; // Successfully handled
    }
    return false; // Faulty parameter value
  }
  else if (param == "zig_angle") {
    // Convert the string value to a double
    double dval = atof(value.c_str());
    
    // Ensure the value is a valid number
    if (isNumber(value)) {
      m_zig_angle = dval;
      return true; // Successfully handled
    }
    return false; // Faulty parameter value
  }
    // If the parameter was not recognized, return false
  return false;
}

//---------------------------------------------------------------
// Procedure: onSetParamComplete()
//   Purpose: Invoked once after all parameters have been handled.
//            Good place to ensure all required params have are set.
//            Or any inter-param relationships like a<b.

void BHV_ZigLeg::onSetParamComplete()
{
}

//---------------------------------------------------------------
// Procedure: onHelmStart()
//   Purpose: Invoked once upon helm start, even if this behavior
//            is a template and not spawned at startup

void BHV_ZigLeg::onHelmStart()
{
}

//---------------------------------------------------------------
// Procedure: onIdleState()
//   Purpose: Invoked on each helm iteration if conditions not met.

void BHV_ZigLeg::onIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onCompleteState()

void BHV_ZigLeg::onCompleteState()
{
}

//---------------------------------------------------------------
// Procedure: postConfigStatus()
//   Purpose: Invoked each time a param is dynamically changed

void BHV_ZigLeg::postConfigStatus()
{
}

//---------------------------------------------------------------
// Procedure: onIdleToRunState()
//   Purpose: Invoked once upon each transition from idle to run state

void BHV_ZigLeg::onIdleToRunState()
{
}

//---------------------------------------------------------------
// Procedure: onRunToIdleState()
//   Purpose: Invoked once upon each transition from run to idle state

void BHV_ZigLeg::onRunToIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onRunState()
//   Purpose: Invoked each iteration when run conditions have been met.

IvPFunction* BHV_ZigLeg::onRunState()
{
  // 1. Get current time and heading
  double m_curr_time = getBufferCurrTime();
  bool ok_hdg = false;
  double current_heading = getBufferDoubleVal("NAV_HEADING", ok_hdg);

  // 2. Query the InfoBuffer safely for WPT_INDEX
  bool ok_idx = false;
  double current_wpt_index = 0;
  if (getBufferTimeVal("WPT_INDEX") != -1) {
      current_wpt_index = getBufferDoubleVal("WPT_INDEX", ok_idx);
  }

  // 3. Start the timers when the waypoint increments
  if (ok_idx && ok_hdg && (current_wpt_index != m_wpt_index)) {
      m_wpt_index = current_wpt_index;
      
      // Calculate start time (5 seconds from now), end time, and target heading
      m_zig_start_time = m_curr_time + 5.0;
      m_zig_end_time   = m_zig_start_time + m_zig_duration;
      
      // Offset heading by the configured zig angle
      m_zig_heading    = current_heading + m_zig_angle; 
  }

  // 4. If we are inside the zig time window, generate the objective function!
  if ((m_zig_start_time > 0) && (m_curr_time >= m_zig_start_time) && (m_curr_time <= m_zig_end_time)) {
      
      // Use ZAIC_PEAK to generate an IvP Function over the "course" domain
      ZAIC_PEAK zaic_peak(m_domain, "course");
      zaic_peak.setSummit(m_zig_heading);
      zaic_peak.setPeakWidth(85.0);
      zaic_peak.setBaseWidth(180.0);
      zaic_peak.setSummitDelta(0);
      zaic_peak.setValueWrap(true);

      IvPFunction *ipf = zaic_peak.extractIvPFunction();
      
      // CRITICAL: Apply your behavior's priority weight before returning it
      if(ipf) {
          ipf->setPWT(m_priority_wt);
      }
      return(ipf);
  }

  // If outside the window, do nothing
  return(0);
}
//---------------------------------------------------------------

