/************************************************************/
/*    NAME: konstantinos779                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.cpp                                        */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPath.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenPath::GenPath()
{
  m_nav_x = 0;
  m_nav_y = 0;
  m_all_points_received = false;
  m_path_generated = false;
  m_visit_radius = 3.0;
}

//---------------------------------------------------------
// Destructor

GenPath::~GenPath()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();
    string sval  = msg.GetString();
    double dval  = msg.GetDouble();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    if (key == "NAV_X") {
      m_nav_x = dval;
    } 
    else if (key == "NAV_Y") {
      m_nav_y = dval;
    } 
    else if (key == "VISIT_POINT") {
      if (sval == "firstpoint") {
        m_visit_points.clear();
      } 
      else if (sval == "lastpoint") {
        m_all_points_received = true;
      } 
      else {
        // Parse the incoming string "x=8, y=9, id=1"
        string x_str = tokStringParse(sval, "x", ',', '=');
        string y_str = tokStringParse(sval, "y", ',', '=');
        string id_str = tokStringParse(sval, "id", ',', '=');

        XYPoint new_pt(atof(x_str.c_str()), atof(y_str.c_str()));
        new_pt.set_msg(id_str);
        m_visit_points.push_back(new_pt);
      }
    }
    else if(key != "APPCAST_REQ") // handled by AppCastingMOOSApp
    reportRunWarning("Unhandled Mail: " + key);
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenPath::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();
  
  // Only calculate the path once we have all points and haven't generated it yet
  if (m_all_points_received && !m_path_generated) {
    
    XYSegList my_seglist;
    double current_x = m_nav_x;
    double current_y = m_nav_y;

    // Greedy shortest path algorithm
    while (!m_visit_points.empty()) {
      int closest_idx = 0;
      double min_dist = -1;

      for (unsigned int i = 0; i < m_visit_points.size(); i++) {
        // hypot(dx, dy) safely calculates the distance
        double dist = hypot(current_x - m_visit_points[i].x(), 
                            current_y - m_visit_points[i].y());
        if (min_dist == -1 || dist < min_dist) {
          min_dist = dist;
          closest_idx = i;
        }
      }

      // Add the closest point to our segment list
      XYPoint closest_pt = m_visit_points[closest_idx];
      my_seglist.add_vertex(closest_pt.x(), closest_pt.y());

      // Update our "current" position to this new point
      current_x = closest_pt.x();
      current_y = closest_pt.y();

      // Remove the point we just visited from the pending list
      m_visit_points.erase(m_visit_points.begin() + closest_idx);
    }

    // Build the string specification and post it to the helm behavior [1, 2]
    string update_str = "points = ";
    update_str += my_seglist.get_spec();
    
    // NOTE: Match "WPT_UPDATE" to the 'updates' parameter in your waypoint behavior
    Notify("WPT_UPDATE", update_str); 
    
    m_path_generated = true; 
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool GenPath::OnStartUp()
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
    if(param == "VISIT_RADIUS") {
      m_visit_radius = atof(value.c_str());
      handled = true;
    }

    // 5. Report if a parameter was misspelled or unrecognized 
    if(!handled)
      reportUnhandledConfigWarning(orig); 
  }
  
  registerVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
  Register("VISIT_POINT", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool GenPath::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "File:                                       " << endl;
  m_msgs << "============================================" << endl;

  m_msgs << "Points Currently in List: " << m_visit_points.size() << endl;
  m_msgs << "All Points Received:      " << (m_all_points_received ? "true" : "false") << endl;
  m_msgs << "Path Generated:           " << (m_path_generated ? "true" : "false") << endl;
  m_msgs << "Ownship Nav X:            " << m_nav_x << endl;
  m_msgs << "Ownship Nav Y:            " << m_nav_y << endl;
  m_msgs << "Visit Radius:             " << m_visit_radius << endl;

  return(true);
}




