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
  m_returning = false;
  m_mid_x = 0.0;
  m_mid_y = 0.0;
  m_region_received = false;
  m_start_x = 0.0;
  m_start_y = 0.0;
  m_outbound_phase = false;
  m_outmost_id = "";
  m_swimmers.clear();
  m_rescued.clear();
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
    // 1. DYNAMIC RETURN TRACKING (No one-way lock)
    if (key == "RETURN") {
      // Allow the flag to dynamically reflect the MOOSDB state
      m_returning = (sval == "true" || sval == "TRUE");
    }
    // 2. FOUND SWIMMER (No longer ignored during return)
    else if (key == "FOUND_SWIMMER") {
      handleMailFoundSwimmer(sval);
    }
    // 3. SWIMMER ALERT (Dynamically abandon return if needed)
    else if (key == "SWIMMER_ALERT") {
      handled = handleMailNewSwimmer(sval);
    }
    // 4. TOUR COMPLETE (The Missed Swimmer Check)
    else if (key == "TOUR_COMPLETE" && (sval == "true" || sval == "TRUE")) {
      if (m_swimmers.empty()) {
        // We got them all! Command the helm to return home.
        Notify("RETURN", "true");
      } else {
        // We missed some! Generate a new path to go back for them.
        generateOptimizedPath();
      }
    }
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }
    else if (key == "RESCUE_REGION") {
      XYPolygon poly = string2Poly(sval);
      if (poly.is_convex()) {
        double min_x = poly.get_min_x();
        double max_x = poly.get_max_x();
        
        // Calculate the absolute East/West halfway line
        m_mid_x = (min_x + max_x) / 2.0;
        m_region_received = true;
      }
    }
     else if (key == "UFRM_FINISHED") {
      string sval = msg.GetString();
      
      if (sval == "true" || sval == "TRUE") {
        // The referee declared the mission finished!
        m_swimmers.clear();       // Erase the stuck last point
        //////////////////////
        //m_returning = true;       // Lock the shield so we ignore late clicks
        Notify("RETURN", "true"); // Send the boat to the dock
      }
    }
    else if (key == "RESCUE_REGION") {
      m_region_poly = string2Poly(sval);
      if (m_region_poly.is_convex()) {
        m_region_received = true;
      }
    }
    else if (key == "STATION_KEEP") {
      // You can store this in a class boolean (e.g., m_station_keeping) 
      // if you want to track it later, or just let it pass to clear the warning!
      bool is_keeping_station = (sval == "true" || sval == "TRUE");
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
  
  if (!m_returning) {
    std::map<std::string, XYPoint>::iterator it;
    for(it = m_swimmers.begin(); it != m_swimmers.end(); it++) {
      string id = it->first;
      double sx = it->second.x();
      double sy = it->second.y();
      
      // Calculate 2D distance between ownship and swimmer
      double dist = hypot(sx - m_nav_x, sy - m_nav_y);

      // If within 10 meters, ask the referee for the rescue
      if (dist <= 10.0) { 
        Notify("RESCUE_REQUEST", "vname=" + m_host_community + ",swimmer_id=" + id);
      }
    }
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp(); 

  STRING_LIST sParams;
  
  // Enable verbatim quoting to ensure the $(START_POS) macro is read correctly
  m_MissionReader.EnableVerbatimQuoting(false);
  
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());
  
  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(stripBlankEnds(biteStringX(sLine, '=')));
    string value  = stripBlankEnds(sLine); // Stripping blanks from the value is safer

    // 1. Keep your existing vname parsing
    if(param == "vname") {
      m_vname = value;
    }
    // 2. Add the start_pos parsing for the dividing line logic
    else if(param == "start_pos") {
      string x_str = biteStringX(value, ',');
      string y_str = value;
      m_start_x = atof(x_str.c_str());
      m_start_y = atof(y_str.c_str());
      m_start_pos_set = true;
    }
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
  Register("TOUR_COMPLETE", 0); 
  Register("RETURN", 0);
  Register("STATION_KEEP", 0); //Keep returning errors

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
    string value = stripBlankEnds(svector[i]);

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

    // 4. If the boat was heading home, ABANDON the return!
    if (m_returning) {
      m_returning = false;
      Notify("RETURN", "false"); 
    }
    Notify("STATION_KEEP", "false"); 
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

// //---------------------------------------------------------
// // Procedure: generateOptimizedPath()
// //   Purpose: Instead of greedy, first try! 

void GenRescue::generateOptimizedPath()
{
  // 0. Safety Checks
  if (m_swimmers.empty()) {
    Notify("RETURN", "true"); 
    return; 
  }
  if (!m_nav_x_set || !m_nav_y_set) {
    return; 
  }

  // Hardcoded Athens Bisector
  double ptA_x = -215.0; double ptA_y = -1.5; 
  double ptB_x = -45.0;  double ptB_y = -39.5; 
  double vx = ptB_x - ptA_x; 
  double vy = ptB_y - ptA_y;
  if (vx == 0 && vy == 0) { vx = 1.0; } 

  // =========================================================
  // 1. INITIAL SETUP: Area Split and Visualization
  // =========================================================
  if (m_outmost_id == "") { 
    
    // Draw the yellow dividing line
    XYSegList div_line;
    div_line.add_vertex(ptA_x, ptA_y);
    div_line.add_vertex(ptB_x, ptB_y);
    div_line.set_label("dividing_line");
    div_line.set_color("edge", "invisible");
    div_line.set_color("vertex", "invisible");
    div_line.set_param("edge_size", "2");
    Notify("VIEW_SEGLIST", div_line.get_spec());

    std::map<std::string, XYPoint> list_A;
    std::map<std::string, XYPoint> list_B;

    for(std::map<string, XYPoint>::iterator it = m_swimmers.begin(); it != m_swimmers.end(); it++) {
      double sx = it->second.x(); double sy = it->second.y();
      double cross_product = vx * (sy - ptA_y) - vy * (sx - ptA_x);
      if (cross_product >= 0) { list_A[it->first] = it->second; } 
      else { list_B[it->first] = it->second; }
    }

    std::map<std::string, XYPoint> outbound_swimmers = (list_A.size() >= list_B.size()) ? list_A : list_B;

    // Find the outmost swimmer from Start Position
    double max_swimmer_dist = -1;
    for(std::map<string, XYPoint>::iterator it = outbound_swimmers.begin(); it != outbound_swimmers.end(); it++) {
      double dist = hypot(it->second.x() - m_nav_x, it->second.y() - m_nav_y);
      if (dist > max_swimmer_dist) {
        max_swimmer_dist = dist;
        m_outmost_id = it->first; 
      }
    }
    m_outbound_phase = true;
  }
  
  // =========================================================
  // 2. PHASE CHECK: Has the Outmost Swimmer been rescued?
  // =========================================================
  if (m_outbound_phase && m_swimmers.count(m_outmost_id) == 0) {
    m_outbound_phase = false; 
  }

  // =========================================================
  // 3. ROUTING: Populate the Unified 'tour' Vector
  // =========================================================
  std::vector<XYPoint> tour;
  double curr_x = m_nav_x;
  double curr_y = m_nav_y;
  double init_nav_x = m_nav_x; // Saved specifically for corner-cutting!
  double init_nav_y = m_nav_y;

  if (m_outbound_phase) {
    std::map<std::string, XYPoint> list_A;
    std::map<std::string, XYPoint> list_B;

    // Re-evaluate left/right sides dynamically
    for(std::map<string, XYPoint>::iterator it = m_swimmers.begin(); it != m_swimmers.end(); it++) {
      double sx = it->second.x(); double sy = it->second.y();
      if (vx * (sy - ptA_y) - vy * (sx - ptA_x) >= 0) { list_A[it->first] = it->second; } 
      else { list_B[it->first] = it->second; }
    }

    std::map<std::string, XYPoint> outbound_swimmers;
    std::map<std::string, XYPoint> inbound_swimmers;

    if (list_A.count(m_outmost_id) > 0) {
      outbound_swimmers = list_A;  inbound_swimmers = list_B;
    } else {
      outbound_swimmers = list_B;  inbound_swimmers = list_A;
    }

    // A. Greedy Route Outbound
    while (!outbound_swimmers.empty()) {
      std::string closest_id = ""; double closest_dist = -1;
      for(std::map<string, XYPoint>::iterator it = outbound_swimmers.begin(); it != outbound_swimmers.end(); it++) {
        double dist = hypot(it->second.x() - curr_x, it->second.y() - curr_y);
        if (closest_id == "" || dist < closest_dist) { closest_id = it->first; closest_dist = dist; }
      }
      tour.push_back(outbound_swimmers[closest_id]);
      curr_x = outbound_swimmers[closest_id].x(); curr_y = outbound_swimmers[closest_id].y();
      outbound_swimmers.erase(closest_id);
    }

    // B. Greedy Route Inbound
    while (!inbound_swimmers.empty()) {
      std::string closest_id = ""; double closest_dist = -1;
      for(std::map<string, XYPoint>::iterator it = inbound_swimmers.begin(); it != inbound_swimmers.end(); it++) {
        double dist = hypot(it->second.x() - curr_x, it->second.y() - curr_y);
        if (closest_id == "" || dist < closest_dist) { closest_id = it->first; closest_dist = dist; }
      }
      tour.push_back(inbound_swimmers[closest_id]);
      curr_x = inbound_swimmers[closest_id].x(); curr_y = inbound_swimmers[closest_id].y();
      inbound_swimmers.erase(closest_id);
    }
  } 
  else {
    // C. Greedy Route Merged (Post Outbound Phase)
    std::map<std::string, XYPoint> all_swimmers = m_swimmers;
    while (!all_swimmers.empty()) {
      std::string closest_id = ""; double closest_dist = -1;
      for(std::map<string, XYPoint>::iterator it = all_swimmers.begin(); it != all_swimmers.end(); it++) {
        double dist = hypot(it->second.x() - curr_x, it->second.y() - curr_y);
        if (closest_id == "" || dist < closest_dist) { closest_id = it->first; closest_dist = dist; }
      }
      tour.push_back(all_swimmers[closest_id]);
      curr_x = all_swimmers[closest_id].x(); curr_y = all_swimmers[closest_id].y();
      all_swimmers.erase(closest_id);
    }
  }

  // =========================================================
  // 4. 2-OPT "UNTWISTING" ALGORITHM (Applied to 'tour')
  // =========================================================
  bool improvement = true;
  while (improvement && tour.size() > 2) {
    improvement = false;
    for (unsigned int i = 1; i < tour.size() - 1; i++) {
      for (unsigned int k = i + 1; k < tour.size(); k++) {
        double dist_current = hypot(tour[i-1].x() - tour[i].x(), tour[i-1].y() - tour[i].y()) +
                              hypot(tour[k-1].x() - tour[k].x(), tour[k-1].y() - tour[k].y());
        double dist_new = hypot(tour[i-1].x() - tour[k-1].x(), tour[i-1].y() - tour[k-1].y()) +
                          hypot(tour[i].x() - tour[k].x(), tour[i].y() - tour[k].y());
        if (dist_new < dist_current) {
          std::reverse(tour.begin() + i, tour.begin() + k);
          improvement = true;
        }
      }
    }
  }

  // =========================================================
  // 5. CORNER-CUTTING (Offsetting the points inward)
  // =========================================================
  double offset_dist = 2.8; // meters to offset inward
  XYSegList my_seglist;

  for (unsigned int i = 0; i < tour.size(); i++) {
    double cx = tour[i].x();
    double cy = tour[i].y();

    // Calculate the turn bisector to pull the point inward
    if (i < tour.size() - 1) {
      double px = (i == 0) ? init_nav_x : tour[i-1].x();
      double py = (i == 0) ? init_nav_y : tour[i-1].y();
      double nx = tour[i+1].x();
      double ny = tour[i+1].y();

      double dist_to_prev = hypot(cx - px, cy - py);
      double dist_to_next = hypot(nx - cx, ny - cy);

      if (dist_to_prev > 0.1 && dist_to_next > 0.1) {
        double v_prev_x = (px - cx) / dist_to_prev;
        double v_prev_y = (py - cy) / dist_to_prev;
        double v_next_x = (nx - cx) / dist_to_next;
        double v_next_y = (ny - cy) / dist_to_next;

        double bisect_x = v_prev_x + v_next_x;
        double bisect_y = v_prev_y + v_next_y;

        double b_mag = hypot(bisect_x, bisect_y);
        if (b_mag > 0.1) {
          cx += (bisect_x / b_mag) * offset_dist;
          cy += (bisect_y / b_mag) * offset_dist;
        }
      }
    }
    my_seglist.add_vertex(cx, cy); 
  }

  // 6. Command the Helm
  string update_str = "points = " + my_seglist.get_spec();
  Notify("SURVEY_UPDATE", update_str);
}
//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Total Active Swimmers:  " << m_swimmers.size() << endl;
  m_msgs << "Total Rescued Swimmers: " << m_rescued.size() << endl;

  return(true);
}
