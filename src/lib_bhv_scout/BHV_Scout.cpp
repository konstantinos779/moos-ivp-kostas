/*****************************************************************/
/*    NAME: M.Benjamin                                           */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.cpp                                        */
/*    DATE: April 30th 2022                                      */
/*****************************************************************/
#include <iterator>
#include <cstdlib>
#include <math.h>
#include "BHV_Scout.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "BuildUtils.h"
#include "GeomUtils.h"
#include "ZAIC_PEAK.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"
#include "NodeRecord.h"
#include "NodeRecordUtils.h"

using namespace std;

//-----------------------------------------------------------
// Constructor()

BHV_Scout::BHV_Scout(IvPDomain gdomain) : 
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "scout");
 
  // Default values for behavior state variables
  m_osx  = 0;
  m_osy  = 0;

  // All distances are in meters, all speed in meters per second
  // Default values for configuration parameters 
  m_desired_speed  = 1; 
  m_capture_radius = 10;

  m_pt_set = false;
  
  addInfoVars("NAV_X, NAV_Y");
  addInfoVars("RESCUE_REGION");
  addInfoVars("SCOUTED_SWIMMER");
  // REQUIRED: We must subscribe to NODE_REPORT to track the teammate's location
  addInfoVars("NODE_REPORT"); 
}

//---------------------------------------------------------------
// Procedure: setParam() - handle behavior configuration parameters

bool BHV_Scout::setParam(string param, string val) 
{
  // Convert the parameter to lower case for more general matching
  param = tolower(param);
  
  bool handled = true;
  if(param == "capture_radius")
    handled = setPosDoubleOnString(m_capture_radius, val);
  else if(param == "desired_speed")
    handled = setPosDoubleOnString(m_desired_speed, val);
  else if(param == "tmate")
    handled = setNonWhiteVarOnString(m_tmate, val);
  else
    handled = false;

  srand(time(NULL));
  
  return(handled);
}

//-----------------------------------------------------------
// Procedure: onEveryState()

void BHV_Scout::onEveryState(string str) 
{
  if(!getBufferVarUpdated("SCOUTED_SWIMMER"))
    return;

  string report = getBufferStringVal("SCOUTED_SWIMMER");
  if(report == "")
    return;

  if(m_tmate == "") {
    postWMessage("Mandatory Teammate name is null");
    return;
  }
  postOffboardMessage(m_tmate, "SWIMMER_ALERT", report);
}

//-----------------------------------------------------------
// Procedure: onIdleState()

void BHV_Scout::onIdleState() 
{
  m_curr_time = getBufferCurrTime();
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction* BHV_Scout::onRunState()
{
  bool ok_x, ok_y, ok_reg, ok_rep, ok_swim;
  
  // 1. Get our ownship position from the InfoBuffer
  m_osx = getBufferDoubleVal("NAV_X", ok_x);
  m_osy = getBufferDoubleVal("NAV_Y", ok_y);
  
  // 2. Establish the search region boundary
  string region_str = getBufferStringVal("RESCUE_REGION", ok_reg);
  if(ok_reg && !m_rescue_region.is_convex()) {
    m_rescue_region = string2Poly(region_str);
  }

  // 3. Parse teammate's position from NODE_REPORT to avoid searching near them
  string rep_str = getBufferStringVal("NODE_REPORT", ok_rep);
  if(ok_rep && m_pt_set) {
    NodeRecord rec = string2NodeRecord(rep_str);
    if(rec.getName() == m_tmate) {
       double dist_teammate_to_target = hypot(m_ptx - rec.getX(), m_pty - rec.getY());
       
       // If our teammate is too close to our current target, abandon it
       if(dist_teammate_to_target < 40.0) {
           m_pt_set = false; 
           postViewPoint(false); 
       }
    }
  }

  // 4. Pick a new target if we reached the current one (using your capture_radius)
  if(m_pt_set) {
    double dist_to_target = hypot(m_ptx - m_osx, m_pty - m_osy);
    if(dist_to_target <= m_capture_radius) {
        m_pt_set = false;
        postViewPoint(false); 
    }
  }
  
  // 5. If we have no target, pick a new one
  if(!m_pt_set) {
      updateScoutPoint();
  }

  // 6. Share Unregistered Swimmers!
  string swimmer_str = getBufferStringVal("SCOUTED_SWIMMER", ok_swim);
  
  // Check if SCOUTED_SWIMMER was received exactly on this iteration
  if (ok_swim && (getBufferTimeVal("SCOUTED_SWIMMER") == getBufferCurrTime())) {
      string msg = "src_node=" + m_us_name + 
                   ",dest_node=" + m_tmate + 
                   ",var_name=SCOUTED_SWIMMER" + 
                   ",string_val=" + swimmer_str;
      
      // Route the message to our teammate
      postMessage("NODE_MESSAGE_LOCAL", msg);
  }

  // 7. Generate the Objective Function using your buildFunction helper
  if(!ok_x || !ok_y || !m_pt_set) {
      return 0;
  }

  IvPFunction *ipf = buildFunction();
  if(ipf) {
      ipf->setPWT(m_priority_wt);
  }
  
  return ipf;
}
//-----------------------------------------------------------
// Procedure: updateScoutPoint()

void BHV_Scout::updateScoutPoint()
{
  if(m_pt_set)
    return;

  string region_str = getBufferStringVal("RESCUE_REGION");
  if(region_str == "")
    postWMessage("Unknown RESCUE_REGION");
  else
    postRetractWMessage("Unknown RESCUE_REGION");

  XYPolygon region = string2Poly(region_str);
  if(!region.is_convex()) {
    postWMessage("Badly formed RESCUE_REGION");
    return;
  }
  m_rescue_region = region;
  
  cout << "updateScoutPoint(): " << endl;
  
  double ptx = 0;
  double pty = 0;
  bool ok = randPointInPoly(m_rescue_region, ptx, pty);
  if(!ok) {
    postWMessage("Unable to generate scout point");
    return;
  }
    
  m_ptx = ptx;
  m_pty = pty;
  m_pt_set = true;
  string msg = "New pt: " + doubleToStringX(ptx) + "," + doubleToStringX(pty);
  postEventMessage(msg);
}

//-----------------------------------------------------------
// Procedure: postViewPoint()

void BHV_Scout::postViewPoint(bool viewable) 
{

 

  XYPoint pt(m_ptx, m_pty);
  pt.set_vertex_size(5);
  pt.set_vertex_color("orange");
  pt.set_label(m_us_name + "'s next waypoint");
  
  string point_spec;
  if(viewable)
    point_spec = pt.get_spec("active=true");
  else
    point_spec = pt.get_spec("active=false");
  postMessage("VIEW_POINT", point_spec);
}


//-----------------------------------------------------------
// Procedure: buildFunction()

// Procedure: buildFunction()

IvPFunction* BHV_Scout::buildFunction()
{
  if(!m_pt_set)
    return(0);
  
  ZAIC_PEAK spd_zaic(m_domain, "speed");
  spd_zaic.setSummit(m_desired_speed);
  spd_zaic.setPeakWidth(0.5);
  spd_zaic.setBaseWidth(1.0);
  
  // FIXED: Drop utility by 50 to assert a strong speed preference!
  spd_zaic.setSummitDelta(50.0);  
  
  if(spd_zaic.stateOK() == false) {
    string warnings = "Speed ZAIC problems " + spd_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }
  
  double rel_ang_to_wpt = relAng(m_osx, m_osy, m_ptx, m_pty);
  ZAIC_PEAK crs_zaic(m_domain, "course");
  crs_zaic.setSummit(rel_ang_to_wpt);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180.0);
  
  // FIXED: Drop utility by 50 to assert a strong heading preference!
  crs_zaic.setSummitDelta(50.0);  
  
  crs_zaic.setValueWrap(true);
  if(crs_zaic.stateOK() == false) {
    string warnings = "Course ZAIC problems " + crs_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }

  IvPFunction *spd_ipf = spd_zaic.extractIvPFunction();
  IvPFunction *crs_ipf = crs_zaic.extractIvPFunction();

  OF_Coupler coupler;
  IvPFunction *ivp_function = coupler.couple(crs_ipf, spd_ipf, 50, 50);

  return(ivp_function);
}
