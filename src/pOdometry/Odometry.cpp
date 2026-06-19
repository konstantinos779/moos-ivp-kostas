/************************************************************/
/*    NAME: konstantinos779                                 */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.cpp                                    */
/*    DATE: December 29th, 1963                             */
/************************************************************/
#include <cmath>
#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "Odometry.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

Odometry::Odometry()
{
  m_first_reading = true;
  m_current_x = 0.0;
  m_current_y = 0.0;
  m_previous_x = 0.0;
  m_previous_y = 0.0;
  m_total_distance = 0.0;
  m_last_nav_time = 0.0;
  m_stale_thresh = 10.0;
  m_alt_unit = "none";  // Default indicates no extra publication
  m_alt_unit_mult = 1.0;
  m_x_updated = false;
  m_y_updated = false;
  m_depth_thresh = 0.0;
  m_current_depth = 0.0;
  m_dist_at_depth = 0.0;
}

//---------------------------------------------------------
// Destructor

Odometry::~Odometry()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool Odometry::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);


  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();

    if (key == "NAV_X" && msg.IsDouble()) {
        m_current_x = msg.GetDouble();
        m_last_nav_time = MOOSTime();
        m_x_updated = true;
    }
    else if (key == "NAV_Y" && msg.IsDouble()) {
        m_current_y = msg.GetDouble();
        m_last_nav_time = MOOSTime();
        m_y_updated = true;
    }
    else if (key == "ALT_UNIT" && msg.IsString()) {
        m_alt_unit = msg.GetString();
    }
    else if (key == "ALT_UNIT_MULT" && msg.IsDouble()) {
        m_alt_unit_mult = msg.GetDouble();
    }
    else if (key == "NAV_DEPTH" && msg.IsDouble()) {
        m_current_depth = msg.GetDouble();
}
    else if (key != "APPCAST_REQ") {
        reportRunWarning("Unhandled Mail: " + key);
    }
  }
    // The moment we have a matching pair of X and Y, calculate the leg!
    if (m_x_updated && m_y_updated) {
        if (m_first_reading) {
            m_previous_x = m_current_x;
            m_previous_y = m_current_y;
            m_first_reading = false;
        } else {
            double leg_dist = std::sqrt((m_current_x - m_previous_x) * (m_current_x - m_previous_x) +
                                        (m_current_y - m_previous_y) * (m_current_y - m_previous_y));

            m_total_distance += leg_dist;
            
            if (m_current_depth > m_depth_thresh) {
                m_dist_at_depth += leg_dist;
            }
            // You can safely publish LEG_DIST here for uXMS/pMarineViewer
            Notify("LEG_DIST", leg_dist);
            // Publish the new variable to the MOOSDB
Notify("ODOMETRY_DIST_AT_DEPTH", m_dist_at_depth);

            m_previous_x = m_current_x;
            m_previous_y = m_current_y;
        }
        // Reset flags to wait for the next safe pair
        m_x_updated = false;
        m_y_updated = false;
    }
  
  return(true);
}
//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool Odometry::OnConnectToServer()
{
  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool Odometry::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // 1. Staleness Check
  if (m_last_nav_time > 0) {
      double time_diff = MOOSTime() - m_last_nav_time;
      string warning_msg = "NAV info is stale! No updates in " + doubleToString(m_stale_thresh) + " seconds.";

      if (time_diff >= m_stale_thresh) {
          reportRunWarning(warning_msg);
      } else {
          retractRunWarning(warning_msg);
      }
  }

  // 2. Publish final distance once per Iterate cycle
  Notify("ODOMETRY_DIST", m_total_distance);

  // 3. Publish Bonus Units
  if (m_alt_unit != "none") {
      double alt_dist = m_total_distance * m_alt_unit_mult;
      string var_name = "ODOMETRY_DIST_" + toupper(m_alt_unit);
      Notify(var_name, alt_dist);
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}
//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Odometry::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;

  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = toupper(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if(param == "STALE_THRESH") {
      double thresh = atof(value.c_str());
      // Validate that the number is greater than zero
      if(thresh > 0) {
        m_stale_thresh = thresh;
      } else {
        // Produce a specific configuration warning if faulty
        reportConfigWarning("Problem with STALE_THRESH. Must be > 0. Got: " + value);
      }
    // Set handled to true whether the value was good or bad, 
    // because we *did* recognize the parameter name.
      handled = true; 
    }
    else if(param == "ALT_UNIT") {
      m_alt_unit = value;
      handled = true;
    }
    else if(param == "ALT_UNIT_MULT") {
      m_alt_unit_mult = atof(value.c_str());
      handled = true;
    }
    else if(param == "DEPTH_THRESH") {
    m_depth_thresh = atof(value.c_str());
    handled = true;
    }
    if(!handled)
      reportUnhandledConfigWarning(orig);
  }
  
  registerVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void Odometry::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("ALT_UNIT", 0);
  Register("ALT_UNIT_MULT", 0);
  Register("NAV_DEPTH", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "Odometry Report                             " << endl;
  m_msgs << "============================================" << endl;

  // Print the total distance to the AppCast window
  m_msgs << "Total Distance: " << m_total_distance << " meters" << endl;

  // (Optional) Print your bonus alternative units too!
  if (m_alt_unit != "none") {
      double alt_dist = m_total_distance * m_alt_unit_mult;
      m_msgs << "Total Distance (" << m_alt_unit << "): " << alt_dist << endl;
  }
  
  m_msgs << "Total Distance:    " << m_total_distance << endl;
  m_msgs << "Depth Threshold:   " << m_depth_thresh << endl;
  m_msgs << "Distance at Depth: " << m_dist_at_depth << endl;
  return(true);
}