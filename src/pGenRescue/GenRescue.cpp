/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: April 18th, 2022                                */
/************************************************************/

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
    string param  = tolower(biteStringX(sLine, '='));
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
    string param = biteStringX(svector[i], '=');
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
    postShortestPath();
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
    string param = biteStringX(svector[i], '=');
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
    
    // Regenerate and update the path to skip the rescued swimmer
    postShortestPath();
  }
  return true;
}

//---------------------------------------------------------
// Procedure: postShortestPath()

void GenRescue::postShortestPath()
{
  // If path has not been set, determine a random path of 9
  // points, and make a greedy path from ownship start position.
  // Once it has been set, don't change it. But keep posting it
  // once every 20 iterations.

  // 1. If there are no swimmers left, return home
  if(m_swimmers.empty()) {
    Notify("RETURN", "true");
    return;
  }

  // 2. Extract all unrescued swimmers into an unoptimized list
  XYSegList unoptimized_path;
  std::map<std::string, XYPoint>::iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    unoptimized_path.add_vertex(p->second.x(), p->second.y());
  }

  // 3. Optimize the route using your greedyPath algorithm
  // (This requires m_nav_x and m_nav_y to be accurate!)
  XYSegList optimized_path = greedyPath(unoptimized_path, m_nav_x, m_nav_y);

  // 4. Give it a label so pMarineViewer draws it properly
  optimized_path.set_label("swimmer_path");

  // 5. Serialize and post
  std::string spec = optimized_path.get_spec();
  
  Notify("VIEW_SEGLIST", spec);
  Notify("WPT_UPDATE", "points=" + spec); // Make sure WPT_UPDATE matches your .bhv file
}
  
  
//   if(m_path.size() == 0) {
//     XYFieldGenerator generator;
//     generator.addPolygon("-184,-5:-188, -14:-130,-44:-106,-3");
//     generator.addPolygon("-85,-3:-89,-8:-51,-1");
//     generator.addPolygon("-78,-74:-54,-32:-104,-53");
//     generator.setBufferDist(7);
//     generator.setMaxTries(1000);
//     generator.generatePoints(9);
    
//     vector<XYPoint> pts = generator.getPoints();
    
//     for(unsigned int i=0; i<pts.size(); i++) {
//       XYPoint pt = pts[i];
//       m_path.add_vertex(pt.x(), pt.y());
//     }
//     // Seglist needs a name, refer when drawging and erasing
//     m_path.set_label("one");    
//     XYSegList segl;
//     segl.add_vertex(m_nav_x, m_nav_y);

//     m_path = greedyPath(m_path, m_nav_x, m_nav_y);
    
//     // Seglist needs a name, refer when drawging and erasing
//     segl.set_label("one");
//   }
  
//   Notify("VIEW_SEGLIST", m_path.get_spec());

//   string update_var = "SURVEY_UPDATE";
//   string update_str = "points = " + m_path.get_spec_pts();

//   Notify(update_var, update_str);
//   reportEvent("SURVEY_UPDATE=" + update_str);
// }
// //---------------------------------------------------------
// // Procedure: generateAndPostPath()
// //   Purpose: Generates a waypoint trajectory of all remaining
//             //swimmers to dynamically update the helm, or commands
//             //a return home if none remain. 

// void GenRescue::generateAndPostPath()
// {
//   // EDGE CASE: All known swimmers have been rescued.
//   if(m_swimmers.empty()) {
//     // Tell the helm to transition to the return mode
//     Notify("RETURN", "true");
//     return;
//   }

//   // 2. Instantiate a geometry object to hold the sequence of waypoints
//   XYSegList seglist;
  
//   // Loop through all remaining swimmers and add them to the path
//   std::map<std::string, XYPoint>::iterator p;
//   for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
//     // Extract the X and Y coordinates from the XYPoint object
//     double x = p->second.x();
//     double y = p->second.y();
    
//     // Add the coordinate as a vertex in the trajectory
//     seglist.add_vertex(x, y);
//   }

//   // 4. Give the path a unique label so pMarineViewer knows how to draw/update it
//   seglist.set_label("swimmer_path");

//   // 5. Serialize the geometry object into a string format
//   std::string spec = seglist.get_spec();

//   // 6. Publish to the GUI to draw the line on your screen
//   Notify("VIEW_SEGLIST", spec);

//   // 7. Publish to the MOOSDB to update the active Waypoint behavior in the IvP Helm
//   Notify("WPT_UPDATE", "points=" + spec);
// }


//---------------------------------------------------------
// Procedure: postNullPath()
//   Purpose: If a found swimmer represents the last swimmer
//            to be found, then post a survey update essentially
// 

void GenRescue::postNullPath()
{
#if 0
  if(!m_nav_x_set || !m_nav_y_set)
    return;
  if(m_map_pts.size() != 0)
    return;
  
  XYSegList segl;
  segl.add_vertex(m_nav_x, m_nav_y);
  
  // Seglist needs a name, refer when drawging and erasing
  segl.set_label("one");
  Notify("VIEW_SEGLIST", segl.get_spec());

  string update_var = "SURVEY_UPDATE";
  string update_str = "points = " + segl.get_spec_pts();

  Notify(update_var, update_str);
  reportEvent("SURVEY_UPDATE=" + update_str);
#endif
}


//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Total Active Swimmers:  " << m_swimmers.size() << endl;
  m_msgs << "Total Rescued Swimmers: " << m_rescued.size() << endl;

  return(true);
}
