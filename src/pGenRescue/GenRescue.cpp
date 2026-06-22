/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: April 18th, 2022                                */
/************************************************************/

#include <algorithm>    // Required for std::reverse
#include <cmath>        // Required for hypot
#include <vector>       // Required for std::vector
#include <iterator>
#include "GenRescue.h"
#include "MBUtils.h"
#include "ColorParse.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "GeomUtils.h"
#include "PathUtils.h"
#include "XYFormatUtilsPoly.h"
#include "XYFieldGenerator.h"


using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // Initialize state variables
  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = 0;
  m_nav_y_set = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    cout << "I just received mail for: " << key << endl;
    string sval = msg.GetString();

    bool handled = true;
    if(key == "SWIMMER_ALERT") 
      handled = handleMailNewSwimmer(sval);
    else if(key == "FOUND_SWIMMER") 
      handled = handleMailFoundSwimmer(sval);
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }
     else if (key == "UFRM_FINISHED") {
      string sval = msg.GetString();
      
      if (sval == "true" || sval == "TRUE") {
        // The referee declared the mission finished!
        m_swimmers.clear();       // Erase the stuck last point
        //m_returning = true;       // Lock the shield so we ignore late clicks
        Notify("RETURN", "true"); // Send the boat to the dock
      }
    }

    else if(key != "APPCAST_REQ") // handle by AppCastingMOOSApp
      handled = false;
    
    if(!handled)
      reportRunWarning("Unhandled Mail: " + key +"=" + sval);
    
  }
  return(true);
}
 
//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();
  
  //if(m_plan_pending)
  // if((m_iteration % 20) == 0)
  //   postShortestPath();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp(); 

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);
  
  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(stripBlankEnds(biteStringX(sLine, '=')));
    string value  = sLine;
    if(param == "vname")
      m_vname = value;
  }
  
  RegisterVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void GenRescue::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);

  // tracking its own position
  Register("NAV_X", 0);
  Register("NAV_Y", 0);

  // Listen for the official referee game-over signal
  Register("UFRM_FINISHED", 0);
}


//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()

bool GenRescue::handleMailNewSwimmer(string sval)
{
double x = 0; 
  double y = 0; 
  string id = "";

  // Parse the comma-separated string (e.g., "x=23, y=54, id=04")
  vector<string> svector = parseString(sval, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = (stripBlankEnds(biteStringX(svector[i], '=')));
    string value = svector[i];

    if(param == "x") 
      x = atof(value.c_str());
    else if(param == "y") 
      y = atof(value.c_str());
    else if(param == "id") 
      id = value;
  }

  // Check if we already know about this swimmer ID
  if(id != "" && m_swimmers.find(id) == m_swimmers.end()&& 
     m_rescued.find(id) == m_rescued.end()) {
    // It's a NEW swimmer! Add to our record.
    XYPoint new_swimmer(x, y);
    new_swimmer.set_label(id);
    m_swimmers[id] = new_swimmer;
    
    // Regenerate and update the path immediately
    generateOptimizedPath();
  }
  return true;
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()

bool GenRescue::handleMailFoundSwimmer(string sval)
{
  string id = ""; 
  string finder = "";

  // Parse the comma-separated string (e.g., "id=01, finder=abe")
  vector<string> svector = parseString(sval, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = (stripBlankEnds(biteStringX(svector[i], '=')));
    string value = svector[i];

    if(param == "id") 
      id = value;
    else if(param == "finder") 
      finder = value;
  }

  // Check if this rescued swimmer is currently in our active list
  if(id != "" && m_swimmers.find(id) != m_swimmers.end()) {
    
    // Remove the rescued swimmer from our internal map
    m_swimmers.erase(id);

    // Permanently mark this ID as rescued so we never add it back!
    m_rescued[id] = true;
    
    // ONLY recalculate the path if the adversary rescued it!
    // (m_host_community holds this vehicle's name)
    if (finder != m_host_community) {
      generateOptimizedPath();
    }
  }
  
  return true;
}

//---------------------------------------------------------
// Procedure: postShortestPath()

// void GenRescue::postShortestPath()
// {
//   // If path has not been set, determine a random path of 9
//   // points, and make a greedy path from ownship start position.
//   // Once it has been set, don't change it. But keep posting it
//   // once every 20 iterations.

//   // 1. If there are no swimmers left, return home
//   if(m_swimmers.empty()) {
//     Notify("RETURN", "true");
//     return;
//   }

//   // 2. Extract all unrescued swimmers into an unoptimized list
//   XYSegList unoptimized_path;
//   std::map<std::string, XYPoint>::iterator p;
//   for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
//     unoptimized_path.add_vertex(p->second.x(), p->second.y());
//   }

//   // 3. Optimize the route using your greedyPath algorithm
//   // (This requires m_nav_x and m_nav_y to be accurate!)
//   XYSegList optimized_path = greedyPath(unoptimized_path, m_nav_x, m_nav_y);

//   // 4. Give it a label so pMarineViewer draws it properly
//   optimized_path.set_label("swimmer_path");

//   // 5. Serialize and post
//   std::string spec = optimized_path.get_spec();
  
//   Notify("VIEW_SEGLIST", spec);
//   Notify("WPT_UPDATE", "points=" + spec); // Make sure WPT_UPDATE matches your .bhv file
// }
  
// //---------------------------------------------------------
// // Procedure: generateOptimizedPath()
// //   Purpose: Instead of greedy, first try! 

void GenRescue::generateOptimizedPath()
{
  // 1. Check if we have any active swimmers left in the map
  if (m_swimmers.empty()) {
    return; 
  }
    
  double current_x = m_nav_x;
  double current_y = m_nav_y;

  // 2. Build the temporary vector of points directly from your map
  std::vector<XYPoint> temp_points;
  std::map<string, XYPoint>::iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    temp_points.push_back(p->second);
  }

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

    // C. Corner-Cutting (Offsetting the points inward)
    double offset_dist = 3.5; // Keep under 10m to ensure the rescue is triggered!
    XYSegList my_seglist;

    for (unsigned int i = 0; i < tour.size(); i++) {
      double cx = tour[i].x();
      double cy = tour[i].y();

      // We only offset if there is an outgoing leg (not the final point)
      if (i < tour.size() - 1) {
        
        // 1. Define the previous point (either the vehicle, or the previous waypoint)
        double px = (i == 0) ? current_x : tour[i-1].x();
        double py = (i == 0) ? current_y : tour[i-1].y();
        
        // 2. Define the next point
        double nx = tour[i+1].x();
        double ny = tour[i+1].y();

        double dist_to_prev = hypot(cx - px, cy - py);
        double dist_to_next = hypot(nx - cx, ny - cy);

        // 3. Calculate unit vectors pointing AWAY from the corner
        if (dist_to_prev > 0.1 && dist_to_next > 0.1) {
          double v_prev_x = (px - cx) / dist_to_prev;
          double v_prev_y = (py - cy) / dist_to_prev;

          double v_next_x = (nx - cx) / dist_to_next;
          double v_next_y = (ny - cy) / dist_to_next;

          // 4. Adding them yields the bisector pointing directly into the turn interior
          double bisect_x = v_prev_x + v_next_x;
          double bisect_y = v_prev_y + v_next_y;

          double b_mag = hypot(bisect_x, bisect_y);
          if (b_mag > 0.1) {
            // Apply the offset to the point
            cx += (bisect_x / b_mag) * offset_dist;
            cy += (bisect_y / b_mag) * offset_dist;
          }
        }
      }
      
      // D. Build the XYSegList with the new offset points
      my_seglist.add_vertex(cx, cy);
    }

    std::string update_str = "points = ";
    update_str += my_seglist.get_spec(); 
    Notify("WPT_UPDATE", update_str);
}

//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Total Active Swimmers:  " << m_swimmers.size() << endl;
  m_msgs << "Total Rescued Swimmers: " << m_rescued.size() << endl;

  return(true);
}
