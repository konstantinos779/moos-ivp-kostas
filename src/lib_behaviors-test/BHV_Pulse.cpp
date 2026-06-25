/************************************************************/
/*    NAME: Konstantinos Voskakis                                              */
/*    ORGN: MIT                                             */
/*    FILE: BHV_Pulse.cpp                                    */
/*    DATE:                                                 */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include "MBUtils.h"
#include "BuildUtils.h"
#include "BHV_Pulse.h"

using namespace std;

//---------------------------------------------------------------
// Constructor

BHV_Pulse::BHV_Pulse(IvPDomain domain) :
  IvPBehavior(domain)
{
  // Provide a default behavior name
  IvPBehavior::setParam("name", "defaultname");

  // Declare the variables this behavior needs from the MOOSDB
  addInfoVars("WPT_INDEX");
  // Declare the behavior decision space
  m_domain = subDomain(m_domain, "course,speed");

  // Add any variables this behavior needs to subscribe for
  addInfoVars("NAV_X","NAV_Y");
  m_pulse_range    = 10.0;
  m_pulse_duration = 5.0;
  m_wpt_index      = -1;
  m_pulse_time     = 0;
}

//---------------------------------------------------------------
// Procedure: setParam()

bool BHV_Pulse::setParam(string param, string value)
{
  // It is good practice to handle parameters in a case-insensitive way
  //param = tolower(param);

  if (param == "pulse_range") {
    // Convert the string value to a double
    double dval = atof(value.c_str());
    
    // Ensure the value is a valid number and greater than zero
    if (dval > 0 && isNumber(value)) {
      m_pulse_range = dval;
      return true; // Successfully handled
    }
    return false; // Faulty parameter value
  }
  else if (param == "pulse_duration") {
    // Convert the string value to a double
    double dval = atof(value.c_str());
    
    // Ensure the value is a valid number and greater than zero
    if (dval > 0 && isNumber(value)) {
      m_pulse_duration = dval;
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

void BHV_Pulse::onSetParamComplete()
{
}

//---------------------------------------------------------------
// Procedure: onHelmStart()
//   Purpose: Invoked once upon helm start, even if this behavior
//            is a template and not spawned at startup

void BHV_Pulse::onHelmStart()
{
}

//---------------------------------------------------------------
// Procedure: onIdleState()
//   Purpose: Invoked on each helm iteration if conditions not met.

void BHV_Pulse::onIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onCompleteState()

void BHV_Pulse::onCompleteState()
{
}

//---------------------------------------------------------------
// Procedure: postConfigStatus()
//   Purpose: Invoked each time a param is dynamically changed

void BHV_Pulse::postConfigStatus()
{
}

//---------------------------------------------------------------
// Procedure: onIdleToRunState()
//   Purpose: Invoked once upon each transition from idle to run state

void BHV_Pulse::onIdleToRunState()
{
}

//---------------------------------------------------------------
// Procedure: onRunToIdleState()
//   Purpose: Invoked once upon each transition from run to idle state

void BHV_Pulse::onRunToIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onRunState()
//   Purpose: Invoked each iteration when run conditions have been met.

IvPFunction* BHV_Pulse::onRunState()
{
  // 1. Update the vehicle's current position and time from the InfoBuffer
  bool ok_x, ok_y, ok_idx;
  m_osx = getBufferDoubleVal("NAV_X", ok_x);
  m_osy = getBufferDoubleVal("NAV_Y", ok_y);
  m_curr_time = getBufferCurrTime();
  
  // 2. Query the InfoBuffer for the Waypoint behavior's current target index
  double current_wpt_index = 0;
  
  // Only extract the double value if WPT_INDEX has actually arrived in the buffer
  if (getBufferTimeVal("WPT_INDEX") != -1) {
      current_wpt_index = getBufferDoubleVal("WPT_INDEX", ok_idx);
  }

  // 3. Check if the waypoint index has incremented since the last iteration
  if (ok_idx && (current_wpt_index != m_wpt_index)) {
      // Update our internal record of the index
      m_wpt_index = current_wpt_index;
      
      // Schedule the pulse to drop exactly 5 seconds from now
      m_pulse_time = m_curr_time + 5.0; 
  }

  // 4. Check if we have a pending pulse AND if 5 seconds have passed
  if ((m_pulse_time > 0) && (m_curr_time >= m_pulse_time)) {
      
      // Generate the visual range pulse using the snippet provided in the lab
      XYRangePulse pulse;
      pulse.set_x(m_osx);                
      pulse.set_y(m_osy);                
      pulse.set_label("bhv_pulse");
      pulse.set_rad(m_pulse_range);      // Configured via setParam()
      pulse.set_time(m_curr_time);       
      pulse.set_color("edge", "yellow");
      pulse.set_color("fill", "yellow");
      pulse.set_duration(m_pulse_duration); // Configured via setParam()

      std::string spec = pulse.get_spec();
      postMessage("VIEW_RANGE_PULSE", spec);

      // Reset the scheduled time so we don't spam the MOOSDB 
      // on every subsequent iteration
      m_pulse_time = 0; 
  }

  // 5. This behavior does not influence the vehicle's heading/speed
  // so we do not generate an IvP function. Return 0.
  return(0);
}
//---------------------------------------------------------------

