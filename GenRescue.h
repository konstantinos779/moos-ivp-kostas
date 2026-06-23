/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.h                                     */
/*    DATE: April 18th, 2022                                */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include <map>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYPolygon.h"
#include "XYFormatUtilsPoly.h"


class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {};

 protected:
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();
  void RegisterVariables();
  
 protected:
  bool handleMailNewSwimmer(std::string sval);
  bool handleMailFoundSwimmer(std::string sval);
  bool handleMailRescueRegion(std::string sval);
  void postShortestPath();
  void postNullPath();

 private: // Config variables
  std::string m_vname;
  
 private: // State variables
  XYSegList  m_path;
  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;
  bool       m_returning;
  double     m_mid_x;
  double     m_mid_y;
  bool      m_start_pos_set;
  bool      m_region_received;
  double     m_start_x;          // To lock in your x-$START position
  double     m_start_y;          // To lock in your y-$START position
  XYPolygon  m_region_poly;      // To hold the area coordinates
  // The swimmer tracking map of unrescued swimmers, keyed by their unique ID
  std::map<std::string, XYPoint> m_swimmers;
  // NEW: Map to remember the IDs of swimmers we have already rescued
  std::map<std::string, bool> m_rescued; 
  bool        m_outbound_phase; // true while we are hunting the outmost point
  std::string m_outmost_id;     // The ID of the trigger swimmer

  // Path generation
  void generateOptimizedPath(); 
};

#endif 
