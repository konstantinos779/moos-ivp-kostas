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
#include <algorithm>

using namespace std;

//---------------------------------------------------------
// Constructor()

GenPath::GenPath()
{
  m_nav_x = 0;
  m_nav_y = 0;
  m_total_points = 0;
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
        m_total_points++;
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
  
  // ---------------------------------------------------------
  // 1. Cross off points as the vehicle visits them
  // ---------------------------------------------------------
  std::vector<XYPoint>::iterator it = m_visit_points.begin();
  while (it != m_visit_points.end()) {
    double dist = hypot(m_nav_x - it->x(), m_nav_y - it->y());
    
    // If we are within the visit radius, erase the point!
    if (dist <= m_visit_radius) {
      it = m_visit_points.erase(it); 
    } else {
      ++it;
    }
  }

  // ---------------------------------------------------------
  // 2. Generate and optimize the path if ready
  // ---------------------------------------------------------
  if (m_all_points_received && !m_path_generated) {
    
    double current_x = m_nav_x;
    double current_y = m_nav_y;

    // Use a temporary copy of unvisited points for processing
    std::vector<XYPoint> temp_points = m_visit_points;
    std::vector<XYPoint> tour;

    // A. Greedy shortest path initial pass
    while (!temp_points.empty()) {
      int closest_idx = 0;
      double min_dist = -1;

      for (unsigned int i = 0; i < temp_points.size(); i++) {
        double dist = hypot(current_x - temp_points[i].x(), 
                            current_y - temp_points[i].y());
        if (min_dist == -1 || dist < min_dist) {
          min_dist = dist;
          closest_idx = i;
        }
      }

      XYPoint closest_pt = temp_points[closest_idx];
      tour.push_back(closest_pt);

      current_x = closest_pt.x();
      current_y = closest_pt.y();

      temp_points.erase(temp_points.begin() + closest_idx);
    }

    // B. 2-opt "Untwisting" Algorithm
    bool improvement = true;
    while (improvement && tour.size() > 2) {
      improvement = false;
      
      // Loop through all pairs of non-adjacent edges
      for (int i = 1; i < tour.size() - 1; i++) {
        for (int k = i + 1; k < tour.size(); k++) {
          
          // Calculate distance of the two current edges
          double dist_current = hypot(tour[i-1].x() - tour[i].x(), tour[i-1].y() - tour[i].y()) +
                                hypot(tour[k-1].x() - tour[k].x(), tour[k-1].y() - tour[k].y());

          // Calculate the distance if we swapped them
          double dist_new = hypot(tour[i-1].x() - tour[k-1].x(), tour[i-1].y() - tour[k-1].y()) +
                            hypot(tour[i].x() - tour[k].x(), tour[i].y() - tour[k].y());

          // If the new distance is strictly shorter, untwist the path!
          if (dist_new < dist_current) {
            std::reverse(tour.begin() + i, tour.begin() + k);
            improvement = true;
          }
        }
      }
    }

    // C. Build the XYSegList and Update the Helm
    XYSegList my_seglist;
    for (unsigned int i = 0; i < tour.size(); i++) {
      my_seglist.add_vertex(tour[i].x(), tour[i].y());
    }

    // Build the string specification and post it to the helm behavior
    std::string update_str = "points = ";
    update_str += my_seglist.get_spec();
    
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
  // Calculate visited points by subtracting the unvisited vector size from your total
  int unvisited_count = m_visit_points.size();
  int visited_count   = m_total_points - unvisited_count;

  m_msgs << "============================================" << endl;
  m_msgs << "Visit Radius:            " << m_visit_radius << endl;
  m_msgs << "Total Points Received:   " << m_total_points << endl;
  m_msgs << "Invalid Points Received: 0" << endl;
  m_msgs << "First Point Received:    " << (m_total_points > 0 ? "true" : "false") << endl;
  m_msgs << "Last Point Received:     " << (m_all_points_received ? "true" : "false") << endl;
  m_msgs << "NAV_X/Y Received:        true" << endl; 
  m_msgs << "" << endl;
  m_msgs << "Tour Status" << endl;
  m_msgs << "------------------------" << endl;
  m_msgs << "  Points Visited:        " << visited_count << endl;
  m_msgs << "  Points Unvisited:      " << unvisited_count << endl;

  return(true);
}




