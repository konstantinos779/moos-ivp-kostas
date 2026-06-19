/************************************************************/
/*    NAME: Konstantin778                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.cpp                                        */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#include "MBUtils.h"
#include "ACTable.h"
#include "PointAssign.h"
#include "XYPoint.h"
#include <iterator>

using namespace std;

//---------------------------------------------------------
// Constructor()

PointAssign::PointAssign()
{
  m_assign_by_region = false;
  m_assign_index = 0;
}

//---------------------------------------------------------
// Destructor

PointAssign::~PointAssign()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool PointAssign::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for (p = NewMail.begin(); p != NewMail.end(); p++)
  {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();
    string val = msg.GetString();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

    if (key == "VISIT_POINT")
    {
      // 1. Handle bookend cues (send to all vehicles)
      if (val == "firstpoint" || val == "lastpoint")
      {
        for (unsigned int i = 0; i < m_vnames.size(); i++)
        {
          Notify("VISIT_POINT_" + m_vnames[i], val);
        }
      }
      // 2. Handle actual coordinates
      else if (m_vnames.size() > 0)
      {

        // Parse the X coordinate from the string "x=8, y=9, id=1"
        // Initialize variables to hold extracted data
        double x_pos = 0;
        double y_pos = 0;
        string id_val = "";

        // Extract x, y, and id from the string (e.g., "x=8, y=9, id=1")
        vector<string> svector = parseString(val, ',');
        for (unsigned int i = 0; i < svector.size(); i++)
        {
          string param = tolower(biteStringX(svector[i], '='));
          string value = svector[i];

          if (param == "x")
          {
            x_pos = atof(value.c_str());
          }
          else if (param == "y")
          {
            y_pos = atof(value.c_str());
          }
          else if (param == "id")
          {
            id_val = value;
          }
        }

        string target_vname;
        string color;

        // Mode 1: Assign by region (West/East)
        if (m_assign_by_region)
        {
          if (x_pos < 87.5)
          {
            target_vname = m_vnames[0]; // West vehicle
            color = "yellow";
          }
          else
          {
            if (m_vnames.size() > 1)
            {
              target_vname = m_vnames[1]; // East vehicle
              color = "red";
            }
            else
            {
              target_vname = m_vnames[0];
              color = "yellow";
            }
          }
        }
        // Mode 2: Alternating assignment
        else
        {
          target_vname = m_vnames[m_assign_index];

          // Alternate the colors to match the vehicles
          if (m_assign_index == 0)
            color = "yellow";
          else
            color = "red";

          m_assign_index = (m_assign_index + 1) % m_vnames.size();
        }

        // Convert the lowercase vehicle name to uppercase

        // 1. Post the assigned point to the vehicle
        // Notify("VISIT_POINT_" + toupper(target_vname), val);
        Notify("VISIT_POINT_" + target_vname, val);

        // 2. Post the visual point to pMarineViewer
        // We use the ID as the label to ensure each point is uniquely identified [1]
        postViewPoint(x_pos, y_pos, id_val, color);
      }
    }
    else if (key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
  }
  return (true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool PointAssign::OnConnectToServer()
{
  RegisterVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool PointAssign::Iterate()
{
  AppCastingMOOSApp::Iterate();
  // Do your thing here!
  AppCastingMOOSApp::PostReport();
  return (true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool PointAssign::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if (!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for (p = sParams.begin(); p != sParams.end(); p++)
  {
    string orig = *p;
    string line = *p;
    string param = toupper(biteStringX(line, '='));
    string value = line;

    if (param == "VNAME")
    {
      // Store names in uppercase to easily append to VISIT_POINT_
      m_vnames.push_back(toupper(value));
    }
    else if (param == "ASSIGN_BY_REGION")
    {
      m_assign_by_region = (tolower(value) == "true");
    }
  }

  // if(!handled)
  //   reportUnhandledConfigWarning(orig);

  RegisterVariables();
  return (true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void PointAssign::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);
}

//---------------------------------------------------------
// Procedure: postViewPoint

void PointAssign::postViewPoint(double x, double y, std::string label, std::string color)

{
  XYPoint point(x, y);
  point.set_label(label);
  point.set_color("vertex", color); // yellow is handy on a dark screen
  point.set_param("vertex_size", "4");

  string spec = point.get_spec(); // gets the string representation of a point
  Notify("VIEW_POINT", spec);
}
//------------------------------------------------------------
// Procedure: buildReport()

bool PointAssign::buildReport()
{
  m_msgs << "m_vnames.size():" << m_vnames.size() << endl;
  m_msgs << "m_assign_by_region:" << (m_assign_by_region ? "true" : "false") << endl;

  return (true);
}
